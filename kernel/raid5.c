#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "defs.h"
#include "raid.h"

// global variable
extern struct RAIDMeta raidmeta;

// cluster lock must be held when calling
int
loadclusterraid5(uint64 clustern)
{
    if (raidmeta.type != RAID5)
        panic("wrong raid function called\n");

    // parity not valid - cannot load - could be removed
//    if (!raidmeta.diskinfo[DISKS - 1].valid)
//        return -1;

    if (clustern < 0 || clustern >= DISK_SIZE_BYTES / BSIZE / CLUSTER_SIZE)
        panic("Wrong cluster number in loading...");

    uchar* page = (uchar*) kalloc();
    uchar* data = page;
    uchar* parity = page + BSIZE;

    struct RAID5Data* raiddata = &raidmeta.data.raid5;

    // acquire all disk locks
    for (int i = 0; i < DISKS; i++)
        acquiresleep(&raidmeta.diskinfo[i].lock);

    uint64 startblock = clustern * CLUSTER_SIZE;
    for (int i=startblock; i<startblock + CLUSTER_SIZE; i++)
    {
        for (int i=0; i<BSIZE; i++)
            parity[i] = 0;

        uint paritydiskn = i % DISKS;

        struct DiskInfo* diskinfo = raidmeta.diskinfo;

        for (int diskn=0; diskn<DISKS; diskn++)
        {
            if (diskinfo[diskn].valid && diskn != paritydiskn)
            {
                read_block(diskinfo[diskn].diskn, i, data);
                for (int i=0; i<BSIZE; i++)
                    parity[i] ^= data[i];
            }
        }

        if (diskinfo[paritydiskn].valid)
        {
            write_block(diskinfo[paritydiskn].diskn, i, parity);
        }
    }

    raiddata->cluster_loaded[clustern] = 1;

    // release all disk locks
    for (int i = 0; i < DISKS; i++)
        releasesleep(&raidmeta.diskinfo[i].lock);

    writeraidmeta();

    kfree(page);
    return 0;
}

// read data from invalid disk, if it is the only one that is invalid
// cluster lock must be held when called
// all disk locks must be held when called
int
readinvalidraid5(int diskn, int blockn, uchar* data)
{
    uchar* newpg = (uchar*)kalloc();
    uchar* buff = newpg;
    uchar* parity = newpg + BSIZE;

    for (int i = 0; i < BSIZE; i++)
        parity[i] = 0;

//    struct RAID4Data* raiddata = &raidmeta.data.raid4;

    for (int i = 0; i < DISKS; i++)
    {
        if (i != diskn)
        {
            read_block(raidmeta.diskinfo[i].diskn, blockn, buff);
            for (int j = 0; j < BSIZE; j++)
                parity[j] ^= buff[j];
        }
    }

    for (int i = 0; i < BSIZE; i++)
        data[i] = parity[i];

    kfree(newpg);
    return 0;
}

uint64
raid5read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID5)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint64 stripe = vblkn / (DISKS - 1);
    uint64 paritydiskn = stripe % DISKS;
    uint64 stripepos = vblkn % (DISKS - 1);
    uint64 diskn = (paritydiskn + 1 + stripepos) % DISKS;

    struct DiskInfo* diskinfo = raidmeta.diskinfo;

    if (!diskinfo[diskn].valid)     // if disk is not valid, try to repair data from it
    {
        // are there more invalid disks
        for (int i = 0; i < DISKS; i++)
            if (i != diskn && !diskinfo[i].valid)       // there are more invalid disks, so it cannot be repaired
                return -1;

        // acquire every disk lock
        for (int i = 0; i < DISKS; i++)
            acquiresleep(&raidmeta.diskinfo[i].lock);

        readinvalidraid5(diskn, stripe, data);

        // release all disk locks
        for (int i = 0; i < DISKS; i++)
            releasesleep(&raidmeta.diskinfo[i].lock);
    }
    else
    {
        acquiresleep(&raidmeta.diskinfo[diskn].lock);
        read_block(diskn+1, stripe, data);
        releasesleep(&raidmeta.diskinfo[diskn].lock);
    }

    return 0;
}

uint64
raid5write(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID5)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint64 stripe = vblkn / (DISKS - 1);
    uint64 paritydiskn = stripe % DISKS;
    uint64 stripepos = vblkn % (DISKS - 1);
    uint64 diskn = (paritydiskn + 1 + stripepos) % DISKS;

    struct DiskInfo* diskinfo = raidmeta.diskinfo;

    // are there 2 or more invalid disks
    if (!diskinfo[diskn].valid)
    {
        for (int i = 0; i < DISKS; i++)
        {
            if (i != diskn && !diskinfo[i].valid)       // there are more invalid disks, so it cannot be repaired
                return -1;
        }
    }

    struct RAID5Data* raiddata = &raidmeta.data.raid5;

    uint64 clustern = stripe / CLUSTER_SIZE;

    uchar* page = (uchar*) kalloc();
    uchar* prevdata = page;
    uchar* parity = page + BSIZE;

    // REPAIR - LOCK -ADD
    acquire(&raiddata->repairlock);
    while (raiddata->repairing)
    {
        sleep(&raiddata->repairing, &raiddata->repairlock);
    }
    raiddata->writecount++;
    release(&raiddata->repairlock);

    acquiresleep(&raiddata->clusterlock);
    if (!raiddata->cluster_loaded[clustern])
    {
        loadclusterraid5(clustern);
    }
    releasesleep(&raiddata->clusterlock);

    if (!diskinfo[diskn].valid)
    {
        // acquire every disk lock
        for (int i = 0; i < DISKS; i++)
            acquiresleep(&raidmeta.diskinfo[i].lock);

        readinvalidraid5(diskn, stripe, prevdata);
        read_block(diskinfo[paritydiskn].diskn, stripe, parity);       // prob not needed

        for (int i = 0; i < BSIZE; i++)
            parity[i] ^= prevdata[i] ^ data[i];

        write_block(diskinfo[paritydiskn].diskn, stripe, parity);

        // release all disk locks
        for (int i = 0; i < DISKS; i++)
            releasesleep(&raidmeta.diskinfo[i].lock);
    }
    else if (!diskinfo[paritydiskn].valid)
    {
        acquiresleep(&raidmeta.diskinfo[diskn].lock);
        write_block(diskn + 1, stripe, data);
        releasesleep(&raidmeta.diskinfo[diskn].lock);
    }
    else
    {
        // uzimamo u rastucem poretku po broju diskova - izbegavanje deadlocka
        if (diskn < paritydiskn)
        {
            acquiresleep(&raidmeta.diskinfo[diskn].lock);
            acquiresleep(&raidmeta.diskinfo[paritydiskn].lock);
        }
        else
        {
            acquiresleep(&raidmeta.diskinfo[paritydiskn].lock);
            acquiresleep(&raidmeta.diskinfo[diskn].lock);
        }

        read_block(diskn + 1, stripe, prevdata);
        read_block(diskinfo[paritydiskn].diskn, stripe, parity);

        for (int i = 0; i < BSIZE; i++)
            parity[i] ^= prevdata[i] ^ data[i];

        write_block(diskn + 1, stripe, data);
        // write new parity
        write_block(diskinfo[paritydiskn].diskn, stripe, parity);

        if (diskn < paritydiskn)
        {
            releasesleep(&raidmeta.diskinfo[paritydiskn].lock);
            releasesleep(&raidmeta.diskinfo[diskn].lock);
        }
        else
        {
            releasesleep(&raidmeta.diskinfo[diskn].lock);
            releasesleep(&raidmeta.diskinfo[paritydiskn].lock);
        }
    }

    kfree(page);

    // REPAIR - LOCK -ADD
    acquire(&raiddata->repairlock);
    raiddata->writecount--;
    if (raiddata->writecount == 0)
    {
        wakeup(&raiddata->writecount);
    }
    release(&raiddata->repairlock);

    return 0;
}
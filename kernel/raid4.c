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
loadcluster(uint64 clustern)
{
    if (raidmeta.type != RAID4)
        panic("wrong raid function called\n");

    if (!raidmeta.diskinfo[DISKS - 1].valid)
        return -1;

    if (clustern < 0 || clustern >= DISK_SIZE_BYTES / BSIZE / CLUSTER_SIZE)
        panic("Wrong cluster number in loading...");

    uchar* page = (uchar*) kalloc();
    uchar* data = page;
    uchar* parity = page + BSIZE;

    struct RAID4Data* raiddata = &raidmeta.data.raid4;

    // acquire all disk locks
    for (int i = 0; i < DISKS; i++)
        acquiresleep(&raidmeta.diskinfo[i].lock);

    uint64 startblock = clustern * CLUSTER_SIZE;
    for (int i=startblock; i<startblock + CLUSTER_SIZE; i++)
    {
        for (int i=0; i<BSIZE; i++)
            parity[i] = 0;

        struct DiskInfo* diskinfo = raidmeta.diskinfo;

        for (int diskn=1; diskn<DISKS; diskn++)
        {
            if (diskinfo[diskn].valid)
            {
                read_block(diskn, i, data);
                for (int i=0; i<BSIZE; i++)
                    parity[i] ^= data[i];
            }
        }
        write_block(DISKS, i, parity);
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
int
readinvalid(int diskn, int blockn, uchar* data)
{
    uchar* newpg = (uchar*)kalloc();
    uchar* buff = newpg;
    uchar* parity = newpg + BSIZE;

    for (int i = 0; i < BSIZE; i++)
        parity[i] = 0;

//    struct RAID4Data* raiddata = &raidmeta.data.raid4;
    // acquire every disk lock
    for (int i = 0; i < DISKS; i++)
        acquiresleep(&raidmeta.diskinfo[i].lock);

    for (int i = 0; i < DISKS; i++)
    {
        if (i != diskn)
        {
            read_block(raidmeta.diskinfo[i].diskn, blockn, buff);
            for (int j = 0; j < BSIZE; j++)
                parity[j] ^= buff[j];
        }
    }

    // release all disk locks
    for (int i = 0; i < DISKS; i++)
        releasesleep(&raidmeta.diskinfo[i].lock);

    for (int i = 0; i < BSIZE; i++)
        data[i] = parity[i];

    kfree(newpg);
    return 0;
}

uint64
raid4read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID4)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint64 diskn = vblkn % (DISKS - 1);
    uint64 pblkn = vblkn / (DISKS - 1);

//    struct RAID4Data* raiddata = &raidmeta.data.raid4;
    struct DiskInfo* diskinfo = raidmeta.diskinfo;

    if (!diskinfo[diskn].valid)     // if disk is not valid, try to repair data from it
    {
        for (int i = 0; i < DISKS; i++)
            if (i != diskn && !diskinfo[i].valid)       // there are more invalid disks, so it cannot be repaired
                return -1;
        return readinvalid(diskn, pblkn, data);
    }

    acquiresleep(&raidmeta.diskinfo[diskn].lock);
    read_block(diskn+1, pblkn, data);
    releasesleep(&raidmeta.diskinfo[diskn].lock);

    return 0;
}

uint64
raid4write(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID4)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint64 diskn = vblkn % (DISKS - 1);
    uint64 pblkn = vblkn / (DISKS - 1);

    struct DiskInfo* diskinfo = raidmeta.diskinfo;
    if (!diskinfo[diskn].valid || !diskinfo[DISKS - 1].valid)
        return -1;

    struct RAID4Data* raiddata = &raidmeta.data.raid4;

    uint64 clustern = pblkn / CLUSTER_SIZE;

    acquiresleep(&raiddata->clusterlock);
    if (!raiddata->cluster_loaded[clustern])
        loadcluster(clustern);
    releasesleep(&raiddata->clusterlock);

    uchar* page = (uchar*) kalloc();
    uchar* prevdata = page;
    uchar* parity = page + BSIZE;

    acquiresleep(&raidmeta.diskinfo[diskn].lock);
    acquiresleep(&raidmeta.diskinfo[DISKS - 1].lock);

    read_block(diskn+1, pblkn, prevdata);
    read_block(DISKS, pblkn, parity);

    for (int i=0; i<BSIZE; i++)
        parity[i] ^= prevdata[i] ^ data[i];

    write_block(diskn+1, pblkn, data);
    // write new parity
    write_block(DISKS, pblkn, parity);

    releasesleep(&raidmeta.diskinfo[diskn].lock);
    releasesleep(&raidmeta.diskinfo[DISKS - 1].lock);

    kfree(page);

    writeraidmeta();

    return 0;
}
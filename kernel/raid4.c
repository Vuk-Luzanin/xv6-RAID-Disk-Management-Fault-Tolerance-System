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

void
loadcluster(uint64 clustern, struct RAID4Data* raiddata)
{
    if (raidmeta.type != RAID4)
        panic("wrong raid function called\n");

    if (clustern < 0 || clustern >= DISK_SIZE_BYTES / BSIZE / CLUSTER_SIZE)
        panic("Wrong cluster number in loading...");

    uchar* page = (uchar*) kalloc();
    uchar* data = page;
    uchar* parity = page + BSIZE;
    for (int i=0; i<BSIZE; i++)
        parity[i] = 0;

    uint64 startblock = clustern * CLUSTER_SIZE;

    for (int i=0; i<CLUSTER_SIZE; i++)
    {
        for (int diskn=1; diskn<DISKS; diskn++)
        {
            read_block(diskn, startblock + i, data);
            for (int i=0; i<BSIZE; i++)
                parity[i] ^= data[i];
        }
        write_block(DISKS, startblock + i, parity);
    }

    raiddata->cluster_loaded[clustern] = 1;

    writeraidmeta();

    kfree(page);
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

//    acquiresleep(&raiddata->lock[diskn]);
    read_block(diskn+1, pblkn, data);
//    releasesleep(&raiddata->lock[diskn]);

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

    struct RAID4Data* raiddata = &raidmeta.data.raid4;

    uint64 clustern = pblkn / CLUSTER_SIZE;

    if (!raiddata->cluster_loaded[clustern])
        loadcluster(clustern, raiddata);

    uchar* page = (uchar*) kalloc();
    uchar* prevdata = page;
    uchar* parity = page + BSIZE;

    read_block(diskn+1, pblkn, prevdata);
    read_block(DISKS, pblkn, parity);

    for (int i=0; i<BSIZE; i++)
        parity[i] ^= prevdata[i] ^ data[i];

    write_block(diskn+1, pblkn, data);
    // write new parity
    write_block(DISKS, pblkn, parity);

    kfree(page);

    return 0;
}
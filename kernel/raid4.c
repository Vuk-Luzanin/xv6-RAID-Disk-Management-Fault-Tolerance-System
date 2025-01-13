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


// NIJE PROVERENO
uint64
raid4read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID4)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint64 diskn = vblkn % (DISKS - 1);
    uint64 pblkn = vblkn / (DISKS - 1);

    struct RAID4Data* raiddata = &raidmeta.data.raid4;

    acquiresleep(&raiddata->mutex);
    read_block(diskn, pblkn, data);
    releasesleep(&raiddata->mutex);

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

    acquiresleep(&raiddata->mutex);
    uchar prevdata[BSIZE], parity[BSIZE];
    read_block(diskn, pblkn, prevdata);
    read_block(DISKS - 1, pblkn, parity);

    for (int i=0; i<BSIZE; i++)
        parity[i] ^= prevdata[i] ^ data[i];

    write_block(diskn, pblkn, data);
    releasesleep(&raiddata->mutex);

    return 0;
}
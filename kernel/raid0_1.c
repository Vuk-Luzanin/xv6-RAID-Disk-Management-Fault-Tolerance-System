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

uint64          readdiskpair(struct DiskPair* diskpair, int pblkn, uchar* data);
uint64          writediskpair(struct DiskPair* diskpair, int pblkn, uchar* data);


uint64
raid0_1read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID0_1)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    // effectively DISKS / 2 to use
    uint pairn = vblkn % (DISKS / 2);
    uint pblkn = vblkn / (DISKS / 2);

    struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;
    struct DiskPair* diskpair = &raiddata->diskpair[pairn];

    return readdiskpair(diskpair, pblkn, data);
}

uint64
raid0_1write(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID0_1)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint pairn = vblkn % (DISKS / 2);
    uint pblkn = vblkn / (DISKS / 2);

    struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;
    struct DiskPair* diskpair = &raiddata->diskpair[pairn];

    return writediskpair(diskpair, pblkn, data);
}
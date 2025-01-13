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
raid1read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID1)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    int pairNum = vblkn / diskblockn();
    int pblkn = vblkn % diskblockn();

    struct RAID1Data* raiddata = &raidmeta.data.raid1;
    struct DiskPair* diskpair = &raiddata->diskpair[pairNum];

    return readdiskpair(diskpair, pblkn, data);
}

uint64
raid1write(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID1)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    int pairNum = vblkn / diskblockn();
    int pblkn = vblkn % diskblockn();

    struct RAID1Data* raiddata = &raidmeta.data.raid1;
    struct DiskPair* diskpair = &raiddata->diskpair[pairNum];

    return writediskpair(diskpair, pblkn, data);
}
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


uint64
raid0read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID0)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
    {
        printf("OVDE: %d", vblkn);
        return -1;
    }

    uint diskn = vblkn % DISKS;
    uint pblkn = vblkn / DISKS;


//    struct RAID0Data* raiddata = &raidmeta.data.raid0;

    struct DiskInfo* diskInfo = &raidmeta.diskinfo[diskn];

    if (!diskInfo->valid)
        return -1;

    acquiresleep(&diskInfo->lock);
    read_block(diskInfo->diskn, pblkn, (uchar*)data);
    releasesleep(&diskInfo->lock);
    return 0;
}

uint64
raid0write(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID0)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint diskn = vblkn % DISKS;
    uint pblkn = vblkn / DISKS;

//    struct RAID0Data* raiddata = &raidmeta.data.raid0;

    struct DiskInfo* diskInfo = &raidmeta.diskinfo[diskn];

    if (!diskInfo->valid)
        return -1;

    acquiresleep(&diskInfo->lock);
    write_block(diskInfo->diskn, pblkn, (uchar*)data);
    releasesleep(&diskInfo->lock);

    return 0;
}
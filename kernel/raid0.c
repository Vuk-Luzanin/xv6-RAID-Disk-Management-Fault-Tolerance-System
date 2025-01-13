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
        return -1;

    uint diskn = vblkn % DISKS;
    uint pblkn = vblkn / DISKS;

    struct RAID0Data* raiddata = &raidmeta.data.raid0;

    struct DiskInfo* disk = &raidmeta.diskinfo[diskn];

    if (!disk->valid)
        return -1;

    acquiresleep(&raiddata->lock[diskn]);
    read_block(disk->diskn, pblkn, (uchar*)data);
    releasesleep(&raiddata->lock[diskn]);
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

    struct RAID0Data* raiddata = &raidmeta.data.raid0;

    struct DiskInfo* disk = &raidmeta.diskinfo[diskn];

    if (!disk->valid)
        return -1;

    acquiresleep(&raiddata->lock[diskn]);
    write_block(disk->diskn, pblkn, (uchar*)data);
    releasesleep(&raiddata->lock[diskn]);

    return 0;
}
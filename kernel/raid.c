#include "types.h"
#include "fs.h"

uint
raid_blockn(void)
{
    return (RAID_DISK_SIZE / BSIZE - 1) * DISKS;
}
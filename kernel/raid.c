#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "raid.h"
#include "defs.h"

uint64 raid0read(int vblkn, uchar* data);
uint64 raid1read(int vblkn, uchar* data);
uint64 raid0_1read(int vblkn, uchar* data);

uint64 raid0write(int vblkn, uchar* data);
uint64 raid1write(int vblkn, uchar* data);
uint64 raid0_1write(int vblkn, uchar* data);

// virtual function table
uint64 (*readtable[])(int vblkn, uchar* data) =
{
        [RAID0] = raid0read,            // on index 0 is raid0read...
        [RAID1] = raid1read,
        [RAID0_1] = raid0_1read
};

uint64 (*writetable[])(int vblkn, uchar* data) =
{
        [RAID0] = raid0write,
        [RAID1] = raid1write,
        [RAID0_1] = raid0_1write
};

// global variable
struct
{
    enum RAID_TYPE type;

    struct DiskInfo diskinfo[DISKS + 1];

    union
    {
        struct RAID0Data raid0;
        struct RAID1Data raid1;
        struct RAID0_1Data raid0_1;
    } data;

    // virtual "methods" for each type
    uint64 (*read)(int vblkn, uchar* data);
    uint64 (*write)(int vblkn, uchar* data);

} raidmeta;

// DISK_SIZE_BYTES - in bytes
// BSIZE - size of block in bytes
// number of free blocks on disk -> -1 because it counts from 0
uint64
diskblockn()
{
    return DISK_SIZE_BYTES / BSIZE - 1;
}


// number of free blocks for every RAID type
uint64
raidblockn(void)
{
    switch (raidmeta.type)
    {
        case RAID0:
            return diskblockn() * DISKS;
        case RAID1:
            return diskblockn() * ((DISKS+1) / 2);      //when odd number of disks -> one is not mirrored
        case RAID0_1:
            return diskblockn() * (DISKS / 2);
        default:
            panic("bad raid type\n");
    }
}

// initialize raid structure when booting
void
loadraid(void)
{
    uchar data[BSIZE];
    int lastblockondisk = diskblockn();
    read_block(1, lastblockondisk, data);
    // extract raidmeta structure from block
    memmove(&raidmeta, data, sizeof(raidmeta));

    // set disk number and valid = 1
    for (int i = 0; i < DISKS; i++)
    {
        // disks will have diskn -> [1, 8]
        raidmeta.diskinfo[i].diskn = i + 1;         // + 1 because we cannot access disk 0
        raidmeta.diskinfo[i].valid = 1;
    }
    raidmeta.diskinfo[DISKS].valid = 0;

    if (raidmeta.type >= RAID0 && raidmeta.type <= RAID5)
    {
        raidmeta.read = readtable[raidmeta.type];
        raidmeta.write = writetable[raidmeta.type];
    }
    else
        raidmeta.read = raidmeta.write = 0;
}

uint64
setraidtype(int type)
{
    if (type < RAID0 || type > RAID5)
        panic("invalid raid type");

    raidmeta.type = type;
    raidmeta.read = readtable[type];
    raidmeta.write = writetable[type];

    if (type == RAID0)
    {
        struct RAID0Data* raiddata = &raidmeta.data.raid0;
        for (int i = 0; i < DISKS; i++)
            initsleeplock(&raiddata->lock[i], "raidlock");
    }
    else if (type == RAID1)
    {
        struct RAID1Data* raiddata = &raidmeta.data.raid1;
        for (int i = 0; i < (DISKS + 1) / 2; i++)
        {
            raiddata->diskpair[i].disk[0] = raidmeta.diskinfo + i * 2;
            raiddata->diskpair[i].disk[1] = raidmeta.diskinfo + i * 2 + 1;
            initlock(&raiddata->diskpair[i].mutex, "raidlock");
            raiddata->diskpair[i].writing = 0;
            for (int j = 0; j < 2; j++)
                raiddata->diskpair[i].reading[j] = 0;
        }
    }
    else if (type == RAID0_1)
    {
        struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;
        for (int i = 0; i < DISKS / 2; i++)
        {
            raiddata->diskpair[i].disk[0] = raidmeta.diskinfo + i;
            raiddata->diskpair[i].disk[1] = raidmeta.diskinfo + i + DISKS / 2;
        }
    }
    else if (type == RAID4)
    {
//    for (int i = 0; i < DISKS; i++)
// if (raidmeta.diskinfo[i].valid == 0)
//           return -1;
//
//        struct RAID4Data* raiddata = &raidmeta.data.raid4;
// raiddata->initialized = 0;
// initraid4(raiddata, 100);
//        initsleeplock(&raiddata->mutex, "raidara4");
    }

    // write strusture on last block on every disk
    int lastblockondisk = diskblockn();
    uchar data[BSIZE] = {0};
    memmove(data, &raidmeta, sizeof(raidmeta));
    for (int i = 1; i <= DISKS; i++)
        write_block(i, lastblockondisk, data);

    return 0;
}

uint64
readdiskpair(struct DiskPair* diskpair, int pblkn, uchar* data)
{
    int diskn = -1, readfrom = -1;
    while (1)
    {
        acquire(&diskpair->mutex);

        if (diskpair->disk[0]->valid == 0 && diskpair->disk[1]->valid == 0)
        {
            release(&diskpair->mutex);
            return -1;
        }

        if (diskpair->writing) {
            release(&diskpair->mutex);
            continue;
        }

        for (int i = 0; i < 2; i++)
            if ((!diskpair->reading[i]) && diskpair->disk[i]->valid) {
                diskpair->reading[i] = 1;
                release(&diskpair->mutex);
                diskn = diskpair->disk[i]->diskn;
                readfrom = i;
                goto readdiskpairloopend;
            }

        release(&diskpair->mutex);
    }

    readdiskpairloopend:
    if (diskn == -1 || readfrom == -1)
        panic("raid1read");

    read_block(diskn, pblkn, data);

    acquire(&diskpair->mutex);
    diskpair->reading[readfrom] = 0;
    release(&diskpair->mutex);

    return 0;
}

uint64
writediskpair(struct DiskPair* diskpair, int pblkn, uchar* data)
{

    while (1)
    {
        acquire(&diskpair->mutex);

        if (diskpair->disk[0]->valid == 0 && diskpair->disk[1]->valid == 0)
        {
            release(&diskpair->mutex);
            return -1;
        }


        if (diskpair->writing) {
            release(&diskpair->mutex);
            continue;
        }

        if (diskpair->reading[0] == 1 || diskpair->reading[1] == 1) {
            release(&diskpair->mutex);
            continue;
        }

        diskpair->writing = 1;
        release(&diskpair->mutex);
        break;
    }

    for (int i = 0; i < 2; i++)
        if (diskpair->disk[i]->valid)
            write_block(diskpair->disk[i]->diskn, pblkn, (uchar*)data);

    acquire(&diskpair->mutex);
    diskpair->writing = 0;
    release(&diskpair->mutex);

    return 0;
}

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
    struct DiskPair* diskpair = raiddata->diskpair + pairNum;

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
    struct DiskPair* diskpair = raiddata->diskpair + pairNum;

    return writediskpair(diskpair, pblkn, data);
}

uint64
raid0_1read(int vblkn, uchar* data)
{
    if (raidmeta.type != RAID0_1)
        panic("wrong raid function called\n");

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    uint pairn = vblkn % (DISKS / 2);
    uint pblkn = vblkn / (DISKS / 2);

    struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;
    struct DiskPair* diskpair = raiddata->diskpair + pairn;

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
    struct DiskPair* diskpair = raiddata->diskpair + pairn;

    return writediskpair(diskpair, pblkn, data);
}

// stub for virtual function
uint64
readraid(int vblkn, uchar* data)
{
    if (raidmeta.read)
        return (*raidmeta.read)(vblkn, data);
    return -1;
}

// stub for virtual function
uint64
writeraid(int vblkn, uchar* data)
{
    if (raidmeta.write)
        return (*raidmeta.write)(vblkn, data);
    return -1;
}

uint64
raidfail(int diskn)         // cannot fail disk 0
{
    // diskn is [1-8]
    diskn--;
    // diskn is [0-7]
    if (diskn < 0 || diskn >= DISKS)
        return -1;

    raidmeta.diskinfo[diskn].valid = 0;

    return 0;
}


uint64
raidrepair(int diskn)
{
    if (raidmeta.type < RAID0 || raidmeta.type >= RAID5)
        panic("raid not initialized\n");

    // diskn is [1-8]
    diskn--;
    // diskn is [0-7]
    if (diskn < 0 || diskn >= DISKS)
        return -1;

    if (raidmeta.diskinfo[diskn].valid)
        return 0;

    if (raidmeta.type == RAID0)
        return -1;
    else if (raidmeta.type == RAID1)
    {
        struct RAID1Data* raiddata = &raidmeta.data.raid1;
        struct DiskPair* diskpair = raiddata->diskpair + diskn / 2;
        struct DiskInfo* pair = diskpair->disk[1 - diskn % 2];
        if (!pair->valid)
            return -1;

        while (1)
        {
            acquire(&diskpair->mutex);

            if (diskpair->writing || diskpair->reading[0] || diskpair->reading[1])
            {
                release(&diskpair->mutex);
                continue;
            }

            diskpair->writing = 1;
            release(&diskpair->mutex);
            break;
        }

        for (int i = 0; i <= diskblockn(); i++)
        {
            uchar data[BSIZE];
            read_block(pair->diskn, i, data);
            write_block(raidmeta.diskinfo[diskn].diskn, i, data);
        }

        raidmeta.diskinfo[diskn].valid = 1;
        acquire(&diskpair->mutex);
        diskpair->writing = 0;
        release(&diskpair->mutex);

        return 0;
    }
    else if (raidmeta.type == RAID0_1)
    {
        struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;

        struct DiskPair* diskpair = raiddata->diskpair + (diskn % (DISKS / 2));
        struct DiskInfo* pair = diskpair->disk[1 - diskn / (DISKS / 2)];

        if (!pair->valid)
            return -1;

        while (1)
        {
            acquire(&diskpair->mutex);

            if (diskpair->writing || diskpair->reading[0] || diskpair->reading[1])
            {
                release(&diskpair->mutex);
                continue;
            }

            diskpair->writing = 1;
            release(&diskpair->mutex);
            break;
        }

        for (int i = 0; i <= diskblockn(); i++)
        {
            uchar data[BSIZE];
            read_block(pair->diskn, i, data);
            write_block(raidmeta.diskinfo[diskn].diskn, i, data);
        }

        raidmeta.diskinfo[diskn].valid = 1;
        acquire(&diskpair->mutex);
        diskpair->writing = 0;
        release(&diskpair->mutex);

        return 0;
    }

    return 0;
}

uint64
raiddestroy(void)
{
    raidmeta.type = -1;

    int lastblockondisk = diskblockn();
    uchar data[BSIZE] = {0};
    memmove(data, &raidmeta, sizeof(raidmeta));
    for (int i = 1; i <= DISKS; i++)
        write_block(i, lastblockondisk, data);
    return 0;
}
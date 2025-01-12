#ifndef RAID_H
#define RAID_H

enum RAID_TYPE {RAID0, RAID1, RAID0_1, RAID4, RAID5};

struct DiskInfo
{
    uint8 valid;
    uint8 diskn;        // number from 1-8
};

struct DiskPair
{
    struct spinlock mutex;              // only for changing conditions
    struct DiskInfo* disk[2];           // 2 disks in pair
    uint8 writing;                      // condition
    uint8 reading[2];                   // condition
};

struct RAID0Data
{
    struct sleeplock lock[DISKS];
};

struct RAID1Data
{
    struct DiskPair diskpair[(DISKS + 1) / 2];
};

struct RAID0_1Data
{
    struct DiskPair diskpair[DISKS / 2];
};

struct RAID4Data
{
    struct sleeplock mutex;
};

#endif //RAID_H
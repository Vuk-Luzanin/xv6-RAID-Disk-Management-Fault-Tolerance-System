#ifndef RAID_H
#define RAID_H

enum RAID_TYPE {RAID0, RAID1, RAID0_1, RAID4, RAID5};

struct DiskInfo
{
    uint8 valid;
    uint8 diskn;        // number from [1-8]
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

//uint64 CLUSTER_SIZE = 1024;

struct RAID4Data
{
    struct sleeplock mutex;
//    int initialized;
    //uint8 stripe_loaded[DISKS_SIZE / BSIZE / CLUSTER_SIZE];         // has initialized flag for cluster
};

extern uint64 (*readtable[])(int, uchar*);
extern uint64 (*writetable[])(int, uchar*);

struct RAIDMeta
{
    enum RAID_TYPE type;
    struct DiskInfo diskinfo[DISKS + 1];

    union
    {
        struct RAID0Data raid0;
        struct RAID1Data raid1;
        struct RAID0_1Data raid0_1;
        struct RAID4Data raid4;
    } data;

    // virtual "methods" for each type
    uint64 (*read)(int vblkn, uchar* data);
    uint64 (*write)(int vblkn, uchar* data);
};

#endif //RAID_H
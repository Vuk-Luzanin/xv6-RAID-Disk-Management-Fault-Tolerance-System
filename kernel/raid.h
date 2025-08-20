#ifndef RAID_H
#define RAID_H

enum RAID_TYPE {RAID0, RAID1, RAID0_1, RAID4, RAID5};

struct DiskInfo
{
    uint8 valid;
    uint8 diskn;        // number from [1-8]
    struct sleeplock lock;      // disk lock - mutex, first acquire raid locks, and then this disk lock
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
//    struct sleeplock lock[DISKS];       // lock per disk -> moved to diskinfo
};

struct RAID1Data
{
    struct DiskPair diskpair[(DISKS + 1) / 2];      // lock per pair
};

struct RAID0_1Data
{
    struct DiskPair diskpair[DISKS / 2];            // lock per pair
};

// number of blocks in one cluster
#define CLUSTER_SIZE 128

struct RAID4Data
{
//    struct sleeplock lock[DISKS];                                           // lock per disk -> moved to diskinfo
    uint8 cluster_loaded[DISK_SIZE_BYTES / BSIZE / CLUSTER_SIZE];           // has been initialized flag for cluster (parity disk is set or not) -> for lazy loading
    struct sleeplock clusterlock;                                           // lock for cluster_loaded array

    // added
    struct spinlock repairlock;
    int writecount;
    int repairing;
};

//
struct RAID5Data
{
//    struct sleeplock lock[DISKS];                                           // lock per disk -> moved to diskinfo
    uint8 cluster_loaded[DISK_SIZE_BYTES / BSIZE / CLUSTER_SIZE];           // has been initialized flag for cluster (parity disk is set or not) -> for lazy loading
    struct sleeplock clusterlock;                                           // lock for cluster_loaded array
};

extern uint64 (*readtable[])(int, uchar*);
extern uint64 (*writetable[])(int, uchar*);

struct RAIDMeta
{
    enum RAID_TYPE type;
    struct DiskInfo diskinfo[DISKS + 1];
//    int valid;

    //struct spinlock dirty;
    //int maxdirty; //struct spinlock dirty;
    //int maxdirty;

    union
    {
        struct RAID0Data raid0;
        struct RAID1Data raid1;
        struct RAID0_1Data raid0_1;
        struct RAID4Data raid4;
        struct RAID5Data raid5;
    } data;

    // virtual "methods" for each type
    uint64 (*read)(int vblkn, uchar* data);
    uint64 (*write)(int vblkn, uchar* data);
};



#endif //RAID_H
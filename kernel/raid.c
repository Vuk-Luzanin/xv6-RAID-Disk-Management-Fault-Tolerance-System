#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "defs.h"
#include "raid.h"

uint64 raid0read(int vblkn, uchar* data);
uint64 raid1read(int vblkn, uchar* data);
uint64 raid0_1read(int vblkn, uchar* data);
uint64 raid4read(int vblkn, uchar* data);

uint64 raid0write(int vblkn, uchar* data);
uint64 raid1write(int vblkn, uchar* data);
uint64 raid0_1write(int vblkn, uchar* data);
uint64 raid4write(int vblkn, uchar* data);

// virtual function table
uint64 (*readtable[])(int vblkn, uchar* data) =
{
        [RAID0] = raid0read,            // on index 0 is raid0read...
        [RAID1] = raid1read,
        [RAID0_1] = raid0_1read,
        [RAID4] = raid4read
};

uint64 (*writetable[])(int vblkn, uchar* data) =
{
        [RAID0] = raid0write,
        [RAID1] = raid1write,
        [RAID0_1] = raid0_1write,
        [RAID4] = raid4write
};

// global variable
struct RAIDMeta raidmeta;

// old impl.
//void
//writeraidmeta()
//{
//    //printf("cuva raidmeta\n");
//    // write structure on last block on every disk
//    int lastblockondisk = diskblockn();
//    uchar data[BSIZE] = {0};
//    memmove(data, &raidmeta, sizeof(raidmeta));
//    for (int i = 1; i <= DISKS; i++)
//        write_block(i, lastblockondisk, data);
//}

// new impl -> need to be checked
void
writeraidmeta()
{
    // acquire all disk locks
    for (int i = 0; i < DISKS; i++)
        acquiresleep(&raidmeta.diskinfo[i].lock);

    //printf("cuva raidmeta\n");
    // write structure on last block on every disk
    for (int i = 1; i <= DISKS; i++)
    {
        struct DiskInfo* diskInfo = raidmeta.diskinfo + i;
        if (diskInfo->valid)
        {
            uchar data[BSIZE] = {0};
            memmove(data, &raidmeta, sizeof(raidmeta));
            int lastblockondisk = diskblockn();
            write_block(i, lastblockondisk, data);
        }
    }

    // release all disk locks
    for (int i = 0; i < DISKS; i++)
        releasesleep(&raidmeta.diskinfo[i].lock);
}



// DISK_SIZE_BYTES - in bytes
// BSIZE - size of block in bytes
// returns last free block on disk -> -1 because it counts from 0
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
            return diskblockn() * ((DISKS+1) / 2);      //when odd number of disks -> one is not mirrored, but used for efficiency
        case RAID0_1:
            return diskblockn() * (DISKS / 2);
        case RAID4:
            return diskblockn() * (DISKS - 1);
        case RAID5:
            return diskblockn() * (DISKS - 1);
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

    if (data[BSIZE-1] == 255)
    {
        panic("RAID structure was destroyed\n");
        exit(0);
    }

    // ako je prethodno vec inicijalizovan raidmeta -> u tom bloku ce biti bar neki bajt != 0
    volatile int prevState = 0;
    for (int i=0; i<BSIZE; i++)
    {
        if (data[i] != 0)
        {
            prevState = 1;
            break;
        }
    }

    // extract raidmeta structure from block - must read in every case
    memmove(&raidmeta, data, sizeof(raidmeta));

    if (prevState == 1)
    {
//        printf("MAXDIRTY je sacuvan: %d\n", raidmeta.maxdirty);
        return;                 // already initialized raidmeta in previous run, just return
    }

    // set disk number and valid = 1 -> not already initialized
    for (int i = 0; i < DISKS; i++)
    {
        // disks will have diskn -> [1, 8]
        raidmeta.diskinfo[i].diskn = i + 1;         // + 1 because we cannot access disk 0
        raidmeta.diskinfo[i].valid = 1;

        // initialize lock per disk
        initsleeplock(&raidmeta.diskinfo[i].lock, "diskinfolock");
    }
    raidmeta.diskinfo[DISKS].valid = 0;


    //initlock(&raidmeta.dirty, "raidmetadirty");
    //raidmeta.maxdirty = -1;

    if (raidmeta.type >= RAID0 && raidmeta.type <= RAID5)
    {
        raidmeta.read = readtable[raidmeta.type];
        raidmeta.write = writetable[raidmeta.type];
    }
    else
    {
        raidmeta.read = raidmeta.write = 0;
    }
    writeraidmeta();
    //printf("MAXDIRTY je resetovan: %d\n", raidmeta.maxdirty);
}

uint64
setraidtype(int type)
{
    if (type < RAID0 || type > RAID5)
        panic("invalid raid type");

    raidmeta.type = type;
    raidmeta.read = readtable[type];
    raidmeta.write = writetable[type];

    switch (type) {
        case RAID0:
        {
            break;
        }
        case RAID1:
        {
            struct RAID1Data *raiddata = &raidmeta.data.raid1;
            for (int i = 0; i < (DISKS + 1) / 2; i++) {
                raiddata->diskpair[i].disk[0] = &raidmeta.diskinfo[i * 2];
                raiddata->diskpair[i].disk[1] = &raidmeta.diskinfo[i * 2 + 1];      // because of this is diskinfo[DISKS + 1];
                initlock(&raiddata->diskpair[i].mutex, "raidlock");
                raiddata->diskpair[i].writing = 0;
                for (int j = 0; j < 2; j++)
                    raiddata->diskpair[i].reading[j] = 0;
            }
            break;
        }
        case RAID0_1:
        {
            struct RAID0_1Data *raiddata = &raidmeta.data.raid0_1;
            for (int i = 0; i < DISKS / 2; i++)                         // even number of disks is used
            {
                raiddata->diskpair[i].disk[0] = &raidmeta.diskinfo[i];
                raiddata->diskpair[i].disk[1] = &raidmeta.diskinfo[i + DISKS / 2];
                initlock(&raiddata->diskpair[i].mutex, "raidlock");
                raiddata->diskpair[i].writing = 0;
                for (int j = 0; j < 2; j++)
                    raiddata->diskpair[i].reading[j] = 0;
            }
            break;
        }
        case RAID4:
        {
            if (raidmeta.diskinfo[DISKS - 1].valid == 0)              // if parity is not valid
                return -1;

            struct RAID4Data* raiddata = &raidmeta.data.raid4;

//            for (int i=0; i<DISKS; i++)
//                initsleeplock(&raiddata->lock[i], "disklock");
            initsleeplock(&raiddata->clusterlock, "clusterlock");

            //int maxdirtycluster = raidmeta.maxdirty > 0 ? raidmeta.maxdirty / CLUSTER_SIZE : raidmeta.maxdirty;
            //printf("Max dirty cluster: %d\n", maxdirtycluster);

            //for (int i=0; i<=maxdirtycluster; i++)          // must be reinitialized
            //    raiddata->cluster_loaded[i] = 0;

            //for (int i=maxdirtycluster+1; i<NELEM(raiddata->cluster_loaded); i++)
            //    raiddata->cluster_loaded[i] = 1;

            for (int i=0; i<NELEM(raiddata->cluster_loaded); i++)
                raiddata->cluster_loaded[i] = 0;

            break;
        }
    }

    writeraidmeta();

    return 0;
}

// multiple readers, single writer
uint64
readdiskpair(struct DiskPair* diskpair, int pblkn, uchar* data)
{
    int diskn = -1, readfromPair = -1;

    // sleep wait on spinlock
    acquire(&diskpair->mutex);

    while (1)
    {
        // both invalid
        if (diskpair->disk[0]->valid == 0 && diskpair->disk[1]->valid == 0)
        {
            release(&diskpair->mutex);
            return -1;
        }

        if (diskpair->writing)
        {
            sleep(&diskpair->mutex, &diskpair->mutex);          // 1. ARG - channel for sleeping (when waking up -> all channel is waking up - can be any number), second is spinlock
            continue;           // busy wait, try to acquire again
        }

        for (int i = 0; i < 2; i++)
            if (!diskpair->reading[i] && diskpair->disk[i]->valid) {
                diskpair->reading[i] = 1;
                release(&diskpair->mutex);
                diskn = diskpair->disk[i]->diskn;
                readfromPair = i;       // to reset reading later
                goto readdiskpairloopend;
            }
        sleep(&diskpair->mutex, &diskpair->mutex);
    }

    readdiskpairloopend:
    if (diskn == -1 || readfromPair == -1)
        panic("raid1read");

    // acquire disk locks
    acquiresleep(&diskpair->disk[readfromPair]->lock);
    read_block(diskn, pblkn, data);
    releasesleep(&diskpair->disk[readfromPair]->lock);

    acquire(&diskpair->mutex);
    diskpair->reading[readfromPair] = 0;
    release(&diskpair->mutex);
    wakeup(&diskpair->mutex);       // arg is channel

    return 0;
}

uint64
writediskpair(struct DiskPair* diskpair, int pblkn, uchar* data)
{
    acquire(&diskpair->mutex);          //must be outside of loop ()

    while (1)
    {
        if (diskpair->disk[0]->valid == 0 && diskpair->disk[1]->valid == 0)
        {
            release(&diskpair->mutex);
            return -1;
        }

        if (diskpair->writing)
        {
            sleep(&diskpair->mutex, &diskpair->mutex);          // 1. ARG - channel for sleeping (when waking up -> all channel is waking up - can be any number), second is spinlock
            continue;
        }

        if (diskpair->reading[0] == 1 || diskpair->reading[1] == 1)
        {
            sleep(&diskpair->mutex, &diskpair->mutex);          // 1. ARG - channel for sleeping (when waking up -> all channel is waking up - can be any number), second is spinlock
            continue;
        }

        diskpair->writing = 1;
        release(&diskpair->mutex);
        break;
    }

    // acquire disk locks
    for (int i=0; i<2; i++)
        acquiresleep(&diskpair->disk[i]->lock);

    // write in both parts of mirror if valid
    for (int i = 0; i < 2; i++)
        if (diskpair->disk[i]->valid)
            write_block(diskpair->disk[i]->diskn, pblkn, (uchar*)data);

    for (int i=0; i<2; i++)
        releasesleep(&diskpair->disk[i]->lock);

    acquire(&diskpair->mutex);
    diskpair->writing = 0;
    release(&diskpair->mutex);
    wakeup(&diskpair->mutex);       // arg is channel

    return 0;
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

    writeraidmeta();

    return 0;
}


// TO-DO -> write raidmeta on disk
uint64
raidrepair(int diskn)
{
    if (raidmeta.type < RAID0 || raidmeta.type > RAID5)
        panic("raid not initialized\n");

    // diskn is [1-8]
    diskn--;
    // diskn is [0-7]
    if (diskn < 0 || diskn >= DISKS)
        return -1;

    if (raidmeta.diskinfo[diskn].valid)
        return 0;

    switch (raidmeta.type)
    {
        case RAID0:
        {
            return -1;
        }
        case RAID1:
        {
            struct RAID1Data* raiddata = &raidmeta.data.raid1;
            struct DiskPair* diskpair = &raiddata->diskpair[diskn / 2];
            struct DiskInfo* pair = diskpair->disk[1 - diskn % 2];
            if (!pair->valid)
                return -1;

            acquire(&diskpair->mutex);

            while (1)
            {
                if (diskpair->writing || diskpair->reading[0] || diskpair->reading[1])
                {
                    sleep(&diskpair->mutex, &diskpair->mutex);
                    continue;
                }

                diskpair->writing = 1;
                release(&diskpair->mutex);
                break;
            }

            // acquire disk locks
            for (int i=0; i<2; i++)
                acquiresleep(&diskpair->disk[i]->lock);

            // WRITE EVERY BLOCK ON DISK FROM PAIR
            for (int i = 0; i <= diskblockn(); i++)
            {
                uchar data[BSIZE];
                read_block(pair->diskn, i, data);
                write_block(raidmeta.diskinfo[diskn].diskn, i, data);
            }

            // release disk locks
            for (int i=0; i<2; i++)
                releasesleep(&diskpair->disk[i]->lock);

            raidmeta.diskinfo[diskn].valid = 1;
            acquire(&diskpair->mutex);
            diskpair->writing = 0;
            release(&diskpair->mutex);
            wakeup(&diskpair->mutex);

            break;
        }
        case RAID0_1:
        {
            struct RAID0_1Data* raiddata = &raidmeta.data.raid0_1;
            struct DiskPair* diskpair = &raiddata->diskpair[diskn % (DISKS / 2)];
            struct DiskInfo* pair = diskpair->disk[1 - diskn / (DISKS / 2)];

            if (!pair->valid)
                return -1;

            acquire(&diskpair->mutex);

            while (1)
            {
                if (diskpair->writing || diskpair->reading[0] || diskpair->reading[1])
                {
                    sleep(&diskpair->mutex, &diskpair->mutex);
                    continue;
                }

                diskpair->writing = 1;
                release(&diskpair->mutex);
                break;
            }

            // acquire disk locks
            for (int i=0; i<2; i++)
                acquiresleep(&diskpair->disk[i]->lock);

            for (int i = 0; i <= diskblockn(); i++)
            {
                uchar data[BSIZE];
                read_block(pair->diskn, i, data);
                write_block(raidmeta.diskinfo[diskn].diskn, i, data);
            }

            // release disk locks
            for (int i=0; i<2; i++)
                releasesleep(&diskpair->disk[i]->lock);

            raidmeta.diskinfo[diskn].valid = 1;
            acquire(&diskpair->mutex);
            diskpair->writing = 0;
            release(&diskpair->mutex);
            wakeup(&diskpair->mutex);

            break;
        }
        case RAID4:
        {
            // more disks are not valid -> could not be fixed
            for (int i=0; i<DISKS; i++)
                if (i != diskn && !raidmeta.diskinfo[i].valid)
                    return -1;

            // similar to readinvalid func
            uchar* newpg = (uchar*)kalloc();
            uchar* buff = newpg;
            uchar* parity = newpg + BSIZE;

//            struct RAID4Data* raiddata = &raidmeta.data.raid4;
            // acquire every disk lock
            for (int i = 0; i < DISKS; i++)
                acquiresleep(&raidmeta.diskinfo[i].lock);

            for (int b = 0; b <= diskblockn(); b++)
            {
                // if cluster has not been loaded before, no need for repair
                struct RAID4Data* raiddata = &raidmeta.data.raid4;
                uint64 clustern = b / CLUSTER_SIZE;

                acquiresleep(&raiddata->clusterlock);
                if (!raiddata->cluster_loaded[clustern])
                {
                    releasesleep(&raiddata->clusterlock);
                    continue;
                }
                releasesleep(&raiddata->clusterlock);

                // set parity to be 0
                for (int i = 0; i < BSIZE; i++)
                    parity[i] = 0;

                // repaired value is in parity - find it first
                for (int i = 0; i < DISKS; i++)
                {
                    if (i != diskn)
                    {
                        read_block(raidmeta.diskinfo[i].diskn, b, buff);
                        for (int j = 0; j < BSIZE; j++)
                            parity[j] ^= buff[j];
                    }
                }

                // write correct value on disk
                write_block(raidmeta.diskinfo[diskn].diskn, b, parity);
            }

            raidmeta.diskinfo[diskn].valid = 1;

//          release all disk locks
            for (int i = 0; i < DISKS; i++)
                releasesleep(&raidmeta.diskinfo[i].lock);

            kfree(newpg);

            break;
        }

        default:
        {}
    }
    writeraidmeta();
    return 0;
}

uint64
raiddestroy(void)
{
    //raidmeta.type = -1;

    int lastblockondisk = diskblockn();
    uchar data[BSIZE];                  // write all 1 on last block on every disk
    for (int i = 0; i < BSIZE; i++)
        data[i] = 255;
    memmove(data, &raidmeta, sizeof(raidmeta));
    for (int i = 1; i <= DISKS; i++)
        write_block(i, lastblockondisk, data);
    return 0;
}
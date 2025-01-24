#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void check_data(uint blocks, uchar blk[BSIZE], uchar blk_read[BSIZE], uint block_size)
{
    for (uint i = 0; i < blocks; i++)
    {
        for (uint j = 0; j < block_size; j++)
        {
            if (blk_read[j] != blk[j])
            {
                printf("expected=%d got=%d", blk[j], blk_read[j]);
                printf("Data in the block %d faulty\n", i);
                break;
            }
        }
    }
}

int
main() {
    printf("Testiranje init_raid...\n");
    init_raid(RAID4);
    printf("Uspesna inicijalizacija raida...\n");
    printf("\n-----------------------------------------------------------------------------------------------------\n\n");

    printf("Testiranje info_raid...\n");
    uint blkn, blks, diskn;
    blkn = blks = diskn = -1;

    if (info_raid(&blkn, &blks, &diskn) < 0)
    {
        printf("Error in info_raid...\n");
        exit(0);
    }

    printf("Broj blokova je: blkn = %d\n", blkn);
    printf("Velicina bloka je: blks = %d\n", blks);
    printf("Broj diskova je: diskn = %d\n", diskn);
    printf("\n-----------------------------------------------------------------------------------------------------\n\n");

    printf("Testiranje upisa i citanja...\n");

    int l, h, pid;
    uchar data[BSIZE];
    uchar data_read[BSIZE];
    int blocks = 1000;
    int child_procs = 0;

    for (int i = 0; i < child_procs; i++) {
        if (fork() == 0) {
            // child
            l = i * blocks;
            h = l + blocks;
            pid = i;
            for (int j=0; j<BSIZE; j++)
                data[j] = i+30;
            break;      // child does not make children
        } else {
            l = child_procs * blocks;
            h = l + blocks;
            pid = child_procs;
            for (int i=0; i<BSIZE; i++)
                data[i] = 255;             //i=child_procs-1
        }
    }

    l = child_procs * blocks;
    h = l + blocks;
    pid = child_procs;
    for (int i=0; i<BSIZE; i++)
        data[i] = 255;



    printf("pid = %d Upis...\n", pid);

    for (int i = l; i < h; i++)
    {
        if (write_raid(i, (uchar*) data) < 0)
        {
            printf("Error in write_raid...\n");
            exit(0);
        }
    }

    printf("pid = %d Citanje...\n", pid);

    for (int i = l; i < h; i++)
    {
        if (read_raid(i, (uchar*) data_read) < 0)
        {
            printf("Error in read_raid...\n");
            exit(0);
        }
    }

    check_data(blocks, data, data_read, BSIZE);

    printf("pid = %d Uspesni upis i citanje!\n", pid);

    exit(0);
}


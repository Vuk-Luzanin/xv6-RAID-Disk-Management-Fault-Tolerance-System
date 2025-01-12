#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int
main() {
    printf("Testiranje init_raid...\n");
    init_raid(RAID1);
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

//    int b, g;
//
//    if (fork())
//    {
//        b = 50;
//        g = 0;
//    }
//    else
//    {
//        b = 100;
//        g = 51;
//    }
//    char data[1024];
//    while (b <= g)
//    {
//        for (int i=0; i<1024; i++)
//            if (i%2)
//                data[i] = b;
//            else
//                data[i] = g;
//    }

    char data[BSIZE];
    for (int i=0; i<BSIZE; i++)
        data[i] = 'v';

    printf("Upis bloka...\n");
    if (write_raid(0, (uchar*) data) < 0)
    {
        printf("Error in write_raid...\n");
        exit(0);
    }

    char data_read[BSIZE];
    printf("Citanje bloka...\n");
    if (read_raid(0, (uchar*) data_read) < 0)
    {
        printf("Error in read_raid...\n");
        exit(0);
    }

    printf("Uporedjivanje...\n");
    for (int i=0; i<BSIZE; i++)
    {
        if (data[i] != data_read[i])
        {
            printf("Bad read...\n");
            exit(0);
        }
    }
    printf("Uspesni upis i citanje bloka!\n");

//    printf("\n-----------------------------------------------------------------------------------------------------\n\n");

    exit(0);
}

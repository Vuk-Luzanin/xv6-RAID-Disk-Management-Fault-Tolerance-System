#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main() {
    uint blkn, blks, diskn;
    blkn = blks = diskn = -1;

    if (info_raid(&blkn, &blks, &diskn) < 0)
    {
        printf("Error in info_raid...\n");
        exit(0);
    }
    printf("Broj blokova je: blkn = %d\n", blkn);
    printf("Velicina bloka je: blks = %d\n", blks);
    printf("Broj diskova je: diskn = %d\n\n", diskn);

//    char data[1024];
//    for (int i=0; i<10; i++)
//    {
//        if (i % 2)
//            data[i] = 'v';
//        else
//            data[i] = 'l';
//    }

    exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(int argc, char *argv[]) {
    for (int i = 0; i < 6000; i++) {
        uint64 a = (uint64) sbrk(4096);
        if (a == 0xffffffffffffffff) {
            break;
        }

        // modify the memory to make sure it's really allocated.
        *(char *) (a + 4096 - 1) = 1;
    }

    exit(0);
}

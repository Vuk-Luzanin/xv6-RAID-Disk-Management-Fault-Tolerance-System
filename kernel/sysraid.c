#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "raid.h"

uint64
sys_init_raid(void)
{
    int type;
    argint(0, &type);
    if (type < 0)
    {
        return -1;
    }

    setraidtype(type);
    return 0;
}

uint64
sys_read_raid(void)
{
    // Fetch arguments and check if they are valid
    int vblkn;              // virtual number of block
    argint(0, &vblkn);

    // get current process
    struct proc* p = myproc();
    uint64 data_addr;               // in virtual address space - parameter
    argaddr(1, &data_addr);

    // check if virtual address (for whole block) can be translated into physical address
    if (data_addr < 0 || walkaddr(p->pagetable, data_addr) == 0 ||
        walkaddr(p->pagetable, data_addr + BSIZE - 1) == 0)
        return -1;

    char data[BSIZE];
    if (readraid(vblkn, (uchar*)data) < 0)
        return -1;

    // translate from physical to virtual space
    if (copyout(p->pagetable, data_addr, data, BSIZE) < 0)
        return -1;

    return 0;
}

uint64
sys_write_raid(void)
{
    // Fetch arguments and check if they are valid
    int vblkn;
    argint(0, &vblkn);

    if (vblkn < 0 || vblkn >= raidblockn())
        return -1;

    struct proc* p = myproc();
    uint64 data_addr;           // in virtual address space - parameter
    argaddr(1, &data_addr);

    // check if virtual address (for whole block) can be translated into physical address
    if (data_addr < 0 || walkaddr(p->pagetable, data_addr) == 0 ||
        walkaddr(p->pagetable, data_addr + BSIZE - 1) == 0)
        return -1;

    char data[BSIZE];
    // translate in kernel address space
    if (copyin(p->pagetable, data, data_addr, BSIZE) < 0)
        return -1;

    return writeraid(vblkn, (uchar*)data);
}

uint64
sys_disk_fail_raid(void)
{
    int diskn;
    argint(0, &diskn);
    if (diskn < 0)
        return -1;

    return raidfail(diskn);
}

uint64
sys_disk_repaired_raid(void)
{
    int diskn;
    argint(0, &diskn);
    if (diskn < 0)
        return -1;

    return raidrepair(diskn);
}


uint64
sys_info_raid(void)
{
    // Getting arguments from registers
    uint64 blkn_addr, blks_addr, diskn_addr;        // in kernel space
    argaddr(0, &blkn_addr);             // from a0 register - first argument
    argaddr(1, &blks_addr);
    argaddr(2, &diskn_addr);

    if (blkn_addr < 0 || blks_addr < 0 || diskn_addr < 0)
        // error
        return -1;

    uint blkn = raidblockn();
    uint blks = BSIZE;
    uint diskn = DISKS;

    struct proc* p = myproc();
    // translate variables in virtual space
    if (copyout(p->pagetable, blkn_addr, (char*) (&blkn), sizeof(blkn)) < 0)
        return -1;
    if (copyout(p->pagetable, blks_addr, (char*) (&blks), sizeof(blks)) < 0)
        return -1;
    if (copyout(p->pagetable, diskn_addr, (char*) (&diskn), sizeof(diskn)) < 0)
        return -1;

    return 0;
}

uint64
sys_destroy_raid(void)
{
    return raiddestroy();
}


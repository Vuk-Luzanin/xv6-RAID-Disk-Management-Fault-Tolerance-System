# RAID Structure Implementation on xv6

This project involves the implementation of RAID structures on the **xv6** operating system. The aim is to enhance system performance and resilience to disk failures by managing multiple disks as a single logical unit. The project is divided into two parts:

1. **Part 1**: Implementation of basic RAID levels (RAID0, RAID1, RAID0+1) with support for disk failure management.
2. **Part 2**: Adding support for RAID4 and RAID5, and ensuring thread-safe access in a multi-threaded environment.

## Requirements

- **Language**: C
- **Operating System**: xv6
- **Development Environment**: CLion (recommended), Linux x64 (host OS)
- **Tools**: Emulator and required libraries (available on the course website)


## Usage

1. **Initialize RAID**: Use the `init_raid` system call to set up the desired RAID configuration.
2. **Perform Read/Write Operations**:
   - Use `read_raid` and `write_raid` to access data.
3. **Handle Disk Failures**:
   - Mark disks as failed using `disk_fail_raid` and repair them with `disk_repaired_raid`.
4. **Retrieve RAID Information**: Use `info_raid` to get details about the RAID structure.
5. **Destroy RAID**: Clean up the RAID setup using `destroy_raid`.

## System Calls

- **Initialization**: `int init_raid(enum RAID_TYPE raid);`
- **Read/Write Operations**:
  - `int read_raid(int blkn, uchar* data);`
  - `int write_raid(int blkn, uchar* data);`
- **Disk Management**:
  - `int disk_fail_raid(int diskn);`
  - `int disk_repaired_raid(int diskn);`
- **Information Retrieval**: `int info_raid(uint *blkn, uint *blks, uint *diskn);`
- **Destruction**: `int destroy_raid();`




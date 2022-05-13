/**
 * pt3-fs-find.c
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h> // write
#include <stdlib.h> // malloc
#include <sys/stat.h> // stat

#include </usr/src/sys/ufs/ffs/fs.h>
#include </usr/src/sys/ufs/ufs/dinode.h>
#include </usr/src/sys/ufs/ufs/dir.h>

void *get_inode_address(struct fs *superblock, void *disk, ino_t inode_num);
void *get_data_address(struct fs *superblock, void *disk, int data_block);
int check_direct(struct direct *dir);
void print_directory(struct fs *superblock, void *dick, ino_t inode_num, int num_spaces);
void print_directory_blk(struct fs *superblock, void *disk, int db_num, int num_spaces);


int
main (int argc, char *argv[]) {
    // Open and mmap file of disk dump into memory
    int fd = open("../partition.img", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Get size of partition.img
    struct stat file_info;
    if (fstat(fd, &file_info) == -1) {
        perror("fstat");
    }
    size_t file_size = file_info.st_size;

    void *disk = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0); 
    if (disk == MAP_FAILED) {
        perror("mmap");
    }

    // Finding the superblock and then printing contents of root inode
    struct fs *superblock = (void *)((char*)disk + SBLOCK_UFS2);
    print_directory(superblock, disk, UFS_ROOTINO, 0);
}

void print_directory(struct fs *superblock, void *disk, ino_t inode_num, int num_spaces) {
    /**
     * Prints out full directory
     */
    // Getting inode struct
    struct ufs2_dinode *inode = get_inode_address(superblock, disk, inode_num);

    // Getting number of direct blocks
    int num_blocks = inode->di_size;
    int num_db = UFS_NDADDR;

    // Iterate thru direct blocks, printing this contents
    int db_num;
    for (int i = 0; i < num_db; i++) {
        db_num = inode->di_db[i];
        if (!db_num) break;
        print_directory_blk(superblock, disk, db_num, num_spaces);
    }
}

void
print_directory_blk(struct fs *superblock, void *disk, int db_num, int num_spaces) {
    /**
     * Prints directories in specified block
     */
    // Getting data
    struct direct *dir = get_data_address(superblock, disk, db_num);

    // Iterate thru directs, printing them
    int res;
    while (dir->d_reclen > 0) {
        res = check_direct(dir);

        if (res == 1) { // Prints file name
            printf("%*s%s\n", num_spaces, "", dir->d_name);
        }

        if (res == 2) { // Prints directory name and then its contents
            printf("%*s%s:\n", num_spaces, "", dir->d_name);
            print_directory(superblock, disk, dir->d_ino, num_spaces+4);
        }

        dir = (struct direct*)((char*)dir + DIRECTSIZ(dir->d_namlen));
    }
}

int
check_direct(struct direct *dir) {
    /*
     * Determines whether this directory describes:
     * 0. free space or hidden file
     * 1. a directory
     * 2. a file
     */
    if (!dir->d_ino || dir->d_name[0] == '.') return 0;
    if (dir->d_type == DT_DIR) return 2;
    return 1;
}

void *
get_inode_address(struct fs *superblock, void *disk, ino_t inode_num) {
    /**
     * Takes in inode number and returns its location on disk
     */
    // Finding the address of inode section start in correct cylinder group
    int cg_inode_start_frag = ino_to_fsba(superblock, inode_num);
    int cg_inode_start_offset = lfragtosize(superblock, cg_inode_start_frag);

    // Finding the address in inode section of inode we want
    int inode_offset = ino_to_fsbo(superblock, inode_num) * sizeof(struct ufs2_dinode);

    int offset = cg_inode_start_offset + inode_offset;

    return (void*)((char*)disk + offset);
}

void *
get_data_address(struct fs *superblock, void *disk, int data_block) {
    /**
     * Takes in data block and returns its address in the system (relative to the buffer)
     */
     // Finding the start of the cylinder group desired
    int cg_num = dtog(superblock, data_block);
    int cg_start_addr = lfragtosize(superblock, (cgbase(superblock, cg_num)));

    // Finding the offset of the block number relative to the cylinder group start
    int blknum_in_cg = dtogd(superblock, data_block);
    int offset_of_blknum_in_cg = lfragtosize(superblock, blknum_in_cg);  

    int offset = offset_of_blknum_in_cg + cg_start_addr;

    return (void*)((char*)disk + offset);
}

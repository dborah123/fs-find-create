/**
 * fs-find.c
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h> // write
#include <stdlib.h> // malloc

#include </usr/src/sys/ufs/ffs/fs.h>
#include </usr/src/sys/ufs/ufs/dinode.h>
#include </usr/src/sys/ufs/ufs/dir.h>

void *get_inode_address(struct fs *superblock, void *disk, ino_t inode_num);
void *get_data_address(struct fs *superblock, void *disk, int data_block);

int
main (int argc, char *argv[]) {
    // Open and mmap file of disk dump into memory
    int fd = open("../partition.img", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Allocate space and read dump data into it
    void *disk;
    if ((disk = malloc(9000000)) == NULL) {
        perror("malloc");
    }

    if (read(fd, disk, 9000000) < 0) {
        perror("read");
    }

    // Finding the superblock and then root inode
    struct fs *superblock = (void *)((char*)disk + SBLOCK_UFS2);
    struct ufs2_dinode *root_inode = get_inode_address(superblock, disk, UFS_ROOTINO);
    printf("Root inode: %p\tdi_size: %d\n", root_inode, root_inode->di_size);

    // Getting data block
    struct direct *dir1 = get_data_address(superblock, disk, root_inode->di_db[0]);
    printf("Dir1 %p\tDir name: %s\n", dir1, dir1->d_name);
}

void *
get_inode_address(struct fs *superblock, void *disk, ino_t inode_num) {
    /**
     * Takes in inode number and returns its location on disk
     */
    int offset = lfragtosize(superblock, ino_to_fsba(superblock, inode_num))
                + ino_to_fsbo(superblock, inode_num) * sizeof(struct ufs2_dinode);

    return (void*)((char*)disk + offset);
}

void *
get_data_address(struct fs *superblock, void *disk, int data_block) {
    /**
     * Gets data address of data block 
     * set bg=dark
     */
    // Finding beginning of datablocks in specific cylinder group
    int cg_num = dtog(superblock, data_block);
    int cg_dblks_start = cgdmin(superblock, cg_num);
    int cyl_group_offset = lfragtosize(superblock, cg_dblks_start);

    // Finding offset from dblks start to desired datablock
    int blknum_in_cg = dtogd(superblock, data_block);
    int offset_of_blknum_in_cg = lfragtosize(superblock, blknum_in_cg);  

    int offset = cyl_group_offset + offset_of_blknum_in_cg;

    printf("Cylinder_group: %d, data start: %d, block_num: %d, offset of blk: %d, offset: %d\n", 
            cg_num, cg_dblks_start, blknum_in_cg, offset_of_blknum_in_cg, offset);

    return (void*)((char*)disk + offset_of_blknum_in_cg);
}

/**
 * pt3-fs-cat.c
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h> // write
#include <stdlib.h> // malloc
#include <sys/stat.h> // stat
#include <string.h> // strcmp

#include </usr/src/sys/ufs/ffs/fs.h>
#include </usr/src/sys/ufs/ufs/dinode.h>
#include </usr/src/sys/ufs/ufs/dir.h>

void *get_inode_address(struct fs *superblock, void *disk, ino_t inode_num);
void *get_data_address(struct fs *superblock, void *disk, int data_block);
int check_direct_cat(struct direct *dir, char *path, int file);
int search_directory(
    struct fs *superblock,
    void *disk,
    ino_t inode_num,
    char *path,
    int num_spaces
);
int search_directory_blk(
    struct fs *superblock,
    void *disk,
    int db_num, 
    char *path,
    int num_spaces
);
void print_file(struct fs *superblock, void *disk, ino_t inode_num);
void print_data_block(struct fs *superblock, void *disk, int db_num, int size);


int
main (int argc, char *argv[]) {
    // Retrieve input path
    if (argc != 3) {
        perror("argc != 3");
        return 1;
    }
    char *partition_name = argv[1];
    char *path = argv[2];

    // Open and mmap file of disk dump into memory
    int fd = open(partition_name, O_RDONLY);
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
    search_directory(superblock, disk, UFS_ROOTINO, path, 0);
}

int
search_directory(
    struct fs *superblock,
    void *disk,
    ino_t inode_num,
    char *path,
    int file
) {
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
        if (search_directory_blk(superblock, disk, db_num, path, 0)) {
            return 1;
        }
    }
    return 0;
}

int
search_directory_blk(
    struct fs *superblock,
    void *disk,
    int db_num,
    char *path,
    int file
) {
    /**
     * Searches directory block for matching path
     */
    // Get substring we are looking at
    char *last_char;
    if ((last_char = strchr(path, '/')) != NULL) {
        *last_char = '\0';
        last_char++;
        file = 0;
    } else {
        file = 1;
    }

    // Getting data
    struct direct *dir = get_data_address(superblock, disk, db_num);

    // Iterate thru directs, printing them
    int res, file_found;
    while (dir->d_reclen > 0) {
        res = check_direct_cat(dir, path, file);

        if (res == 1) { // Prints file name
            file_found = search_directory(superblock, disk, dir->d_ino, last_char, 0);
            
            if (file_found) return 1;
        }

        if (res == 2) { // Prints directory name and then its contents
            print_file(superblock, disk, dir->d_ino);
            return 1;
        }

        // Move to next direct struct
        dir = (struct direct*)((char*)dir + DIRECTSIZ(dir->d_namlen));
    }
    return 0;
}

void
print_file(struct fs *superblock, void *disk, ino_t inode_num) {
    /**
     * Prints contents of file
     */
    // Get inode data
    struct ufs2_dinode *inode = get_inode_address(superblock, disk, inode_num);

    // Getting number of direct and indirect blocks
    int total_num_blocks = inode->di_blocks;
    int num_direct_blocks = total_num_blocks > 12 ? 12 : total_num_blocks;
    int num_indirect_blocks = total_num_blocks > 12 ? total_num_blocks - 12 : 0;

    // Getting data size
    int file_size = inode->di_size;

    // Handling direct blocks
    int db_num;
    for (int i = 0; i < num_direct_blocks; i++) {
        db_num = inode->di_db[i];
        if (!db_num) return;

        // If we are printing remainder of file, return
        if (file_size < superblock->fs_bsize) {
            print_data_block(superblock, disk, db_num, file_size); 
            return;
        }
        print_data_block(superblock, disk, db_num, superblock->fs_bsize);
        file_size -= superblock->fs_bsize;
    }

    // Handling indirect block
}

void
print_data_block(struct fs *superblock, void *disk, int db_num, int size) {
    /**
     * Prints the data block specified by the block number
     */
    // Get data block address
    void *data = get_data_address(superblock, disk, db_num);

    // Write data
    if (!fwrite(data, size, 1, stdout)) {
        perror("fwrite");
    }
}


int
check_direct_cat(struct direct *dir, char *path, int file) {
    /*
     * Determines whether this directory describes:
     * 0. directory and path do not match
     * 1. directories match
     * 2. file found
     */
    if (!dir->d_ino || strcmp(path, dir->d_name)!= 0) return 0;
    if (dir->d_type == DT_REG && file) return 2;
    if (dir->d_type == DT_DIR && !file) return 1;
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

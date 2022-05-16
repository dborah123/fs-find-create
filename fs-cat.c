/**
 * fs-cat.c
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h>  // write
#include <stdlib.h>   // malloc
#include <sys/stat.h> // stat
#include <string.h>   // strcmp
#include <stdlib.h>   // exit

#include </usr/src/sys/ufs/ffs/fs.h>
#include </usr/src/sys/ufs/ufs/dinode.h>
#include </usr/src/sys/ufs/ufs/dir.h>

#define SINGLE 1
#define DOUBLE 2


void *get_inode_address(struct fs *superblock, void *partition_start, ino_t inode_num);
void *get_data_address(struct fs *superblock, void *partition_start, int data_block);
int check_direct_cat(struct direct *dir, char *path, int file);
int search_directory(
    struct fs *superblock,
    void *partition_start,
    ino_t inode_num,
    char *path,
    int num_spaces
);
int search_directory_blk(
    struct fs *superblock,
    void *partition_start,
    int db_num, 
    char *path,
    int blk_size,
    int num_spaces
);
void print_file(struct fs *superblock, void *partition_start, ino_t inode_num);
void print_data_block(struct fs *superblock, void *partition_start, int db_num, int size);
int search_indirect_blocks(
    struct fs *superblock,
    void *partition_start,
    int db_num,
    char *path,
    int bytes_left,
    int indirection_type,
    int file
);

void print_indirect_block(
    struct fs *superblock,
    void *partition_start,
    int db_num,
    int size,
    int indirection_type
);


int
main (int argc, char *argv[]) {
    // Retrieve input path
    if (argc != 3) {
        perror("argc != 3");
        exit(1);
    }
    char *partition_name = argv[1];
    char *path = argv[2];

    // Open and mmap file of partition_start dump into memory
    int fd = open(partition_name, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    // Get size of partition.img
    struct stat file_info;
    if (fstat(fd, &file_info) == -1) {
        perror("fstat");
        exit(1);
    }
    size_t file_size = file_info.st_size;

    void *partition_start = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0); 
    if (partition_start == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Finding the superblock and then search for path
    struct fs *superblock = (void *)((char*)partition_start + SBLOCK_UFS2);
    search_directory(superblock, partition_start, UFS_ROOTINO, path, 0);
}

int
search_directory(
    struct fs *superblock,
    void *partition_start,
    ino_t inode_num,
    char *path,
    int file
) {
    /**
     * Prints out full directory
     */
    // Getting inode struct
    struct ufs2_dinode *inode = get_inode_address(superblock, partition_start, inode_num);

    // Getting number of direct blocks
    int num_db = UFS_NDADDR;

    // Remaining bytes of file after full blocks
    int bytes_left = inode->di_size % superblock->fs_bsize;
    int num_blocks = (inode->di_size / superblock->fs_bsize) + 1;

    // Iterate thru direct blocks, printing this contents
    int db_num, blk_size;
    for (int i = 0; i < num_db; i++) {
        db_num = inode->di_db[i];
        if (!db_num) break;

        // Setting size of block of direct we are searching
        blk_size = i + 1 < num_blocks ? superblock->fs_bsize : bytes_left; 

        if (search_directory_blk(superblock, partition_start, db_num, path, blk_size, 0))
            return 1;
    }

    // Handling indirect blocks
    int bytes_left_after_dbs = inode->di_size - (12 * superblock->fs_bsize);
    if (!bytes_left_after_dbs) return 0;

    if (!inode->di_ib[0]) return 0;
    search_indirect_blocks(
        superblock,
        partition_start,
        inode->di_ib[0],
        path,
        bytes_left_after_dbs,
        SINGLE,
        file
    );

    int bytes_left_after_single = bytes_left_after_dbs - (1024 * superblock->fs_bsize);

    if (!inode->di_ib[1]) return 0;
    search_indirect_blocks(
        superblock,
        partition_start,
        inode->di_ib[1],
        path,
        bytes_left_after_single,
        DOUBLE,
        file
    );

    return 0;
}

int
search_directory_blk(
    struct fs *superblock,
    void *partition_start,
    int db_num,
    char *path,
    int blk_size,
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
    struct direct *dir = get_data_address(superblock, partition_start, db_num);

    // Iterate thru directs, printing them
    int res, file_found, bytes_left = blk_size;
    while (bytes_left > 0) {
        res = check_direct_cat(dir, path, file);

        if (res == 1) { // Path matches and directory
            file_found = search_directory(
                            superblock,
                            partition_start,
                            dir->d_ino,
                            last_char,
                            0
                        );
            if (file_found) return 1;
        }

        if (res == 2) { // File matches >>> print contents
            print_file(superblock, partition_start, dir->d_ino);
            return 1;
        }

        // Move to next direct struct and update bytes elft
        bytes_left -= dir->d_reclen;
        dir = (struct direct*)((char*)dir + DIRECTSIZ(dir->d_namlen));
    }
    return 0;
}

int
search_indirect_blocks(
    struct fs *superblock,
    void *partition_start,
    int db_num,
    char *path,
    int bytes_left,
    int indirection_type,
    int file
) {
    /**
     * Searches indirect blocks
     */
    // Get indirect datablock
    ufs2_daddr_t *data = get_data_address(superblock, partition_start, db_num);

    int num_db_nums = superblock->fs_bsize / 8;
    int in_db_num, bytes_in_block, res;

    for (int i = 0; i < num_db_nums; i++) {
        if (bytes_left <= 0) return 0;
        // Get data block number
        in_db_num = data[i];

        if (indirection_type == SINGLE) {
            // Getting bytes in block
            bytes_in_block = bytes_left >= superblock->fs_bsize
                            ? superblock->fs_bsize
                            : bytes_left;

            res = search_directory_blk(
                superblock,
                partition_start,
                in_db_num,
                path,
                bytes_left,
                file
            );

            if (res) return 1;

            bytes_left -= bytes_in_block;
        }

        if (indirection_type == DOUBLE) {
            // Getting bytes in block
            bytes_in_block = bytes_left >= (superblock->fs_bsize * 4096)
                            ? superblock->fs_bsize * 4096
                            : bytes_left;

            res = search_indirect_blocks(
                superblock,
                partition_start,
                in_db_num,
                path,
                bytes_left,
                SINGLE,
                file
            );

            if (res) return 1;

            bytes_left -= bytes_in_block;
        }
    }

    return 0;
}

void
print_file(struct fs *superblock, void *partition_start, ino_t inode_num) {
    /**
     * Prints contents of file
     */
    // Get inode data
    struct ufs2_dinode *inode = get_inode_address(superblock, partition_start, inode_num);

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
            print_data_block(superblock, partition_start, db_num, file_size); 
            return;
        }
        print_data_block(superblock, partition_start, db_num, superblock->fs_bsize);
        file_size -= superblock->fs_bsize;
    }

    // Handling indirect block
    int file_size_after_dbs = file_size - (superblock->fs_bsize * 12);

    if (!inode->di_ib[0]) return;
    print_indirect_block(
        superblock,
        partition_start,
        db_num,
        file_size_after_dbs,
        SINGLE
    );

    int file_size_after_single = file_size_after_dbs - (superblock->fs_bsize * 4096);

    if (!inode->di_ib[1]) return;
    print_indirect_block(
        superblock,
        partition_start,
        db_num,
        file_size_after_single,
        DOUBLE
    );
}

void
print_indirect_block(
    struct fs *superblock,
    void *partition_start,
    int db_num,
    int size,
    int indirection_type
) {
    /**
     * Write file block data to standard out. Only size size is written
     */
    // Get indirect datablock
    ufs2_daddr_t *data = get_data_address(superblock, partition_start, db_num);

    int num_db_nums = superblock->fs_bsize / 8;
    int in_db_num, bytes_in_block;
    for (int i = 0; i < num_db_nums; i++) {
        if (size <= 0) return;
        // Get data block number
        in_db_num = data[i];

        if (indirection_type == SINGLE) {
            // Getting bytes in block
            bytes_in_block = size >= superblock->fs_bsize
                            ? superblock->fs_bsize
                            : size;

            print_data_block(superblock, partition_start, in_db_num, bytes_in_block);

            size -= bytes_in_block;
        }

        if (indirection_type == DOUBLE) {
            // Getting bytes in block
            bytes_in_block = size >= (superblock->fs_bsize * 4096)
                            ? superblock->fs_bsize * 4096
                            : size;

            print_indirect_block(
                superblock, partition_start, in_db_num, bytes_in_block, SINGLE
            );

            size -= bytes_in_block;
        }
    }
}

void
print_data_block(struct fs *superblock, void *partition_start, int db_num, int size) {
    /**
     * Prints the data block specified by the block number
     */
    // Get data block address
    void *data = get_data_address(superblock, partition_start, db_num);

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
get_inode_address(struct fs *superblock, void *partition_start, ino_t inode_num) {
    /**
     * Takes in inode number and returns its location on partition_start
     */
    // Finding the address of inode section start in correct cylinder group
    int cg_inode_start_frag = ino_to_fsba(superblock, inode_num);
    int cg_inode_start_offset = lfragtosize(superblock, cg_inode_start_frag);

    // Finding the address in inode section of inode we want
    int num_inode_in_cg = ino_to_fsbo(superblock, inode_num);
    int inode_offset = num_inode_in_cg * sizeof(struct ufs2_dinode);

    int offset = cg_inode_start_offset + inode_offset;

    return (void*)((char*)partition_start + offset);
}

void *
get_data_address(struct fs *superblock, void *partition_start, int data_block) {
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

    return (void*)((char*)partition_start + offset);
}

/**
 * fs-cat.c
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h> // write
#include <sys/stat.h> // stat

#include </usr/src/sys/ufs/ffs/fs.h>
#include </usr/src/sys/ufs/ufs/dinode.h>
#include </usr/src/sys/ufs/ufs/dir.h>

void *get_inode_address(struct fs *superblock, void *disk, ino_t inode_num);
void *get_data_address(struct fs *superblock, void *disk, int data_block);
void search_directory(
    struct fs *superblock,
    void *disk,
    ino_t inode_num,
    char *path,
    int file
);
void print_file(struct fs *superblock, void *disk, ino_t inode_num);
int check_direct_cat(struct direct *dir, char *path, int file);

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
        return 1;
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
     * Searches directories for matching path
     */
    // Get substring we are looking at
    char *last_char;
    if ((last_char = strchr(path, '/')) != NULL) {
        *last_char = '\0';
        last_char++;
        file = 0;
    } else { // designate that we are looking for a file
        file = 1;
    }

    // Getting inode struct from inode number
    struct ufs2_dinode *inode = get_inode_address(superblock, disk, inode_num);

    // Getting data address
    struct direct *dir = get_data_address(superblock,disk, inode->di_db[0]);

    while (dir->d_reclen > 0) {
        int res = check_direct_cat(dir, path, file);
        if (res == 1) {
            search_directory(superblock, disk, dir->d_ino, last_char, 0);
        }

        if (res == 2) {
            print_file(superblock, disk, dir->d_ino);
            return;
        }

        // Move to next direct struct
        dir = (struct direct*)((char*)dir + DIRECTSIZ(dir->d_namlen));
    }
    return 0;
}

int
check_direct_cat(struct direct *dir, char *path, int file) {
    /*
     * Determines whether this directory describes:
     * 0. directory and path do not match
     * 1. directories match
     * 2. a file found
     */
    if (!dir->d_ino || strcmp(path, dir->d_name)) return 0;
    if (dir->d_type == DT_REG && file) return 2;
    if (dir->d_type == DT_DIR && !file) return 1;
    return 0;
}

void
print_file(struct fs *superblock, void *disk, ino_t inode_num) {
    /**
     * Prints contents of file described by inode
     */
    // Getting inode struct from inode number
    struct ufs2_dinode *inode = get_inode_address(superblock, disk, inode_num);

    // Getting data address
    void *data = get_data_address(superblock,disk, inode->di_db[0]);

    size_t data_size = inode->di_size;

    // Write data to standard out
    if (!fwrite(data, data_size, 1, stdout)) {
        perror("write");
    }

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

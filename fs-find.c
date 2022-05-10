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

int
main (int argc, char *argv[]) {
    // Open and mmap file of disk dump into memory
    int fd = open("../partition.img", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Allocate space and read dump data into it
    char *disk;
    if ((disk = malloc(6000000)) == NULL) {
        perror("malloc");
    }

    if (read(fd, disk, 6000000) < 0) {
        perror("read");
    }

    // Finding the superblock and then root inode
    struct fs *superblock = disk + SBLOCK_UFS2;
    printf("Mount point: %s\n", superblock->fs_fsmnt);

    struct ufs2_dinode *root_inode = ino_to_cg(superblock, 2);
    printf("di_db[0]: %p\n", root_inode->di_db[0]);
}

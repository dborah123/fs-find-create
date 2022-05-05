/**
 * fs-find.c
 */
#include <stdio.h>

int
main (int argc, char *argv[]) {
    printf("Hello world\n");


}

void
get_inode(u_int32_t inode_number) {
    /**
     * Takes in inode number as a parameter and returns the location of the
     * inode struct
     */
    
}

char *
get_data_from_inode(inode *inode) {
    /**
     * Takes in inode as parameter. Return pointer to data of inode
     */
    int block_number = inode->di_db[0];
    int offset = offset * 32768;

    return (char *)offset;
}

void print_direct(direct dir) {
    /**
     * Intakes direct and prints all of the directories and files under it
     */

    inode *inode = get_inode(u_int32_t d_ino);

    int offset = get_data_offset_from_inode(
}

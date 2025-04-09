#include <stdio.h>
#include <stdlib.h>

#include "ext4_fs.h"

/*
 * Currently, main.c is used only for testing functions and contains
 * random shit. Any useful functionality will be added later.
 * If you're reading this and no meaningful functionality has been
 * implemented yet, read the previous statement again.
 */

int main(void) {
    struct ext4_fs fs;
    init_ext4_fs("../test_ext4_x32.img", &fs);

    // printf("Total inodes: %d\n", fs.sb->s_inodes_count);
    // printf("Total block groups: %d\n", fs.block_group_count);
    // printf("Inode table location: %d\n", fs.gdt[0]->gd_inode_table_lo);

    struct ext4_inode inode;
    read_inode(&fs, &inode, 24);

    for (int i = 0; i < 15; i++)
        printf("%d ", inode.i_block[i]);

    // Testing reading with inodes
    int not_null_blocks = 0;
    for (int i = 0; i < 15; i++)
        if (inode.i_block[i] != 0)
            not_null_blocks++;

    char *buffer = (char *) malloc(fs.block_size * not_null_blocks);
    for (int i = 0; i < not_null_blocks; i++) {
        fseek(fs.img, inode.i_block[i] * fs.block_size, SEEK_SET);
        if (fread(buffer + fs.block_size * i, fs.block_size, 1, fs.img) != 1) {
            printf("Error reading file\n");
        }
    }

    fwrite(buffer, fs.block_size * not_null_blocks, 1, stdout);

    free(buffer);

    return 0;
}

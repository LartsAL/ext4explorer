#ifndef EXT4_FS_H
#define EXT4_FS_H

#include <stdio.h>

#include "ext4_structs.h"

struct ext4_fs {
    FILE *img;
    struct ext4_super_block *sb;
    struct ext4_group_descriptor **gdt;
    uint64_t fs_size;
    uint32_t block_size;
    uint32_t block_group_count;
    uint32_t inodes_per_group;
};

uint8_t init_ext4_fs(const char *fname, struct ext4_fs *fs);

uint8_t read_super_block(struct ext4_fs *fs, struct ext4_super_block *sb);

uint8_t read_group_descriptor(struct ext4_fs *fs, struct ext4_group_descriptor *gd);

uint8_t read_inode(struct ext4_fs *fs, struct ext4_inode *inode, uint32_t inode_num);

#endif /* EXT4_FS_H */

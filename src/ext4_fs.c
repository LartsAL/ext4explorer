#include "ext4_fs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext4_structs.h"
#include "ext4_utils.h"

uint8_t init_ext4_fs(const char *fname, struct ext4_fs *fs) {
    if (fname == NULL || fs == NULL) return -1;

    memset(fs, 0, sizeof(struct ext4_fs));

    fs->img = fopen(fname, "rb");
    if (fs->img == NULL) {
        fprintf(stderr, "ERROR: Could not open file\n");
        return 1;
    }

    fseek(fs->img, 0, SEEK_END);
    fs->fs_size = ftell(fs->img);
    rewind(fs->img);

    fs->sb = malloc(sizeof(struct ext4_super_block));
    if (fs->sb == NULL) {
        fprintf(stderr, "ERROR: Memory allocation for superblock failed\n");
        goto cleanup;
    }

    if (!read_super_block(fs, fs->sb)) {
        fprintf(stderr, "ERROR: Could not read superblock\n");
        goto cleanup;
    }

    if (fs->sb->s_magic != EXT4_S_MAGIC) {
        fprintf(stderr, "ERROR: Wrong superblock magic. Filesystem either not ext4 or superblock is damaged\n");
        goto cleanup;
    }

    fs->block_size = 1024 << fs->sb->s_log_block_size;

    // Skip free space between superblock and GDT
    const int32_t skip_after_sb = (int32_t) fs->block_size - ftell(fs->img);
    if (skip_after_sb > 0) {
        fseek(fs->img, skip_after_sb, SEEK_CUR);
    }

    fs->block_group_count = (uint32_t) ceil((double) fs->sb->s_blocks_count_lo / fs->sb->s_blocks_per_group);
    fs->inodes_per_group = fs->sb->s_inodes_count / fs->block_group_count;

    fs->gdt = malloc(sizeof(struct ext4_group_descriptor *) * fs->block_group_count);
    if (fs->gdt == NULL) {
        fprintf(stderr, "ERROR: Memory allocation for GDT failed\n");
        goto cleanup;
    }

    for (int i = 0; i < fs->block_group_count; i++) {
        fs->gdt[i] = malloc(sizeof(struct ext4_group_descriptor));
        if (fs->gdt[i] == NULL) {
            fprintf(stderr, "ERROR: Memory allocation for group descriptor failed\n");
            goto cleanup;
        }

        if (!read_group_descriptor(fs, fs->gdt[i])) {
            fprintf(stderr, "ERROR: Could not read group descriptor\n");
            goto cleanup;
        }
    }

    return 0;

cleanup:
    if (fs->sb != NULL) free(fs->sb);
    if (fs->gdt != NULL) {
        for (int i = 0; i < fs->block_group_count; i++)
            if (fs->gdt[i] != NULL) free(fs->gdt[i]);
        free(fs->gdt);
    }
    fclose(fs->img);
    return 1;
}

uint8_t read_super_block(struct ext4_fs *fs, struct ext4_super_block *sb) {
    fseek(fs->img, 1024, SEEK_SET);
    return fread(sb, sizeof(struct ext4_super_block), 1, fs->img) == 1;
}

uint8_t read_group_descriptor(struct ext4_fs *fs, struct ext4_group_descriptor *gd) {
    return fread(gd, sizeof(struct ext4_group_descriptor), 1, fs->img) == 1;
}

// TODO: Rewrite?
uint8_t read_inode(struct ext4_fs *fs, struct ext4_inode *inode, uint32_t inode_num) {
    const uint32_t inode_bg_num = (inode_num - 1) / fs->inodes_per_group;
    const uint32_t relative_inode_num = (inode_num - 1) % fs->inodes_per_group;
    const struct ext4_group_descriptor *related_gd = fs->gdt[inode_bg_num];
    const int32_t offset = (int32_t) (related_gd->gd_inode_table_lo * fs->block_size
                                      + relative_inode_num * sizeof(struct ext4_inode));

    fseek(fs->img, offset, SEEK_SET);
    return fread(inode, sizeof(struct ext4_inode), 1, fs->img) == 1;
}

uint8_t read_physical_block(struct ext4_fs *fs, uint8_t *buffer, uint32_t physical_block_num) {
    fseek(fs->img, physical_block_num * fs->block_size, SEEK_SET);
    return fread(buffer, fs->block_size, 1, fs->img) == 1;
}

uint8_t read_logical_block(struct ext4_fs *fs, struct ext4_inode *inode, uint8_t *buffer, uint32_t logical_block_num) {
    int8_t level = resolve_addressing_level(logical_block_num, fs->block_size);
    if (level == -1) return 0;

    const uint32_t entries_per_block = fs->block_size / sizeof(uint32_t);
    uint32_t physical_block_num;

    switch (level) {
        case 0: physical_block_num = inode->i_block[logical_block_num];
            break;
        case 1: physical_block_num = inode->i_block[12];
            break;
        case 2: physical_block_num = inode->i_block[13];
            break;
        case 3: physical_block_num = inode->i_block[14];
            break;
        default: return 0;
    }

    uint32_t *block_buffer = (uint32_t *) malloc(fs->block_size);
    if (block_buffer == NULL) return 0;

    while (level > 0) {
        if (!read_physical_block(fs, (uint8_t *) block_buffer, physical_block_num)) {
            free(block_buffer);
            return 0;
        }

        uint32_t offset;
        if (level == 1) {
            offset = logical_block_num - 12;
        } else if (level == 2) {
            offset = (logical_block_num - 12 - entries_per_block) / entries_per_block;
        } else {
            offset = (logical_block_num - 12 - entries_per_block - entries_per_block * entries_per_block)
                     / (entries_per_block * entries_per_block);
        }

        if (offset >= entries_per_block || block_buffer[offset] == 0) {
            free(block_buffer);
            return 0;
        }

        physical_block_num = block_buffer[offset];
        level--;
    }

    free(block_buffer);

    return read_physical_block(fs, buffer, physical_block_num) == 1;
}

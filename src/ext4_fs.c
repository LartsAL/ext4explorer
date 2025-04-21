#include "ext4_fs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc32c.h"
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

    if (!read_primary_super_block(fs, fs->sb)) {
        fprintf(stderr, "ERROR: Could not read superblock\n");
        goto cleanup;
    }

    if (!is_valid_super_block(fs->sb)) {
        fprintf(stderr, "ERROR: Filesystem either not ext4 or primary superblock is damaged\n");

        if (!read_backup_super_block(fs, fs->sb)) {
            fprintf(stderr, "ERROR: No valid superblock backups found\n");
            goto cleanup;
        }
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

uint8_t read_primary_super_block(struct ext4_fs *fs, struct ext4_super_block *sb) {
    fseek(fs->img, 1024, SEEK_SET);
    return fread(sb, sizeof(struct ext4_super_block), 1, fs->img) == 1;
}

uint8_t read_super_block(struct ext4_fs *fs, struct ext4_super_block *sb, uint32_t block_num) {

}

uint8_t read_backup_super_block(struct ext4_fs *fs, struct ext4_super_block *sb) {
    printf("Trying to read superblock backup...\n");

    const uint32_t common_block_sizes[4] = {4096, 1024, 2048, 8192};
    // Block groups 1, 3, 5, 7 and 9
    const uint32_t common_backup_blocks[4][5] = {
        {32768, 98304, 163840, 229376, 294912}, // 4K
        {8193, 24577, 40961, 57345, 73729},     // 1K
        {16384, 49152, 81920, 114688, 147456},  // 2K
        {65528, 196584, 327640, 458696, 589752} // 8K
    };

    struct ext4_super_block super_block_backup;
    for (int i = 0; i < 4; i++) {
        printf("%dK blocks:\n", common_block_sizes[i] / 1024);

        for (int j = 0; j < 5; j++) {
            printf("  Block %d: ", common_backup_blocks[i][j]);

            uint64_t offset = common_backup_blocks[i][j] * common_block_sizes[i];

            if (fs->fs_size < offset) {
                printf("SKIPPED (beyond filesystem size)\n");
                break;
            }

            if (offset > LONG_MAX) {
                printf("SKIPPED (long overflow)\n");
                break;
            }

            fseeko64(fs->img, offset, SEEK_SET);
            fread(&super_block_backup, sizeof(struct ext4_super_block), 1, fs->img);

            if (is_valid_super_block(&super_block_backup)) {
                memcpy(fs->sb, &super_block_backup, sizeof(struct ext4_super_block));
                printf("SUCCEED\n");
                return 1;
            }

            printf("FAILED\n");
        }
    }
    return 0;
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

uint8_t is_valid_super_block(struct ext4_super_block *sb) {
    if (sb->s_magic != EXT4_S_MAGIC) {
        return 0;
    }

    struct ext4_super_block sb_copy;
    memcpy(&sb_copy, sb, sizeof(struct ext4_super_block));

    sb_copy.s_checksum = 0;

    const uint32_t expected_csum = crc32c((uint8_t *) &sb_copy, sizeof(struct ext4_super_block) - 4);
    if (sb->s_checksum != expected_csum) {
        return 0;
    }

    return 1;
}

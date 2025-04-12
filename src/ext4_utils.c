#include "ext4_utils.h"

uint64_t fast_pow(uint32_t base, uint32_t power) {
    if (power == 0) {
        return 1;
    }
    if (power % 2 == 1) {
        return base * fast_pow(base, power - 1);
    }
    uint64_t sqrt = fast_pow(base, power / 2);
    return sqrt * sqrt;
}

int8_t resolve_addressing_level(uint32_t logical_block_num, const uint32_t block_size) {
    if (logical_block_num < 12) return 0;

    const uint32_t entries_per_block = block_size / sizeof(uint32_t);
    logical_block_num -= 12;
    if (logical_block_num < entries_per_block) return 1;

    const uint64_t entries_per_block_2 = entries_per_block * entries_per_block;
    logical_block_num -= entries_per_block;
    if (logical_block_num < entries_per_block_2) return 2;

    const uint64_t entries_per_block_3 = entries_per_block * entries_per_block_2;
    logical_block_num -= entries_per_block_2;
    if (logical_block_num < entries_per_block_3) return 3;

    return -1;
}

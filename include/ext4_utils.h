#ifndef EXT4_UTILS_H
#define EXT4_UTILS_H

#include <stdint.h>

uint64_t fast_pow(uint32_t base, uint32_t power);

int8_t resolve_addressing_level(uint32_t logical_block_num, uint32_t block_size);

#endif
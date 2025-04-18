#include "crc32c.h"

void generate_crc_table(uint32_t table[256]) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t curByte = i;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (curByte & 1) {
                curByte = (curByte >> 1) ^ CRC32C_POLY_REFL;
            } else {
                curByte >>= 1;
            }
        }
        table[i] = curByte;
    }
}

uint32_t crc32c(const uint8_t *data, uint32_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        const uint32_t idx = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32c_table[idx];
    }
    return crc;
}

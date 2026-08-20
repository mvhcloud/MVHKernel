#ifndef MVH_CRC32_H
#define MVH_CRC32_H

#include <stdint.h>

uint32_t crc32_update(uint32_t previous, const void *data, uint32_t size);
uint32_t crc32(const void *data, uint32_t size);
int crc32_self_test(void);

#endif

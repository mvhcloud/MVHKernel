#include <stdint.h>
#include "mvh/crc32.h"

uint32_t crc32_update(uint32_t previous, const void *data, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t value = ~previous;
    uint32_t index;
    uint32_t bit;
    if (bytes == 0 && size != 0u) return previous;
    for (index = 0u; index < size; index++) {
        value ^= bytes[index];
        for (bit = 0u; bit < 8u; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(value & 1u);
            value = (value >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~value;
}

uint32_t crc32(const void *data, uint32_t size)
{
    return crc32_update(0u, data, size);
}

int crc32_self_test(void)
{
    static const char vector[] = "123456789";
    uint32_t split = crc32_update(0u, vector, 4u);
    split = crc32_update(split, vector + 4u, 5u);
    if (crc32(vector, 9u) != 0xCBF43926u || split != 0xCBF43926u) return -1;
    return crc32(0, 0u) == 0u ? 0 : -1;
}

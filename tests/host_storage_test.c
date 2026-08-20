#include <stdint.h>
#include <stdio.h>
#include "mvh/block.h"
#include "mvh/crc32.h"

static int rejected_read(void *context, uint64_t lba, uint32_t count, void *buffer)
{
    (void)context;
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

int main(void)
{
    block_init();
    if (crc32_self_test() != 0) return 1;
    if (block_self_test() != 0) return 2;
    if (block_count() != 0u) return 3;
    if (block_register("", 512u, 1u, 0u, 0, rejected_read, 0) >= 0) return 4;
    if (block_register("bad-sector", 513u, 1u, 0u, 0, rejected_read, 0) >= 0) return 5;
    if (block_register("bad-writer", 512u, 1u, 1u, 0, rejected_read, 0) >= 0) return 6;
    puts("host storage and CRC32 tests passed");
    return 0;
}

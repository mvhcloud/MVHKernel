#include <stdint.h>
#include <stdio.h>
#include "mvh/block.h"
#include "mvh/bootinfo.h"
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
    if (bootinfo_self_test() != 0) return 7;
    if (bootinfo_capture(65536u, 0u) != 0 || bootinfo_current()->versioned != 0u ||
        bootinfo_current()->memory_kib != 65536u) return 8;
    if (bootinfo_capture(1024u, 0u) == 0) return 9;
    puts("host storage, CRC32 and BootInfo tests passed");
    return 0;
}

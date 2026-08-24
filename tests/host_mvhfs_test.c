#include <stdint.h>
#include <stdio.h>
#include "mvh/block.h"
#include "mvh/mvhfs.h"

#define TEST_SECTORS 600u
#define SECTOR_SIZE 512u
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

typedef struct {
    uint8_t bytes[TEST_SECTORS * SECTOR_SIZE];
    int32_t writes_before_failure;
} memory_disk_t;

static memory_disk_t disk;

static int memory_read(void *context, uint64_t lba, uint32_t count, void *buffer)
{
    memory_disk_t *memory = (memory_disk_t *)context;
    uint8_t *output = (uint8_t *)buffer;
    uint32_t index;
    if (lba >= TEST_SECTORS || count > TEST_SECTORS - lba) return -1;
    for (index = 0u; index < count * SECTOR_SIZE; index++)
        output[index] = memory->bytes[(uint32_t)lba * SECTOR_SIZE + index];
    return 0;
}

static int memory_write(void *context, uint64_t lba, uint32_t count, const void *buffer)
{
    memory_disk_t *memory = (memory_disk_t *)context;
    const uint8_t *input = (const uint8_t *)buffer;
    uint32_t index;
    if (lba >= TEST_SECTORS || count > TEST_SECTORS - lba) return -1;
    if (memory->writes_before_failure == 0) return -1;
    if (memory->writes_before_failure > 0) memory->writes_before_failure--;
    for (index = 0u; index < count * SECTOR_SIZE; index++)
        memory->bytes[(uint32_t)lba * SECTOR_SIZE + index] = input[index];
    return 0;
}

static int text_equal(const char *left, const char *right, uint32_t size)
{
    uint32_t index;
    for (index = 0u; index < size; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

int main(void)
{
    static const char first[] = "persistent version one";
    static const char second[] = "persistent version two after atomic commit";
    mvhfs_t filesystem;
    mvhfs_entry_t entries[MVHFS_MAX_FILES];
    char output[MVHFS_FILE_MAX];
    uint8_t saved_superblock_byte;
    uint32_t size;
    int device;

    disk.writes_before_failure = -1;
    block_init();
    device = block_register("persistent-memory-disk", SECTOR_SIZE, TEST_SECTORS,
                            1u, &disk, memory_read, memory_write);
    CHECK(device >= 0);
    CHECK(mvhfs_format((uint32_t)device) == 0);
    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_create(&filesystem, "config") == 0);
    CHECK(mvhfs_create(&filesystem, "config") != 0);
    CHECK(mvhfs_write(&filesystem, "config", first, sizeof(first)) == 0);
    CHECK(mvhfs_list(&filesystem, entries, MVHFS_MAX_FILES) == 1u);
    CHECK(entries[0].size == sizeof(first));

    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) == 0);
    CHECK(size == sizeof(first) && text_equal(output, first, size));

    disk.writes_before_failure = 12;
    CHECK(mvhfs_write(&filesystem, "config", second, sizeof(second)) != 0);
    disk.writes_before_failure = -1;
    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) == 0);
    CHECK(size == sizeof(first) && text_equal(output, first, size));

    CHECK(mvhfs_write(&filesystem, "config", second, sizeof(second)) == 0);
    saved_superblock_byte = disk.bytes[SECTOR_SIZE];
    disk.bytes[SECTOR_SIZE] ^= 1u;
    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) == 0);
    CHECK(size == sizeof(first) && text_equal(output, first, size));
    disk.bytes[SECTOR_SIZE] = saved_superblock_byte;

    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) == 0);
    CHECK(size == sizeof(second) && text_equal(output, second, size));
    disk.bytes[10u * SECTOR_SIZE] ^= 1u;
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) == -3);
    disk.bytes[10u * SECTOR_SIZE] ^= 1u;

    CHECK(mvhfs_remove(&filesystem, "config") == 0);
    CHECK(mvhfs_mount(&filesystem, (uint32_t)device) == 0);
    CHECK(mvhfs_list(&filesystem, entries, MVHFS_MAX_FILES) == 0u);
    CHECK(mvhfs_read(&filesystem, "config", output, sizeof(output), &size) != 0);
    puts("MVHFS persistence and recovery tests passed");
    return 0;
}

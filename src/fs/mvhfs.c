#include <stdint.h>
#include "mvh/block.h"
#include "mvh/crc32.h"
#include "mvh/mvhfs.h"

#define MVHFS_VERSION 1u
#define MVHFS_SECTOR_SIZE 512u
#define MVHFS_DIRECTORY_SECTORS 4u
#define MVHFS_DIRECTORY0_LBA 2u
#define MVHFS_DIRECTORY1_LBA 6u
#define MVHFS_DATA_LBA 10u
#define MVHFS_SECTORS_PER_COPY 8u
#define MVHFS_ENTRY_SIZE 64u
#define MVHFS_CLEAN_MARKER 0xC1EA4D56u

static const uint8_t mvhfs_magic[8] = {'M', 'V', 'H', 'F', 'S', '1', '\r', '\n'};

static uint32_t load32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static uint64_t load64(const uint8_t *data)
{
    return (uint64_t)load32(data) | ((uint64_t)load32(data + 4u) << 32u);
}

static void store32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void store64(uint8_t *data, uint64_t value)
{
    store32(data, (uint32_t)value);
    store32(data + 4u, (uint32_t)(value >> 32u));
}

static void clear_bytes(void *data, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)data;
    uint32_t index;
    for (index = 0u; index < size; index++) bytes[index] = 0u;
}

static void copy_bytes(void *destination, const void *source, uint32_t size)
{
    uint8_t *target = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    uint32_t index;
    for (index = 0u; index < size; index++) target[index] = input[index];
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, uint32_t size)
{
    uint32_t index;
    for (index = 0u; index < size; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static int device_info(uint32_t device_id, block_device_t *result)
{
    block_device_t devices[BLOCK_DEVICE_MAX];
    uint32_t count = block_list(devices, BLOCK_DEVICE_MAX);
    uint32_t index;
    for (index = 0u; index < count; index++) {
        if (devices[index].id == device_id) {
            *result = devices[index];
            return 0;
        }
    }
    return -1;
}

static uint64_t directory_lba(uint32_t slot)
{
    return slot == 0u ? MVHFS_DIRECTORY0_LBA : MVHFS_DIRECTORY1_LBA;
}

static uint64_t data_lba(uint32_t file, uint32_t slot)
{
    return MVHFS_DATA_LBA + (uint64_t)file * MVHFS_SECTORS_PER_COPY * 2u +
           (uint64_t)slot * MVHFS_SECTORS_PER_COPY;
}

static uint8_t *entry_at(mvhfs_t *filesystem, uint32_t index)
{
    return filesystem->directory + index * MVHFS_ENTRY_SIZE;
}

static const uint8_t *const_entry_at(const mvhfs_t *filesystem, uint32_t index)
{
    return filesystem->directory + index * MVHFS_ENTRY_SIZE;
}

static int valid_name(const char *name)
{
    uint32_t length = 0u;
    if (name == 0 || name[0] == '\0') return 0;
    while (name[length] != '\0') {
        if (length + 1u >= MVHFS_NAME_MAX || name[length] == '/' || name[length] == '\\')
            return 0;
        length++;
    }
    return 1;
}

static int entry_name_equal(const uint8_t *entry, const char *name)
{
    uint32_t index = 0u;
    if (load32(entry + 32u) == 0u) return 0;
    while (index < MVHFS_NAME_MAX) {
        if (entry[index] != (uint8_t)name[index]) return 0;
        if (name[index] == '\0') return 1;
        index++;
    }
    return 0;
}

static int find_entry(const mvhfs_t *filesystem, const char *name)
{
    uint32_t index;
    for (index = 0u; index < MVHFS_MAX_FILES; index++) {
        if (entry_name_equal(const_entry_at(filesystem, index), name)) return (int)index;
    }
    return -1;
}

static int find_free(const mvhfs_t *filesystem)
{
    uint32_t index;
    for (index = 0u; index < MVHFS_MAX_FILES; index++) {
        if (load32(const_entry_at(filesystem, index) + 32u) == 0u) return (int)index;
    }
    return -1;
}

static void build_superblock(uint8_t *sector, uint64_t total_sectors,
                             uint64_t generation, uint32_t directory_slot,
                             uint32_t directory_crc)
{
    clear_bytes(sector, MVHFS_SECTOR_SIZE);
    copy_bytes(sector, mvhfs_magic, sizeof(mvhfs_magic));
    store32(sector + 8u, MVHFS_VERSION);
    store32(sector + 12u, MVHFS_SECTOR_SIZE);
    store64(sector + 16u, total_sectors);
    store64(sector + 24u, generation);
    store32(sector + 32u, directory_slot);
    store32(sector + 36u, MVHFS_MAX_FILES);
    store32(sector + 40u, directory_crc);
    store32(sector + 44u, MVHFS_CLEAN_MARKER);
    store32(sector + 52u, MVHFS_DATA_LBA);
    store32(sector + 56u, MVHFS_SECTORS_PER_COPY);
    store32(sector + 48u, crc32(sector, MVHFS_SECTOR_SIZE));
}

static int validate_superblock(uint8_t *sector, const block_device_t *device)
{
    uint32_t expected_crc;
    if (!bytes_equal(sector, mvhfs_magic, sizeof(mvhfs_magic)) ||
        load32(sector + 8u) != MVHFS_VERSION ||
        load32(sector + 12u) != MVHFS_SECTOR_SIZE ||
        load64(sector + 16u) > device->sector_count ||
        load64(sector + 16u) < MVHFS_MIN_SECTORS ||
        load32(sector + 32u) > 1u || load32(sector + 36u) != MVHFS_MAX_FILES ||
        load32(sector + 44u) != MVHFS_CLEAN_MARKER ||
        load32(sector + 52u) != MVHFS_DATA_LBA ||
        load32(sector + 56u) != MVHFS_SECTORS_PER_COPY) return -1;
    expected_crc = load32(sector + 48u);
    store32(sector + 48u, 0u);
    return crc32(sector, MVHFS_SECTOR_SIZE) == expected_crc ? 0 : -1;
}

static int read_directory(uint32_t device_id, uint32_t slot, uint8_t *directory,
                          uint32_t expected_crc)
{
    uint32_t sector;
    for (sector = 0u; sector < MVHFS_DIRECTORY_SECTORS; sector++) {
        if (block_read(device_id, directory_lba(slot) + sector, 1u,
                       directory + sector * MVHFS_SECTOR_SIZE) != 0) return -1;
    }
    return crc32(directory, MVHFS_DIRECTORY_BYTES) == expected_crc ? 0 : -1;
}

static int mount_from_superblock(mvhfs_t *filesystem, uint32_t device_id,
                                 uint32_t superblock_slot, const uint8_t *superblock)
{
    uint32_t slot = load32(superblock + 32u);
    if (read_directory(device_id, slot, filesystem->directory,
                       load32(superblock + 40u)) != 0) return -1;
    filesystem->device_id = device_id;
    filesystem->superblock_slot = superblock_slot;
    filesystem->directory_slot = slot;
    filesystem->generation = load64(superblock + 24u);
    filesystem->mounted = 1u;
    return 0;
}

static int commit_directory(mvhfs_t *filesystem)
{
    block_device_t device;
    uint8_t superblock[MVHFS_SECTOR_SIZE];
    uint32_t new_directory_slot = filesystem->directory_slot ^ 1u;
    uint32_t new_superblock_slot = filesystem->superblock_slot ^ 1u;
    uint32_t sector;
    if (device_info(filesystem->device_id, &device) != 0) return -1;
    for (sector = 0u; sector < MVHFS_DIRECTORY_SECTORS; sector++) {
        if (block_write(filesystem->device_id, directory_lba(new_directory_slot) + sector,
                        1u, filesystem->directory + sector * MVHFS_SECTOR_SIZE) != 0)
            return -1;
    }
    build_superblock(superblock, device.sector_count, filesystem->generation + 1u,
                     new_directory_slot, crc32(filesystem->directory, MVHFS_DIRECTORY_BYTES));
    if (block_write(filesystem->device_id, new_superblock_slot, 1u, superblock) != 0)
        return -1;
    filesystem->directory_slot = new_directory_slot;
    filesystem->superblock_slot = new_superblock_slot;
    filesystem->generation++;
    return 0;
}

int mvhfs_format(uint32_t device_id)
{
    block_device_t device;
    uint8_t sector[MVHFS_SECTOR_SIZE];
    uint32_t zero_crc = 0u;
    uint32_t index;
    if (device_info(device_id, &device) != 0 || device.sector_size != MVHFS_SECTOR_SIZE ||
        device.sector_count < MVHFS_MIN_SECTORS || device.writable == 0u) return -1;
    clear_bytes(sector, sizeof(sector));
    for (index = 0u; index < MVHFS_DIRECTORY_SECTORS; index++)
        zero_crc = crc32_update(zero_crc, sector, sizeof(sector));
    for (index = 0u; index < MVHFS_DIRECTORY_SECTORS * 2u; index++) {
        if (block_write(device_id, MVHFS_DIRECTORY0_LBA + index, 1u, sector) != 0) return -1;
    }
    build_superblock(sector, device.sector_count, 0u, 1u, zero_crc);
    if (block_write(device_id, 1u, 1u, sector) != 0) return -1;
    build_superblock(sector, device.sector_count, 1u, 0u, zero_crc);
    return block_write(device_id, 0u, 1u, sector);
}

int mvhfs_mount(mvhfs_t *filesystem, uint32_t device_id)
{
    block_device_t device;
    uint8_t first[MVHFS_SECTOR_SIZE];
    uint8_t second[MVHFS_SECTOR_SIZE];
    int first_valid;
    int second_valid;
    if (filesystem == 0 || device_info(device_id, &device) != 0 ||
        device.sector_size != MVHFS_SECTOR_SIZE || device.sector_count < MVHFS_MIN_SECTORS)
        return -1;
    filesystem->mounted = 0u;
    first_valid = block_read(device_id, 0u, 1u, first) == 0 &&
                  validate_superblock(first, &device) == 0;
    second_valid = block_read(device_id, 1u, 1u, second) == 0 &&
                   validate_superblock(second, &device) == 0;
    if (!first_valid && !second_valid) return -1;
    if (first_valid && (!second_valid || load64(first + 24u) >= load64(second + 24u))) {
        if (mount_from_superblock(filesystem, device_id, 0u, first) == 0) return 0;
        if (second_valid) return mount_from_superblock(filesystem, device_id, 1u, second);
    } else {
        if (mount_from_superblock(filesystem, device_id, 1u, second) == 0) return 0;
        if (first_valid) return mount_from_superblock(filesystem, device_id, 0u, first);
    }
    return -1;
}

int mvhfs_create(mvhfs_t *filesystem, const char *name)
{
    uint8_t backup[MVHFS_ENTRY_SIZE];
    uint8_t *entry;
    uint32_t index;
    int slot;
    if (filesystem == 0 || filesystem->mounted == 0u || !valid_name(name) ||
        find_entry(filesystem, name) >= 0) return -1;
    slot = find_free(filesystem);
    if (slot < 0) return -1;
    entry = entry_at(filesystem, (uint32_t)slot);
    copy_bytes(backup, entry, sizeof(backup));
    clear_bytes(entry, MVHFS_ENTRY_SIZE);
    for (index = 0u; name[index] != '\0'; index++) entry[index] = (uint8_t)name[index];
    store32(entry + 32u, 1u);
    store32(entry + 36u, 0u);
    store32(entry + 40u, 0u);
    store32(entry + 44u, 0u);
    if (commit_directory(filesystem) != 0) {
        copy_bytes(entry, backup, sizeof(backup));
        return -1;
    }
    return 0;
}

int mvhfs_write(mvhfs_t *filesystem, const char *name, const void *data, uint32_t size)
{
    uint8_t sector[MVHFS_SECTOR_SIZE];
    uint8_t backup[MVHFS_ENTRY_SIZE];
    const uint8_t *input = (const uint8_t *)data;
    uint8_t *entry;
    uint32_t active_slot;
    uint32_t new_slot;
    uint32_t sector_index;
    uint32_t offset = 0u;
    int file;
    if (filesystem == 0 || filesystem->mounted == 0u || !valid_name(name) ||
        size > MVHFS_FILE_MAX || (data == 0 && size != 0u)) return -1;
    file = find_entry(filesystem, name);
    if (file < 0) return -1;
    entry = entry_at(filesystem, (uint32_t)file);
    copy_bytes(backup, entry, sizeof(backup));
    active_slot = load32(entry + 44u);
    new_slot = active_slot ^ 1u;
    for (sector_index = 0u; sector_index < MVHFS_SECTORS_PER_COPY; sector_index++) {
        uint32_t chunk = size - offset > MVHFS_SECTOR_SIZE ? MVHFS_SECTOR_SIZE : size - offset;
        clear_bytes(sector, sizeof(sector));
        if (chunk != 0u) copy_bytes(sector, input + offset, chunk);
        if (block_write(filesystem->device_id,
                        data_lba((uint32_t)file, new_slot) + sector_index,
                        1u, sector) != 0) return -1;
        offset += chunk;
    }
    store32(entry + 36u, size);
    store32(entry + 40u, crc32(data, size));
    store32(entry + 44u, new_slot);
    if (commit_directory(filesystem) != 0) {
        copy_bytes(entry, backup, sizeof(backup));
        return -1;
    }
    return 0;
}

int mvhfs_read(mvhfs_t *filesystem, const char *name, void *buffer,
               uint32_t capacity, uint32_t *size)
{
    uint8_t sector[MVHFS_SECTOR_SIZE];
    uint8_t *output = (uint8_t *)buffer;
    const uint8_t *entry;
    uint32_t file_size;
    uint32_t active_slot;
    uint32_t sector_index;
    uint32_t offset = 0u;
    uint32_t checksum = 0u;
    int file;
    if (filesystem == 0 || filesystem->mounted == 0u || !valid_name(name) || size == 0)
        return -1;
    file = find_entry(filesystem, name);
    if (file < 0) return -1;
    entry = const_entry_at(filesystem, (uint32_t)file);
    file_size = load32(entry + 36u);
    *size = file_size;
    if (file_size > MVHFS_FILE_MAX || capacity < file_size || (buffer == 0 && file_size != 0u))
        return -2;
    active_slot = load32(entry + 44u);
    if (active_slot > 1u) return -3;
    for (sector_index = 0u; offset < file_size; sector_index++) {
        uint32_t chunk = file_size - offset > MVHFS_SECTOR_SIZE ? MVHFS_SECTOR_SIZE : file_size - offset;
        if (block_read(filesystem->device_id,
                       data_lba((uint32_t)file, active_slot) + sector_index,
                       1u, sector) != 0) return -1;
        checksum = crc32_update(checksum, sector, chunk);
        copy_bytes(output + offset, sector, chunk);
        offset += chunk;
    }
    return checksum == load32(entry + 40u) ? 0 : -3;
}

int mvhfs_remove(mvhfs_t *filesystem, const char *name)
{
    uint8_t backup[MVHFS_ENTRY_SIZE];
    uint8_t *entry;
    int file;
    if (filesystem == 0 || filesystem->mounted == 0u || !valid_name(name)) return -1;
    file = find_entry(filesystem, name);
    if (file < 0) return -1;
    entry = entry_at(filesystem, (uint32_t)file);
    copy_bytes(backup, entry, sizeof(backup));
    clear_bytes(entry, MVHFS_ENTRY_SIZE);
    if (commit_directory(filesystem) != 0) {
        copy_bytes(entry, backup, sizeof(backup));
        return -1;
    }
    return 0;
}

uint32_t mvhfs_list(const mvhfs_t *filesystem, mvhfs_entry_t *entries,
                    uint32_t capacity)
{
    uint32_t index;
    uint32_t count = 0u;
    if (filesystem == 0 || filesystem->mounted == 0u) return 0u;
    for (index = 0u; index < MVHFS_MAX_FILES; index++) {
        const uint8_t *entry = const_entry_at(filesystem, index);
        uint32_t character;
        if (load32(entry + 32u) == 0u) continue;
        if (entries != 0 && count < capacity) {
            for (character = 0u; character < MVHFS_NAME_MAX; character++)
                entries[count].name[character] = (char)entry[character];
            entries[count].size = load32(entry + 36u);
        }
        count++;
    }
    return count;
}

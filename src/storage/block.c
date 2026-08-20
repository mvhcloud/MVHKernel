#include <stdint.h>
#include "mvh/block.h"
#include "mvh/crc32.h"
#include "mvh/sync.h"

static block_device_t block_devices[BLOCK_DEVICE_MAX];
static uint32_t block_device_count;
static spinlock_t block_lock;
static uint8_t test_disk[4096];

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

static void copy_name(char *target, const char *source)
{
    uint32_t index = 0u;
    while (source[index] != '\0' && index + 1u < BLOCK_NAME_MAX) {
        target[index] = source[index];
        index++;
    }
    target[index] = '\0';
}

void block_init(void)
{
    block_device_count = 0u;
    spinlock_init(&block_lock);
}

int block_register(const char *name, uint32_t sector_size, uint64_t sector_count,
                   uint8_t writable, void *context, block_read_fn read,
                   block_write_fn write)
{
    block_device_t *device;
    if (name == 0 || name[0] == '\0' || read == 0 || sector_size < 512u ||
        (sector_size & (sector_size - 1u)) != 0u || sector_count == 0u ||
        (writable != 0u && write == 0)) return -1;
    spinlock_lock(&block_lock);
    if (block_device_count >= BLOCK_DEVICE_MAX) {
        spinlock_unlock(&block_lock);
        return -1;
    }
    device = &block_devices[block_device_count];
    device->id = block_device_count;
    copy_name(device->name, name);
    device->sector_size = sector_size;
    device->sector_count = sector_count;
    device->writable = writable;
    device->context = context;
    device->read = read;
    device->write = write;
    block_device_count++;
    spinlock_unlock(&block_lock);
    return (int)device->id;
}

uint32_t block_count(void)
{
    uint32_t count;
    spinlock_lock(&block_lock);
    count = block_device_count;
    spinlock_unlock(&block_lock);
    return count;
}

uint32_t block_list(block_device_t *devices, uint32_t capacity)
{
    uint32_t count;
    uint32_t index;
    if (devices == 0 || capacity == 0u) return 0u;
    spinlock_lock(&block_lock);
    count = block_device_count < capacity ? block_device_count : capacity;
    for (index = 0u; index < count; index++) devices[index] = block_devices[index];
    spinlock_unlock(&block_lock);
    return count;
}

static int block_get(uint32_t id, block_device_t *device)
{
    spinlock_lock(&block_lock);
    if (id >= block_device_count) {
        spinlock_unlock(&block_lock);
        return -1;
    }
    *device = block_devices[id];
    spinlock_unlock(&block_lock);
    return 0;
}

int block_read(uint32_t id, uint64_t lba, uint32_t count, void *buffer)
{
    block_device_t device;
    if (count == 0u || buffer == 0 || block_get(id, &device) != 0) return -1;
    if (lba >= device.sector_count || count > device.sector_count - lba) return -1;
    return device.read(device.context, lba, count, buffer);
}

int block_write(uint32_t id, uint64_t lba, uint32_t count, const void *buffer)
{
    block_device_t device;
    if (count == 0u || buffer == 0 || block_get(id, &device) != 0) return -1;
    if (device.writable == 0u || device.write == 0 || lba >= device.sector_count ||
        count > device.sector_count - lba) return -1;
    return device.write(device.context, lba, count, buffer);
}

static int gpt_probe(uint32_t device_id, const block_device_t *device,
                     uint8_t *header, partition_info_t *info)
{
    uint8_t sector[512];
    uint32_t header_size = load32(header + 12u);
    uint32_t expected_header_crc = load32(header + 16u);
    uint32_t entry_count = load32(header + 80u);
    uint32_t entry_size = load32(header + 84u);
    uint32_t expected_entries_crc = load32(header + 88u);
    uint64_t entry_lba = load64(header + 72u);
    uint64_t entry_bytes;
    uint64_t entry_sectors;
    uint64_t sector_index;
    uint64_t remaining;
    uint32_t entries_crc = 0u;
    uint32_t populated = 0u;
    if (load32(header + 8u) != 0x00010000u ||
        header_size < 92u || header_size > 512u || load32(header + 20u) != 0u ||
        load64(header + 24u) != 1u || load64(header + 32u) >= device->sector_count ||
        entry_count == 0u || entry_count > 128u || entry_size < 128u ||
        entry_size > 512u || (entry_size & 127u) != 0u ||
        (512u % entry_size) != 0u) return -2;
    info->first_usable_lba = load64(header + 40u);
    info->last_usable_lba = load64(header + 48u);
    if (info->first_usable_lba > info->last_usable_lba ||
        info->last_usable_lba >= device->sector_count ||
        load64(header + 32u) <= info->last_usable_lba) return -2;
    store32(header + 16u, 0u);
    if (crc32(header, header_size) != expected_header_crc) return -2;
    info->gpt_header_valid = 1u;
    entry_bytes = (uint64_t)entry_count * entry_size;
    entry_sectors = (entry_bytes + 511u) / 512u;
    if (entry_lba <= 1u || entry_lba >= info->first_usable_lba ||
        entry_lba >= device->sector_count ||
        entry_sectors > device->sector_count - entry_lba ||
        entry_sectors > info->first_usable_lba - entry_lba) return -2;
    remaining = entry_bytes;
    for (sector_index = 0u; sector_index < entry_sectors; sector_index++) {
        uint32_t chunk = remaining > 512u ? 512u : (uint32_t)remaining;
        uint32_t offset;
        if (block_read(device_id, entry_lba + sector_index, 1u, sector) != 0) return -2;
        entries_crc = crc32_update(entries_crc, sector, chunk);
        for (offset = 0u; offset + entry_size <= chunk; offset += entry_size) {
            uint32_t byte;
            uint8_t populated_entry = 0u;
            for (byte = 0u; byte < 16u; byte++) populated_entry |= sector[offset + byte];
            if (populated_entry != 0u) populated++;
        }
        remaining -= chunk;
    }
    if (entries_crc != expected_entries_crc) return -2;
    info->gpt_entries_valid = 1u;
    info->type = PARTITION_TABLE_GPT;
    info->partition_count = populated;
    return 0;
}

int partition_probe(uint32_t device_id, partition_info_t *info)
{
    uint8_t sector[512];
    uint32_t index;
    uint32_t count = 0u;
    uint8_t protective = 0u;
    block_device_t device;
    if (info == 0 || block_get(device_id, &device) != 0 ||
        device.sector_size != 512u ||
        block_read(device_id, 0u, 1u, sector) != 0) return -1;
    info->type = PARTITION_TABLE_NONE;
    info->partition_count = 0u;
    info->protective_mbr = 0u;
    info->gpt_header_valid = 0u;
    info->gpt_entries_valid = 0u;
    info->first_usable_lba = 0u;
    info->last_usable_lba = 0u;
    if (sector[510] != 0x55u || sector[511] != 0xAAu) return 0;
    for (index = 0u; index < 4u; index++) {
        const uint8_t *entry = &sector[446u + index * 16u];
        if (entry[4] == 0xEEu) protective = 1u;
        if (entry[4] != 0u && load32(entry + 12u) != 0u) count++;
    }
    if (protective != 0u) {
        info->protective_mbr = 1u;
        if (block_read(device_id, 1u, 1u, sector) != 0 ||
            load64(sector) != 0x5452415020494645ull) return -2;
        return gpt_probe(device_id, &device, sector, info);
    }
    info->type = PARTITION_TABLE_MBR;
    info->partition_count = count;
    info->protective_mbr = protective;
    return 0;
}

const char *partition_table_name(partition_table_t type)
{
    if (type == PARTITION_TABLE_MBR) return "MBR";
    if (type == PARTITION_TABLE_GPT) return "GPT";
    return "none";
}

static int test_read(void *context, uint64_t lba, uint32_t count, void *buffer)
{
    uint8_t *target = (uint8_t *)buffer;
    uint32_t index;
    (void)context;
    if (lba >= 8u || count > 8u - lba) return -1;
    for (index = 0u; index < count * 512u; index++) {
        target[index] = test_disk[(uint32_t)lba * 512u + index];
    }
    return 0;
}

int block_self_test(void)
{
    partition_info_t info;
    uint8_t scratch[512];
    uint32_t index;
    int id;
    uint32_t before = block_count();
    int result = -1;
    for (index = 0u; index < sizeof(test_disk); index++) test_disk[index] = 0u;
    test_disk[446u + 4u] = 0xEEu;
    test_disk[446u + 12u] = 1u;
    test_disk[510] = 0x55u;
    test_disk[511] = 0xAAu;
    store64(test_disk + 512u, 0x5452415020494645ull);
    store32(test_disk + 512u + 8u, 0x00010000u);
    store32(test_disk + 512u + 12u, 92u);
    store64(test_disk + 512u + 24u, 1u);
    store64(test_disk + 512u + 32u, 7u);
    store64(test_disk + 512u + 40u, 3u);
    store64(test_disk + 512u + 48u, 6u);
    store64(test_disk + 512u + 72u, 2u);
    store32(test_disk + 512u + 80u, 4u);
    store32(test_disk + 512u + 84u, 128u);
    test_disk[1024u] = 0x28u;
    store32(test_disk + 512u + 88u, crc32(test_disk + 1024u, 512u));
    store32(test_disk + 512u + 16u, crc32(test_disk + 512u, 92u));
    id = block_register("selftest", 512u, 8u, 0u, 0, test_read, 0);
    if (id >= 0 && partition_probe((uint32_t)id, &info) == 0 &&
        info.type == PARTITION_TABLE_GPT && info.partition_count == 1u &&
        info.gpt_header_valid != 0u && info.gpt_entries_valid != 0u &&
        info.first_usable_lba == 3u && info.last_usable_lba == 6u &&
        info.protective_mbr != 0u && block_read((uint32_t)id, 8u, 1u, scratch) != 0 &&
        block_write((uint32_t)id, 0u, 1u, scratch) != 0) {
        test_disk[1024u + 16u] ^= 1u;
        if (partition_probe((uint32_t)id, &info) == -2) {
            test_disk[1024u + 16u] ^= 1u;
            test_disk[512u + 20u] = 1u;
            if (partition_probe((uint32_t)id, &info) == -2) result = 0;
            test_disk[512u + 20u] = 0u;
        } else {
            test_disk[1024u + 16u] ^= 1u;
        }
    }
    if (result == 0) {
        result = -1;
        test_disk[446u + 4u] = 0x83u;
        for (index = 0u; index < 8u; index++) test_disk[512u + index] = 0u;
        if (partition_probe((uint32_t)id, &info) == 0 &&
            info.type == PARTITION_TABLE_MBR && info.partition_count == 1u &&
            info.protective_mbr == 0u) result = 0;
    }
    spinlock_lock(&block_lock);
    block_device_count = before;
    spinlock_unlock(&block_lock);
    return result;
}

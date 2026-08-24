#include <stdint.h>
#include "mvh/bootinfo.h"
#include "mvh/smbios.h"

#define SMBIOS2_MIN_SIZE 31u
#define SMBIOS3_MIN_SIZE 24u

static smbios_info_t current;

static int bytes_equal(const uint8_t *left, const char *right, uint32_t length)
{
    uint32_t index;
    for (index = 0u; index < length; index++) {
        if (left[index] != (uint8_t)right[index]) return 0;
    }
    return 1;
}

static void clear_bytes(void *data, uint32_t length)
{
    uint8_t *bytes = (uint8_t *)data;
    uint32_t index;
    for (index = 0u; index < length; index++) bytes[index] = 0u;
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static uint64_t read_u64(const uint8_t *data)
{
    return (uint64_t)read_u32(data) | ((uint64_t)read_u32(data + 4u) << 32u);
}

static int checksum_valid(const uint8_t *data, uint32_t length)
{
    uint8_t sum = 0u;
    uint32_t index;
    for (index = 0u; index < length; index++) sum = (uint8_t)(sum + data[index]);
    return sum == 0u;
}

static int mapped_range(uint64_t address, uint32_t length)
{
    return address >= 0x1000u && length != 0u &&
           address < MVH_BOOTINFO_IDENTITY_LIMIT &&
           length <= MVH_BOOTINFO_IDENTITY_LIMIT - address;
}

static void copy_text(char *output, const uint8_t *text, uint32_t length)
{
    uint32_t count = length;
    uint32_t index;
    if (count >= SMBIOS_TEXT_MAX) count = SMBIOS_TEXT_MAX - 1u;
    for (index = 0u; index < count; index++) {
        uint8_t value = text[index];
        output[index] = value >= 0x20u && value <= 0x7Eu ? (char)value : '?';
    }
    output[count] = '\0';
}

static void copy_structure_string(char *output, const uint8_t *strings,
                                  const uint8_t *end, uint8_t wanted)
{
    uint8_t current_index = 1u;
    const uint8_t *cursor = strings;
    output[0] = '\0';
    if (wanted == 0u) return;
    while (cursor < end && *cursor != 0u) {
        const uint8_t *start = cursor;
        while (cursor < end && *cursor != 0u) cursor++;
        if (current_index == wanted) {
            copy_text(output, start, (uint32_t)(cursor - start));
            return;
        }
        if (cursor < end) cursor++;
        current_index++;
    }
}

int smbios_validate_entry_blob(const void *data, uint32_t size, smbios_info_t *result)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint8_t length;
    if (data == 0 || result == 0) return -1;
    clear_bytes(result, sizeof(*result));
    if (size >= SMBIOS3_MIN_SIZE && bytes_equal(bytes, "_SM3_", 5u)) {
        length = bytes[6u];
        if (length < SMBIOS3_MIN_SIZE || length > size || !checksum_valid(bytes, length))
            return -1;
        result->entry_point_64 = 1u;
        result->major = bytes[7u];
        result->minor = bytes[8u];
        result->document_revision = bytes[9u];
        result->table_size = read_u32(bytes + 12u);
        result->table_address = read_u64(bytes + 16u);
    } else if (size >= SMBIOS2_MIN_SIZE && bytes_equal(bytes, "_SM_", 4u)) {
        length = bytes[5u];
        if (length < SMBIOS2_MIN_SIZE || length > size ||
            !checksum_valid(bytes, length) || !bytes_equal(bytes + 16u, "_DMI_", 5u) ||
            !checksum_valid(bytes + 16u, 15u)) return -1;
        result->major = bytes[6u];
        result->minor = bytes[7u];
        result->table_size = read_u16(bytes + 22u);
        result->table_address = read_u32(bytes + 24u);
        result->announced_structures = read_u16(bytes + 28u);
    } else return -1;
    if (result->major == 0u || result->table_size == 0u ||
        result->table_size > SMBIOS_MAX_TABLE_SIZE || result->table_address == 0u) return -1;
    return 0;
}

static void parse_memory_device(const uint8_t *structure, uint8_t length,
                                smbios_info_t *result)
{
    uint16_t size;
    uint64_t size_mib = 0u;
    uint16_t speed = 0u;
    result->memory_device_structures++;
    if (length < 14u) return;
    size = read_u16(structure + 12u);
    if (size == 0u || size == 0xFFFFu) return;
    if (size == 0x7FFFu && length >= 32u) size_mib = read_u32(structure + 28u) & 0x7FFFFFFFu;
    else if ((size & 0x8000u) != 0u) size_mib = (uint64_t)(size & 0x7FFFu) / 1024u;
    else size_mib = size;
    result->populated_memory_devices++;
    if (result->installed_memory_mib <= UINT64_MAX - size_mib)
        result->installed_memory_mib += size_mib;
    if (length >= 23u) speed = read_u16(structure + 21u);
    if (speed > result->maximum_memory_speed_mhz) result->maximum_memory_speed_mhz = speed;
}

int smbios_parse_table_blob(const void *data, uint32_t size, uint32_t announced,
                            smbios_info_t *result)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t offset = 0u;
    uint8_t found_end = 0u;
    if (data == 0 || result == 0 || size < 6u || size > SMBIOS_MAX_TABLE_SIZE) return -1;
    while (offset + 4u <= size) {
        const uint8_t *structure = bytes + offset;
        const uint8_t *strings;
        const uint8_t *end = bytes + size;
        const uint8_t *cursor;
        uint8_t type = structure[0u];
        uint8_t length = structure[1u];
        if (length < 4u || length > size - offset) return -1;
        strings = structure + length;
        cursor = strings;
        while (cursor + 1u < end && !(cursor[0u] == 0u && cursor[1u] == 0u)) cursor++;
        if (cursor + 1u >= end) return -1;
        result->parsed_structures++;
        if (type == 0u) {
            result->bios_structures++;
            if (length >= 6u) {
                copy_structure_string(result->bios_vendor, strings, cursor, structure[4u]);
                copy_structure_string(result->bios_version, strings, cursor, structure[5u]);
            }
        } else if (type == 1u) {
            result->system_structures++;
            if (length >= 8u) {
                copy_structure_string(result->system_manufacturer, strings, cursor, structure[4u]);
                copy_structure_string(result->system_product, strings, cursor, structure[5u]);
                copy_structure_string(result->system_version, strings, cursor, structure[6u]);
                copy_structure_string(result->system_serial, strings, cursor, structure[7u]);
            }
        } else if (type == 2u) {
            result->baseboard_structures++;
            if (length >= 6u) {
                copy_structure_string(result->baseboard_manufacturer, strings, cursor, structure[4u]);
                copy_structure_string(result->baseboard_product, strings, cursor, structure[5u]);
            }
        } else if (type == 4u) result->processor_structures++;
        else if (type == 17u) parse_memory_device(structure, length, result);
        offset = (uint32_t)((cursor + 2u) - bytes);
        if (type == 127u) {
            found_end = 1u;
            break;
        }
        if (announced != 0u && result->parsed_structures > announced) return -1;
    }
    if (found_end == 0u && announced == 0u) return -1;
    if (announced != 0u && result->parsed_structures != announced) return -1;
    return 0;
}

int smbios_init(uint64_t entry_address)
{
    const uint8_t *entry;
    uint32_t entry_size;
    smbios_info_t parsed;
    clear_bytes(&current, sizeof(current));
    if (!mapped_range(entry_address, 16u)) return -1;
    entry = (const uint8_t *)(uintptr_t)entry_address;
    if (bytes_equal(entry, "_SM3_", 5u)) entry_size = entry[6u];
    else if (bytes_equal(entry, "_SM_", 4u)) entry_size = entry[5u];
    else return -1;
    if (!mapped_range(entry_address, entry_size) ||
        smbios_validate_entry_blob(entry, entry_size, &parsed) != 0 ||
        !mapped_range(parsed.table_address, parsed.table_size) ||
        smbios_parse_table_blob((const void *)(uintptr_t)parsed.table_address,
                                parsed.table_size, parsed.announced_structures, &parsed) != 0)
        return -1;
    parsed.available = 1u;
    current = parsed;
    return 0;
}

uint64_t smbios_discover_entry(void)
{
    uint64_t address;
    smbios_info_t parsed;
    for (address = 0xF0000u; address + 32u <= 0x100000u; address += 16u) {
        const uint8_t *entry = (const uint8_t *)(uintptr_t)address;
        uint32_t size;
        if (bytes_equal(entry, "_SM3_", 5u)) size = entry[6u];
        else if (bytes_equal(entry, "_SM_", 4u)) size = entry[5u];
        else continue;
        if (size >= 16u && size <= 64u && address + size <= 0x100000u &&
            smbios_validate_entry_blob(entry, size, &parsed) == 0) return address;
    }
    return 0u;
}

const smbios_info_t *smbios_info(void)
{
    return &current;
}

static void set_checksum(uint8_t *data, uint32_t length, uint32_t offset)
{
    uint8_t sum = 0u;
    uint32_t index;
    data[offset] = 0u;
    for (index = 0u; index < length; index++) sum = (uint8_t)(sum + data[index]);
    data[offset] = (uint8_t)(0u - sum);
}

int smbios_self_test(void)
{
    uint8_t entry[SMBIOS3_MIN_SIZE] = {0};
    uint8_t entry2[SMBIOS2_MIN_SIZE] = {0};
    uint8_t table[16] = {127u, 4u, 0u, 0u, 0u, 0u};
    smbios_info_t parsed;
    uint32_t index;
    for (index = 0u; index < 5u; index++) entry[index] = (uint8_t)"_SM3_"[index];
    entry[6u] = SMBIOS3_MIN_SIZE;
    entry[7u] = 3u;
    entry[8u] = 6u;
    entry[12u] = sizeof(table);
    entry[16u] = 0x00u;
    entry[17u] = 0x20u;
    set_checksum(entry, sizeof(entry), 5u);
    if (smbios_validate_entry_blob(entry, sizeof(entry), &parsed) != 0 ||
        parsed.major != 3u || parsed.table_size != sizeof(table)) return -1;
    if (smbios_parse_table_blob(table, sizeof(table), 0u, &parsed) != 0 ||
        parsed.parsed_structures != 1u) return -1;
    entry[5u]++;
    if (smbios_validate_entry_blob(entry, sizeof(entry), &parsed) == 0) return -1;
    for (index = 0u; index < 4u; index++) entry2[index] = (uint8_t)"_SM_"[index];
    for (index = 0u; index < 5u; index++) entry2[16u + index] = (uint8_t)"_DMI_"[index];
    entry2[5u] = SMBIOS2_MIN_SIZE;
    entry2[6u] = 2u;
    entry2[7u] = 8u;
    entry2[22u] = sizeof(table);
    entry2[24u] = 0x00u;
    entry2[25u] = 0x20u;
    entry2[28u] = 1u;
    set_checksum(entry2 + 16u, 15u, 5u);
    set_checksum(entry2, sizeof(entry2), 4u);
    if (smbios_validate_entry_blob(entry2, sizeof(entry2), &parsed) != 0 ||
        parsed.entry_point_64 != 0u || parsed.major != 2u ||
        parsed.announced_structures != 1u) return -1;
    table[1u] = 3u;
    return smbios_parse_table_blob(table, sizeof(table), 0u, &parsed) != 0 ? 0 : -1;
}

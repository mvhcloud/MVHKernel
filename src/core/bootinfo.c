#include <stdint.h>
#include "mvh/bootinfo.h"

#define BOOTINFO_MIN_MEMORY_KIB 4096ull
#define BOOTINFO_MAX_MEMORY_KIB (UINT64_MAX / 1024ull)

static mvh_bootinfo_snapshot_t current;

static void clear_snapshot(void)
{
    uint32_t index;
    uint8_t *bytes = (uint8_t *)&current;
    for (index = 0u; index < sizeof(current); index++) bytes[index] = 0u;
}

static int range_is_mapped(uint64_t address, uint64_t size)
{
    return address >= 0x1000u && size != 0u && address < MVH_BOOTINFO_IDENTITY_LIMIT &&
           size <= MVH_BOOTINFO_IDENTITY_LIMIT - address;
}

static int optional_range(uint64_t flags, uint64_t flag, uint64_t address, uint64_t size)
{
    if ((flags & flag) == 0u) return address == 0u;
    return range_is_mapped(address, size);
}

static int validate_v2(const mvh_bootinfo_v2_t *info)
{
    const uint8_t *map;
    uint64_t map_size;
    uint32_t index;
    if (info->magic != MVH_BOOTINFO_MAGIC || info->version != MVH_BOOTINFO_VERSION ||
        info->size < sizeof(mvh_bootinfo_v2_t) || info->size > MVH_BOOTINFO_MAX_SIZE ||
        (info->flags & ~MVH_BOOTINFO_KNOWN_FLAGS) != 0u ||
        info->memory_kib < BOOTINFO_MIN_MEMORY_KIB ||
        info->memory_kib > BOOTINFO_MAX_MEMORY_KIB || info->reserved0 != 0u ||
        info->reserved1 != 0u) return -1;
    if ((info->flags & MVH_BOOTINFO_FLAG_MEMORY_MAP) != 0u) {
        if (info->memory_map_entries == 0u || (info->memory_map_address & 7u) != 0u ||
            info->memory_map_entry_size < sizeof(mvh_memory_map_entry_t) ||
            (info->memory_map_entry_size & 7u) != 0u ||
            info->memory_map_entries > 65536u ||
            info->memory_map_entry_size > MVH_BOOTINFO_MAX_SIZE) return -1;
        map_size = (uint64_t)info->memory_map_entries * info->memory_map_entry_size;
        if (!range_is_mapped(info->memory_map_address, map_size)) return -1;
        map = (const uint8_t *)(uintptr_t)info->memory_map_address;
        for (index = 0u; index < info->memory_map_entries; index++) {
            const mvh_memory_map_entry_t *entry =
                (const mvh_memory_map_entry_t *)(const void *)(map +
                    (uint64_t)index * info->memory_map_entry_size);
            if (entry->length == 0u || entry->base > UINT64_MAX - entry->length ||
                entry->type < MVH_MEMORY_USABLE || entry->type > MVH_MEMORY_MMIO)
                return -1;
        }
    } else if (info->memory_map_address != 0u || info->memory_map_entries != 0u ||
               info->memory_map_entry_size != 0u) return -1;
    if (!optional_range(info->flags, MVH_BOOTINFO_FLAG_ACPI_RSDP,
                        info->acpi_rsdp_address, 20u) ||
        !optional_range(info->flags, MVH_BOOTINFO_FLAG_SMBIOS,
                        info->smbios_address, 16u)) return -1;
    if ((info->flags & MVH_BOOTINFO_FLAG_FRAMEBUFFER) != 0u) {
        if (info->framebuffer.address == 0u || info->framebuffer.width == 0u ||
            info->framebuffer.height == 0u || info->framebuffer.pitch == 0u ||
            (info->framebuffer.bits_per_pixel != 24u &&
             info->framebuffer.bits_per_pixel != 32u)) return -1;
    } else if (info->framebuffer.address != 0u || info->framebuffer.width != 0u ||
               info->framebuffer.height != 0u || info->framebuffer.pitch != 0u ||
               info->framebuffer.bits_per_pixel != 0u || info->framebuffer.red_mask != 0u ||
               info->framebuffer.green_mask != 0u || info->framebuffer.blue_mask != 0u ||
               info->framebuffer.reserved != 0u) return -1;
    if (!optional_range(info->flags, MVH_BOOTINFO_FLAG_RANDOM_SEED,
                        info->random_seed_address, info->random_seed_size) ||
        info->random_seed_size > 256u ||
        ((info->flags & MVH_BOOTINFO_FLAG_RANDOM_SEED) == 0u &&
         info->random_seed_size != 0u) ||
        !optional_range(info->flags, MVH_BOOTINFO_FLAG_COMMAND_LINE,
                        info->command_line_address, info->command_line_size) ||
        info->command_line_size > 4096u ||
        ((info->flags & MVH_BOOTINFO_FLAG_COMMAND_LINE) == 0u &&
         info->command_line_size != 0u)) return -1;
    return 0;
}

int bootinfo_capture(uint64_t argument0, uint64_t argument1)
{
    const mvh_bootinfo_v2_t *info;
    clear_snapshot();
    if (argument1 != MVH_BOOTINFO_HANDOFF_MAGIC) {
        if (argument0 < BOOTINFO_MIN_MEMORY_KIB || argument0 > BOOTINFO_MAX_MEMORY_KIB)
            return -1;
        current.memory_kib = argument0;
        return 0;
    }
    if ((argument0 & 7u) != 0u || !range_is_mapped(argument0, sizeof(*info))) return -1;
    info = (const mvh_bootinfo_v2_t *)(uintptr_t)argument0;
    if (validate_v2(info) != 0 || !range_is_mapped(argument0, info->size)) return -1;
    current.versioned = 1u;
    current.version = info->version;
    current.size = info->size;
    current.flags = info->flags;
    current.memory_kib = info->memory_kib;
    current.memory_map_address = info->memory_map_address;
    current.memory_map_entries = info->memory_map_entries;
    current.memory_map_entry_size = info->memory_map_entry_size;
    current.acpi_rsdp_address = info->acpi_rsdp_address;
    current.smbios_address = info->smbios_address;
    current.framebuffer = info->framebuffer;
    current.random_seed_address = info->random_seed_address;
    current.random_seed_size = info->random_seed_size;
    current.command_line_address = info->command_line_address;
    current.command_line_size = info->command_line_size;
    return 0;
}

const mvh_bootinfo_snapshot_t *bootinfo_current(void)
{
    return &current;
}

int bootinfo_self_test(void)
{
    mvh_bootinfo_v2_t info = {0};
    info.magic = MVH_BOOTINFO_MAGIC;
    info.version = MVH_BOOTINFO_VERSION;
    info.size = sizeof(info);
    info.memory_kib = 65536u;
    if (validate_v2(&info) != 0) return -1;
    info.flags = 1ull << 63u;
    if (validate_v2(&info) == 0) return -1;
    info.flags = 0u;
    info.size--;
    return validate_v2(&info) != 0 ? 0 : -1;
}

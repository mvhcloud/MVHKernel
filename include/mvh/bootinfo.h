#ifndef MVH_BOOTINFO_H
#define MVH_BOOTINFO_H

#include <stdint.h>

#define MVH_BOOTINFO_HANDOFF_MAGIC 0x32564F464E494856ull
#define MVH_BOOTINFO_MAGIC 0x324F464E4948564Dull
#define MVH_BOOTINFO_VERSION 2u
#define MVH_BOOTINFO_MAX_SIZE 4096u
#define MVH_BOOTINFO_IDENTITY_LIMIT 0x40000000ull

#define MVH_BOOTINFO_FLAG_MEMORY_MAP (1ull << 0u)
#define MVH_BOOTINFO_FLAG_ACPI_RSDP (1ull << 1u)
#define MVH_BOOTINFO_FLAG_SMBIOS (1ull << 2u)
#define MVH_BOOTINFO_FLAG_FRAMEBUFFER (1ull << 3u)
#define MVH_BOOTINFO_FLAG_RANDOM_SEED (1ull << 4u)
#define MVH_BOOTINFO_FLAG_COMMAND_LINE (1ull << 5u)
#define MVH_BOOTINFO_KNOWN_FLAGS ((1ull << 6u) - 1u)

typedef enum {
    MVH_MEMORY_USABLE = 1u,
    MVH_MEMORY_RESERVED = 2u,
    MVH_MEMORY_ACPI_RECLAIMABLE = 3u,
    MVH_MEMORY_ACPI_NVS = 4u,
    MVH_MEMORY_BAD = 5u,
    MVH_MEMORY_MMIO = 6u
} mvh_memory_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} mvh_memory_map_entry_t;

typedef struct {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bits_per_pixel;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved;
} mvh_framebuffer_info_t;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t size;
    uint64_t flags;
    uint64_t memory_kib;
    uint64_t memory_map_address;
    uint32_t memory_map_entries;
    uint32_t memory_map_entry_size;
    uint64_t acpi_rsdp_address;
    uint64_t smbios_address;
    mvh_framebuffer_info_t framebuffer;
    uint64_t random_seed_address;
    uint32_t random_seed_size;
    uint32_t reserved0;
    uint64_t command_line_address;
    uint32_t command_line_size;
    uint32_t reserved1;
} mvh_bootinfo_v2_t;

typedef struct {
    uint8_t versioned;
    uint32_t version;
    uint32_t size;
    uint64_t flags;
    uint64_t memory_kib;
    uint64_t memory_map_address;
    uint32_t memory_map_entries;
    uint32_t memory_map_entry_size;
    uint64_t acpi_rsdp_address;
    uint64_t smbios_address;
    mvh_framebuffer_info_t framebuffer;
    uint64_t random_seed_address;
    uint32_t random_seed_size;
    uint64_t command_line_address;
    uint32_t command_line_size;
} mvh_bootinfo_snapshot_t;

_Static_assert(sizeof(mvh_memory_map_entry_t) == 24u, "BootInfo memory entry ABI changed");
_Static_assert(sizeof(mvh_framebuffer_info_t) == 40u, "BootInfo framebuffer ABI changed");
_Static_assert(sizeof(mvh_bootinfo_v2_t) == 136u, "BootInfo V2 ABI changed");

int bootinfo_capture(uint64_t argument0, uint64_t argument1);
const mvh_bootinfo_snapshot_t *bootinfo_current(void);
int bootinfo_self_test(void);

#endif

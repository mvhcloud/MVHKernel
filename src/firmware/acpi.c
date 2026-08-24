#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/bootinfo.h"

#define RSDP_V1_SIZE 20u
#define RSDP_V2_SIZE 36u
#define SDT_HEADER_SIZE 36u

typedef struct __attribute__((packed)) {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} acpi_rsdp_t;

typedef struct {
    const acpi_sdt_header_t *header;
    uint64_t address;
} registry_entry_t;

static registry_entry_t registry[ACPI_MAX_TABLES];
static acpi_cpu_info_t cpus[ACPI_MAX_CPUS];
static acpi_ioapic_info_t ioapics[ACPI_MAX_IOAPICS];
static acpi_interrupt_override_t overrides[ACPI_MAX_OVERRIDES];
static acpi_mcfg_segment_t mcfg_segments[ACPI_MAX_MCFG_SEGMENTS];
static uint32_t stored_cpus;
static uint32_t stored_ioapics;
static uint32_t stored_overrides;
static uint32_t stored_mcfg_segments;
static acpi_status_t state;

static int bytes_equal(const char *left, const char *right, uint32_t length)
{
    uint32_t index;
    for (index = 0u; index < length; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static void clear_bytes(void *data, uint32_t length)
{
    uint8_t *bytes = (uint8_t *)data;
    uint32_t index;
    for (index = 0u; index < length; index++) bytes[index] = 0u;
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
}

static uint64_t read_u64(const uint8_t *data)
{
    return (uint64_t)read_u32(data) | ((uint64_t)read_u32(data + 4u) << 32u);
}

static int mapped_range(uint64_t address, uint32_t length)
{
    uint64_t limit = (bootinfo_current()->flags & MVH_BOOTINFO_FLAG_IDENTITY_4G) != 0u ?
                     0x100000000ull : MVH_BOOTINFO_IDENTITY_LIMIT;
    return address >= 0x1000u && length != 0u &&
           address < limit && length <= limit - address;
}

static uint64_t scan_rsdp_range(uint64_t start, uint64_t end)
{
    uint64_t address;
    start = (start + 15u) & ~15ull;
    for (address = start; address + RSDP_V1_SIZE <= end; address += 16u) {
        const acpi_rsdp_t *candidate = (const acpi_rsdp_t *)(uintptr_t)address;
        uint32_t length = RSDP_V1_SIZE;
        if (!bytes_equal(candidate->signature, "RSD PTR ", 8u) ||
            !acpi_checksum_valid(candidate, RSDP_V1_SIZE)) continue;
        if (candidate->revision >= 2u) {
            length = candidate->length;
            if (length < RSDP_V2_SIZE || length > 4096u || address + length > end ||
                !acpi_checksum_valid(candidate, length)) continue;
        }
        return address;
    }
    return 0u;
}

uint64_t acpi_discover_rsdp(void)
{
    uint16_t ebda_segment;
    __asm__ volatile ("movw 0x40e, %0" : "=r"(ebda_segment));
    uint64_t ebda = (uint64_t)ebda_segment << 4u;
    uint64_t result = 0u;
    if (ebda >= 0x80000u && ebda < 0xA0000u)
        result = scan_rsdp_range(ebda, ebda + 1024u);
    if (result == 0u) result = scan_rsdp_range(0xE0000u, 0x100000u);
    return result;
}

int acpi_checksum_valid(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint8_t sum = 0u;
    uint32_t index;
    if (data == 0 || length == 0u) return 0;
    for (index = 0u; index < length; index++) sum = (uint8_t)(sum + bytes[index]);
    return sum == 0u;
}

int acpi_validate_rsdp_blob(const void *data, uint32_t size, acpi_rsdp_info_t *result)
{
    const acpi_rsdp_t *rsdp = (const acpi_rsdp_t *)data;
    uint32_t index;
    if (data == 0 || result == 0 || size < RSDP_V1_SIZE ||
        !bytes_equal(rsdp->signature, "RSD PTR ", 8u) ||
        !acpi_checksum_valid(data, RSDP_V1_SIZE)) return -1;
    clear_bytes(result, sizeof(*result));
    result->revision = rsdp->revision;
    for (index = 0u; index < 6u; index++) result->oem_id[index] = rsdp->oem_id[index];
    if (rsdp->revision >= 2u) {
        if (size < RSDP_V2_SIZE || rsdp->length < RSDP_V2_SIZE ||
            rsdp->length > size || !acpi_checksum_valid(data, rsdp->length)) return -1;
        if (rsdp->xsdt_address != 0u) {
            result->root_address = rsdp->xsdt_address;
            result->uses_xsdt = 1u;
        } else result->root_address = rsdp->rsdt_address;
    } else result->root_address = rsdp->rsdt_address;
    return result->root_address != 0u ? 0 : -1;
}

int acpi_validate_sdt_blob(const void *data, uint32_t available, const char *signature)
{
    const acpi_sdt_header_t *header = (const acpi_sdt_header_t *)data;
    if (data == 0 || available < sizeof(*header) ||
        header->length < sizeof(*header) || header->length > available ||
        header->length > ACPI_MAX_TABLE_SIZE) return -1;
    if (signature != 0 && !bytes_equal(header->signature, signature, 4u)) return -1;
    return acpi_checksum_valid(data, header->length) ? 0 : -1;
}

static int known_signature(const char *signature)
{
    static const char known[][5] = {
        "APIC", "FACP", "HPET", "MCFG", "SRAT", "SLIT",
        "DMAR", "IVRS", "SPCR", "TPM2"
    };
    uint32_t index;
    for (index = 0u; index < sizeof(known) / sizeof(known[0]); index++) {
        if (bytes_equal(signature, known[index], 4u)) return 1;
    }
    return 0;
}

static int parse_madt(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    uint32_t offset = 44u;
    if (header->length < offset) return -1;
    state.lapic_address = read_u32(data + 36u);
    state.lapic_flags = read_u32(data + 40u);
    while (offset + 2u <= header->length) {
        uint8_t type = data[offset];
        uint8_t length = data[offset + 1u];
        if (length < 2u || length > header->length - offset) return -1;
        if (type == 0u && length >= 8u) {
            state.local_apics++;
            if (stored_cpus < ACPI_MAX_CPUS) {
                cpus[stored_cpus].processor_uid = data[offset + 2u];
                cpus[stored_cpus].apic_id = data[offset + 3u];
                cpus[stored_cpus].enabled = (data[offset + 4u] & 1u) != 0u;
                cpus[stored_cpus].online_capable = (data[offset + 4u] & 2u) != 0u;
                cpus[stored_cpus].x2apic = 0u;
                stored_cpus++;
            }
        } else if (type == 1u && length >= 12u) {
            state.ioapics++;
            if (stored_ioapics < ACPI_MAX_IOAPICS) {
                ioapics[stored_ioapics].id = data[offset + 2u];
                ioapics[stored_ioapics].address = read_u32(data + offset + 4u);
                ioapics[stored_ioapics].global_interrupt_base = read_u32(data + offset + 8u);
                stored_ioapics++;
            }
        } else if (type == 2u && length >= 10u) {
            state.interrupt_overrides++;
            if (stored_overrides < ACPI_MAX_OVERRIDES) {
                overrides[stored_overrides].bus = data[offset + 2u];
                overrides[stored_overrides].source_irq = data[offset + 3u];
                overrides[stored_overrides].global_interrupt = read_u32(data + offset + 4u);
                overrides[stored_overrides].flags = read_u16(data + offset + 8u);
                stored_overrides++;
            }
        }
        else if (type == 3u && length >= 8u) state.nmi_sources++;
        else if (type == 4u && length >= 6u) state.local_apic_nmis++;
        else if (type == 5u && length >= 12u) state.lapic_address = read_u64(data + offset + 4u);
        else if (type == 9u && length >= 16u) {
            state.local_x2apics++;
            if (stored_cpus < ACPI_MAX_CPUS) {
                cpus[stored_cpus].apic_id = read_u32(data + offset + 4u);
                cpus[stored_cpus].enabled = (data[offset + 8u] & 1u) != 0u;
                cpus[stored_cpus].online_capable = (data[offset + 8u] & 2u) != 0u;
                cpus[stored_cpus].processor_uid = read_u32(data + offset + 12u);
                cpus[stored_cpus].x2apic = 1u;
                stored_cpus++;
            }
        }
        else if (type == 10u && length >= 12u) state.local_apic_nmis++;
        offset += length;
    }
    return offset == header->length ? 0 : -1;
}

static int parse_fadt(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    if (header->length < 80u) return -1;
    state.pm_timer_port = read_u32(data + 76u);
    if (header->length >= 129u) {
        state.reset_register.address_space = data[116u];
        state.reset_register.bit_width = data[117u];
        state.reset_register.bit_offset = data[118u];
        state.reset_register.access_size = data[119u];
        state.reset_register.address = read_u64(data + 120u);
        state.reset_value = data[128u];
    }
    return 0;
}

static int parse_hpet(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    if (header->length < 56u) return -1;
    state.hpet_address = read_u64(data + 44u);
    return data[40u] == 0u && state.hpet_address != 0u ? 0 : -1;
}

static int parse_mcfg(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    uint32_t offset;
    if (header->length < 44u || ((header->length - 44u) % 16u) != 0u) return -1;
    for (offset = 44u; offset < header->length; offset += 16u) {
        uint64_t base = read_u64(data + offset);
        uint16_t segment = read_u16(data + offset + 8u);
        uint8_t start_bus = data[offset + 10u];
        uint8_t end_bus = data[offset + 11u];
        if (base == 0u || (base & ((1ull << 20u) - 1u)) != 0u || start_bus > end_bus)
            return -1;
        state.mcfg_segments++;
        if (stored_mcfg_segments < ACPI_MAX_MCFG_SEGMENTS) {
            mcfg_segments[stored_mcfg_segments].base_address = base;
            mcfg_segments[stored_mcfg_segments].segment_group = segment;
            mcfg_segments[stored_mcfg_segments].start_bus = start_bus;
            mcfg_segments[stored_mcfg_segments].end_bus = end_bus;
            stored_mcfg_segments++;
        }
    }
    return 0;
}

static int parse_srat(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    uint32_t offset = 48u;
    if (header->length < offset) return -1;
    while (offset + 2u <= header->length) {
        uint8_t type = data[offset];
        uint8_t length = data[offset + 1u];
        if (length < 2u || length > header->length - offset) return -1;
        if ((type == 0u && length >= 16u) || (type == 2u && length >= 24u))
            state.srat_cpu_affinities++;
        else if (type == 1u && length >= 40u) state.srat_memory_affinities++;
        offset += length;
    }
    return offset == header->length ? 0 : -1;
}

static int parse_slit(const acpi_sdt_header_t *header)
{
    const uint8_t *data = (const uint8_t *)header;
    uint64_t count;
    uint64_t matrix_size;
    if (header->length < 44u) return -1;
    count = read_u64(data + 36u);
    if (count == 0u || count > 4096u || count > UINT64_MAX / count) return -1;
    matrix_size = count * count;
    if (matrix_size != (uint64_t)header->length - 44u) return -1;
    state.slit_localities = count;
    return 0;
}

static int parse_known(const acpi_sdt_header_t *header)
{
    if (bytes_equal(header->signature, "APIC", 4u)) return parse_madt(header);
    if (bytes_equal(header->signature, "FACP", 4u)) return parse_fadt(header);
    if (bytes_equal(header->signature, "HPET", 4u)) return parse_hpet(header);
    if (bytes_equal(header->signature, "MCFG", 4u)) return parse_mcfg(header);
    if (bytes_equal(header->signature, "SRAT", 4u)) return parse_srat(header);
    if (bytes_equal(header->signature, "SLIT", 4u)) return parse_slit(header);
    return 0;
}

static int register_table(uint64_t address)
{
    const acpi_sdt_header_t *header;
    acpi_status_t before;
    uint32_t before_cpus;
    uint32_t before_ioapics;
    uint32_t before_overrides;
    uint32_t before_mcfg_segments;
    if (!mapped_range(address, sizeof(acpi_sdt_header_t))) return -1;
    header = (const acpi_sdt_header_t *)(uintptr_t)address;
    if (!mapped_range(address, header->length) ||
        acpi_validate_sdt_blob(header, header->length, 0) != 0) return -1;
    if (state.table_count >= ACPI_MAX_TABLES) return -1;
    before = state;
    before_cpus = stored_cpus;
    before_ioapics = stored_ioapics;
    before_overrides = stored_overrides;
    before_mcfg_segments = stored_mcfg_segments;
    if (known_signature(header->signature)) {
        if (parse_known(header) != 0) {
            state = before;
            stored_cpus = before_cpus;
            stored_ioapics = before_ioapics;
            stored_overrides = before_overrides;
            stored_mcfg_segments = before_mcfg_segments;
            return -1;
        }
    } else state.unknown_tables++;
    registry[state.table_count].header = header;
    registry[state.table_count].address = address;
    state.table_count++;
    return 0;
}

int acpi_init(uint64_t rsdp_address)
{
    const acpi_rsdp_t *rsdp;
    const acpi_sdt_header_t *root;
    acpi_rsdp_info_t info;
    uint32_t entry_size;
    uint32_t entry_count;
    uint32_t index;
    clear_bytes(&state, sizeof(state));
    clear_bytes(registry, sizeof(registry));
    clear_bytes(cpus, sizeof(cpus));
    clear_bytes(ioapics, sizeof(ioapics));
    clear_bytes(overrides, sizeof(overrides));
    clear_bytes(mcfg_segments, sizeof(mcfg_segments));
    stored_cpus = 0u;
    stored_ioapics = 0u;
    stored_overrides = 0u;
    stored_mcfg_segments = 0u;
    uint32_t rsdp_size;
    if (!mapped_range(rsdp_address, RSDP_V1_SIZE)) return -1;
    rsdp = (const acpi_rsdp_t *)(uintptr_t)rsdp_address;
    rsdp_size = RSDP_V1_SIZE;
    if (rsdp->revision >= 2u) {
        if (!mapped_range(rsdp_address, RSDP_V2_SIZE) || rsdp->length < RSDP_V2_SIZE ||
            rsdp->length > 4096u || !mapped_range(rsdp_address, rsdp->length)) return -1;
        rsdp_size = rsdp->length;
    }
    if (acpi_validate_rsdp_blob(rsdp, rsdp_size, &info) != 0) return -1;
retry_root:
    if (!mapped_range(info.root_address, sizeof(acpi_sdt_header_t))) {
        if (info.uses_xsdt != 0u && rsdp->rsdt_address != 0u) {
            info.root_address = rsdp->rsdt_address;
            info.uses_xsdt = 0u;
            goto retry_root;
        }
        return -1;
    }
    root = (const acpi_sdt_header_t *)(uintptr_t)info.root_address;
    if (!mapped_range(info.root_address, root->length) ||
        acpi_validate_sdt_blob(root, root->length, info.uses_xsdt ? "XSDT" : "RSDT") != 0) {
        if (info.uses_xsdt != 0u && rsdp->rsdt_address != 0u) {
            info.root_address = rsdp->rsdt_address;
            info.uses_xsdt = 0u;
            goto retry_root;
        }
        return -1;
    }
    entry_size = info.uses_xsdt ? 8u : 4u;
    if (((root->length - sizeof(*root)) % entry_size) != 0u) return -1;
    entry_count = (root->length - sizeof(*root)) / entry_size;
    state.revision = info.revision;
    state.uses_xsdt = info.uses_xsdt;
    for (index = 0u; index < entry_count; index++) {
        const uint8_t *entry = (const uint8_t *)root + sizeof(*root) + index * entry_size;
        uint64_t address = info.uses_xsdt ? read_u64(entry) : read_u32(entry);
        if (register_table(address) != 0) state.rejected_tables++;
    }
    state.available = 1u;
    return 0;
}

const acpi_status_t *acpi_status(void)
{
    return &state;
}

uint32_t acpi_table_count(void)
{
    return state.table_count;
}

int acpi_table_info(uint32_t index, acpi_table_info_t *result)
{
    uint32_t character;
    if (result == 0 || index >= state.table_count) return -1;
    for (character = 0u; character < 4u; character++)
        result->signature[character] = registry[index].header->signature[character];
    result->signature[4] = '\0';
    result->length = registry[index].header->length;
    result->revision = registry[index].header->revision;
    result->address = registry[index].address;
    return 0;
}

const acpi_sdt_header_t *acpi_find_table(const char *signature, uint32_t instance)
{
    uint32_t index;
    if (signature == 0) return 0;
    for (index = 0u; index < state.table_count; index++) {
        if (bytes_equal(registry[index].header->signature, signature, 4u)) {
            if (instance == 0u) return registry[index].header;
            instance--;
        }
    }
    return 0;
}

uint32_t acpi_cpu_count(void)
{
    return stored_cpus;
}

int acpi_cpu_info(uint32_t index, acpi_cpu_info_t *result)
{
    if (result == 0 || index >= stored_cpus) return -1;
    *result = cpus[index];
    return 0;
}

uint32_t acpi_ioapic_count(void)
{
    return stored_ioapics;
}

int acpi_ioapic_info(uint32_t index, acpi_ioapic_info_t *result)
{
    if (result == 0 || index >= stored_ioapics) return -1;
    *result = ioapics[index];
    return 0;
}

uint32_t acpi_interrupt_override_count(void)
{
    return stored_overrides;
}

int acpi_interrupt_override_info(uint32_t index, acpi_interrupt_override_t *result)
{
    if (result == 0 || index >= stored_overrides) return -1;
    *result = overrides[index];
    return 0;
}

uint32_t acpi_mcfg_segment_count(void)
{
    return stored_mcfg_segments;
}

int acpi_mcfg_segment_info(uint32_t index, acpi_mcfg_segment_t *result)
{
    if (result == 0 || index >= stored_mcfg_segments) return -1;
    *result = mcfg_segments[index];
    return 0;
}

static void set_checksum(uint8_t *data, uint32_t length, uint32_t checksum_offset)
{
    uint8_t sum = 0u;
    uint32_t index;
    data[checksum_offset] = 0u;
    for (index = 0u; index < length; index++) sum = (uint8_t)(sum + data[index]);
    data[checksum_offset] = (uint8_t)(0u - sum);
}

int acpi_self_test(void)
{
    uint8_t rsdp_data[RSDP_V2_SIZE] = {0};
    uint8_t sdt_data[SDT_HEADER_SIZE] = {0};
    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)(void *)rsdp_data;
    acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)(void *)sdt_data;
    acpi_rsdp_info_t info;
    uint32_t index;
    for (index = 0u; index < 8u; index++) rsdp->signature[index] = "RSD PTR "[index];
    rsdp->revision = 2u;
    rsdp->length = RSDP_V2_SIZE;
    rsdp->xsdt_address = 0x2000u;
    set_checksum(rsdp_data, RSDP_V1_SIZE, 8u);
    set_checksum(rsdp_data, RSDP_V2_SIZE, 32u);
    if (acpi_validate_rsdp_blob(rsdp, sizeof(rsdp_data), &info) != 0 ||
        info.uses_xsdt == 0u || info.root_address != 0x2000u) return -1;
    rsdp_data[9]++;
    if (acpi_validate_rsdp_blob(rsdp, sizeof(rsdp_data), &info) == 0) return -1;
    clear_bytes(rsdp_data, sizeof(rsdp_data));
    for (index = 0u; index < 8u; index++) rsdp->signature[index] = "RSD PTR "[index];
    rsdp->rsdt_address = 0x3000u;
    set_checksum(rsdp_data, RSDP_V1_SIZE, 8u);
    if (acpi_validate_rsdp_blob(rsdp, RSDP_V1_SIZE, &info) != 0 ||
        info.uses_xsdt != 0u || info.root_address != 0x3000u) return -1;
    for (index = 0u; index < 4u; index++) sdt->signature[index] = "TEST"[index];
    sdt->length = sizeof(sdt_data);
    set_checksum(sdt_data, sizeof(sdt_data), 9u);
    if (acpi_validate_sdt_blob(sdt, sizeof(sdt_data), "TEST") != 0) return -1;
    sdt_data[10]++;
    if (acpi_validate_sdt_blob(sdt, sizeof(sdt_data), "TEST") == 0) return -1;
    sdt_data[10]--;
    sdt->length = SDT_HEADER_SIZE - 1u;
    return acpi_validate_sdt_blob(sdt, sizeof(sdt_data), "TEST") != 0 ? 0 : -1;
}

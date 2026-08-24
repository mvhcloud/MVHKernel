#ifndef MVH_ACPI_H
#define MVH_ACPI_H

#include <stdint.h>

#define ACPI_MAX_TABLES 64u
#define ACPI_MAX_TABLE_SIZE (1024u * 1024u)
#define ACPI_MAX_CPUS 256u
#define ACPI_MAX_IOAPICS 16u
#define ACPI_MAX_OVERRIDES 32u
#define ACPI_MAX_MCFG_SEGMENTS 32u

typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct __attribute__((packed)) {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_gas_t;

typedef struct {
    uint8_t revision;
    uint64_t root_address;
    uint8_t uses_xsdt;
    char oem_id[7];
} acpi_rsdp_info_t;

typedef struct {
    char signature[5];
    uint32_t length;
    uint8_t revision;
    uint64_t address;
} acpi_table_info_t;

typedef struct {
    uint32_t processor_uid;
    uint32_t apic_id;
    uint8_t enabled;
    uint8_t online_capable;
    uint8_t x2apic;
} acpi_cpu_info_t;

typedef struct {
    uint8_t id;
    uint32_t address;
    uint32_t global_interrupt_base;
} acpi_ioapic_info_t;

typedef struct {
    uint8_t bus;
    uint8_t source_irq;
    uint32_t global_interrupt;
    uint16_t flags;
} acpi_interrupt_override_t;

typedef struct {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
} acpi_mcfg_segment_t;

typedef struct {
    uint8_t available;
    uint8_t revision;
    uint8_t uses_xsdt;
    uint32_t table_count;
    uint32_t rejected_tables;
    uint32_t unknown_tables;
    uint64_t lapic_address;
    uint32_t lapic_flags;
    uint32_t local_apics;
    uint32_t local_x2apics;
    uint32_t ioapics;
    uint32_t interrupt_overrides;
    uint32_t nmi_sources;
    uint32_t local_apic_nmis;
    uint32_t mcfg_segments;
    uint32_t srat_cpu_affinities;
    uint32_t srat_memory_affinities;
    uint64_t hpet_address;
    uint32_t pm_timer_port;
    acpi_gas_t reset_register;
    uint8_t reset_value;
    uint64_t slit_localities;
} acpi_status_t;

int acpi_checksum_valid(const void *data, uint32_t length);
int acpi_validate_rsdp_blob(const void *data, uint32_t size, acpi_rsdp_info_t *result);
int acpi_validate_sdt_blob(const void *data, uint32_t available, const char *signature);
int acpi_init(uint64_t rsdp_address);
uint64_t acpi_discover_rsdp(void);
const acpi_status_t *acpi_status(void);
uint32_t acpi_table_count(void);
int acpi_table_info(uint32_t index, acpi_table_info_t *result);
const acpi_sdt_header_t *acpi_find_table(const char *signature, uint32_t instance);
uint32_t acpi_cpu_count(void);
int acpi_cpu_info(uint32_t index, acpi_cpu_info_t *result);
uint32_t acpi_ioapic_count(void);
int acpi_ioapic_info(uint32_t index, acpi_ioapic_info_t *result);
uint32_t acpi_interrupt_override_count(void);
int acpi_interrupt_override_info(uint32_t index, acpi_interrupt_override_t *result);
uint32_t acpi_mcfg_segment_count(void);
int acpi_mcfg_segment_info(uint32_t index, acpi_mcfg_segment_t *result);
int acpi_self_test(void);

#endif

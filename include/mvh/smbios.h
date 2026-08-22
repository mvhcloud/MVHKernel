#ifndef MVH_SMBIOS_H
#define MVH_SMBIOS_H

#include <stdint.h>

#define SMBIOS_TEXT_MAX 64u
#define SMBIOS_MAX_TABLE_SIZE (1024u * 1024u)

typedef struct {
    uint8_t available;
    uint8_t entry_point_64;
    uint8_t major;
    uint8_t minor;
    uint8_t document_revision;
    uint64_t table_address;
    uint32_t table_size;
    uint32_t announced_structures;
    uint32_t parsed_structures;
    uint32_t bios_structures;
    uint32_t system_structures;
    uint32_t baseboard_structures;
    uint32_t processor_structures;
    uint32_t memory_device_structures;
    uint32_t populated_memory_devices;
    uint64_t installed_memory_mib;
    uint32_t maximum_memory_speed_mhz;
    char bios_vendor[SMBIOS_TEXT_MAX];
    char bios_version[SMBIOS_TEXT_MAX];
    char system_manufacturer[SMBIOS_TEXT_MAX];
    char system_product[SMBIOS_TEXT_MAX];
    char system_version[SMBIOS_TEXT_MAX];
    char system_serial[SMBIOS_TEXT_MAX];
    char baseboard_manufacturer[SMBIOS_TEXT_MAX];
    char baseboard_product[SMBIOS_TEXT_MAX];
} smbios_info_t;

int smbios_validate_entry_blob(const void *data, uint32_t size, smbios_info_t *result);
int smbios_parse_table_blob(const void *data, uint32_t size, uint32_t announced,
                            smbios_info_t *result);
int smbios_init(uint64_t entry_address);
const smbios_info_t *smbios_info(void);
int smbios_self_test(void);

#endif

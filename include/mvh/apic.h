#ifndef MVH_APIC_H
#define MVH_APIC_H

#include <stdint.h>

typedef struct {
    uint8_t available;
    uint8_t local_apic_enabled;
    uint8_t ioapic_enabled;
    uint8_t legacy_pic_disabled;
    uint32_t local_apic_id;
    uint32_t local_apic_version;
    uint32_t ioapic_id;
    uint32_t ioapic_version;
    uint32_t ioapic_redirections;
    uint32_t timer_gsi;
    uint8_t timer_vector;
    uint64_t local_apic_address;
    uint64_t ioapic_address;
} apic_status_t;

int apic_init(void);
const apic_status_t *apic_status(void);
void apic_eoi(void);
void apic_init_local_cpu(void);
int apic_start_application_processor(uint32_t apic_id, uint8_t startup_vector);

#endif

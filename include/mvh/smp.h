#ifndef MVH_SMP_H
#define MVH_SMP_H

#include <stdint.h>

#define SMP_MAX_CPUS 64u

typedef struct {
    uintptr_t self;
    uint32_t logical_id;
    uint32_t apic_id;
    uint32_t processor_uid;
    volatile uint32_t online;
    uint8_t bootstrap;
    uint8_t x2apic;
    uint16_t reserved;
    uintptr_t stack_base;
    uintptr_t stack_top;
    uint64_t idle_halts;
} smp_cpu_t;

int smp_init(void);
uint32_t smp_cpu_count(void);
uint32_t smp_online_count(void);
const smp_cpu_t *smp_cpu_info(uint32_t logical_id);
const smp_cpu_t *smp_current_cpu(void);
void smp_ap_entry(smp_cpu_t *cpu) __attribute__((noreturn));

#endif

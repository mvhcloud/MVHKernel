#ifndef MVH_SMP_H
#define MVH_SMP_H

#include <stdint.h>

#define SMP_MAX_CPUS 64u
#define SMP_IPI_RESCHEDULE 0xF0u
#define SMP_IPI_TLB_SHOOTDOWN 0xF1u
#define SMP_IPI_STOP 0xF2u

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
    volatile uint32_t reschedule_pending;
    volatile uint32_t ipi_count;
    volatile uint32_t tlb_shootdowns;
} smp_cpu_t;

int smp_init(void);
uint32_t smp_cpu_count(void);
uint32_t smp_online_count(void);
const smp_cpu_t *smp_cpu_info(uint32_t logical_id);
const smp_cpu_t *smp_current_cpu(void);
int smp_request_reschedule(uint32_t logical_id);
int smp_stop_cpu(uint32_t logical_id);
int smp_tlb_shootdown(uintptr_t address, uint64_t size);
int smp_ipi_self_test(void);
void smp_reschedule_ipi(void);
void smp_tlb_ipi(void);
void smp_stop_ipi(void) __attribute__((noreturn));
void smp_ap_entry(smp_cpu_t *cpu) __attribute__((noreturn));

#endif

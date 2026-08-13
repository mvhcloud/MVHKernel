#ifndef MVH_CPU_H
#define MVH_CPU_H

#include <stdint.h>

typedef struct {
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t apic_id;
    uint32_t logical_cpus;
    uint32_t cache_line_bytes;
    uint32_t l1_data_kib;
    uint32_t l1_instruction_kib;
    uint32_t l2_kib;
    uint32_t l3_kib;
    uint32_t xsave_bytes;
    uint32_t microcode;
    uint64_t tsc_hz;
    uint64_t local_apic_base;
    uint8_t local_apic_enabled;
    uint8_t x2apic_enabled;
    uint8_t microcode_available;
    uint8_t temperature_available;
    int32_t temperature_celsius;
} cpu_info_t;

typedef struct {
    uint8_t fpu;
    uint8_t sse;
    uint8_t sse2;
    uint8_t avx;
    uint8_t avx2;
    uint8_t avx512f;
    uint8_t xsave;
    uint8_t osxsave;
    uint8_t msr;
    uint8_t apic;
    uint8_t x2apic;
    uint8_t tsc;
    uint8_t invariant_tsc;
    uint8_t rdrand;
    uint8_t rdseed;
    uint8_t mtrr;
    uint8_t pat;
    uint8_t nx;
    uint8_t smep;
    uint8_t smap;
    uint8_t umip;
} cpu_capabilities_t;

typedef struct {
    uint8_t write_protect;
    uint8_t nx;
    uint8_t smep;
    uint8_t smap;
    uint8_t umip;
} cpu_security_state_t;

void cpu_init(void);
void cpu_vendor(char *vendor);
void cpu_brand(char *brand);
uint32_t cpu_logical_count(void);
uint32_t cpu_feature_ecx(void);
uint32_t cpu_feature_edx(void);
uint32_t cpu_extended_feature_edx(void);
void cpu_get_info(cpu_info_t *info);
void cpu_get_capabilities(cpu_capabilities_t *capabilities);
void cpu_get_security_state(cpu_security_state_t *state);
uint64_t cpu_read_tsc(void);
uint8_t cpu_random64(uint64_t *value);
uint8_t cpu_rdmsr(uint32_t msr, uint64_t *value);
void cpu_wrmsr(uint32_t msr, uint64_t value);

#endif

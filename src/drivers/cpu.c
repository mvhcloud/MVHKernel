#include <stdint.h>
#include "mvh/cpu.h"
#include "mvh/pci.h"
#include "mvh/timer.h"

static cpu_info_t detected_info;
static cpu_capabilities_t detected_capabilities;
static cpu_security_state_t security_state;
static uint8_t intel_digital_thermal_sensor;
static uint8_t amd_temperature_sensor;

static uint8_t text_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) return 0u;
    }
    return (uint8_t)(*left == *right);
}

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile ("cpuid"
                      : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                      : "a"(leaf), "c"(subleaf));
}

static void store_u32(char *target, uint32_t value)
{
    target[0] = (char)(value & 0xFFu);
    target[1] = (char)((value >> 8u) & 0xFFu);
    target[2] = (char)((value >> 16u) & 0xFFu);
    target[3] = (char)((value >> 24u) & 0xFFu);
}

static uint64_t xgetbv(uint32_t index)
{
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("xgetbv" : "=a"(low), "=d"(high) : "c"(index));
    return ((uint64_t)high << 32u) | low;
}

static void xsetbv(uint32_t index, uint64_t value)
{
    __asm__ volatile ("xsetbv" : : "a"((uint32_t)value),
                      "d"((uint32_t)(value >> 32u)), "c"(index));
}

uint64_t cpu_read_tsc(void)
{
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("lfence; rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32u) | low;
}

uint8_t cpu_rdmsr(uint32_t msr, uint64_t *value)
{
    uint32_t low;
    uint32_t high;
    if (detected_capabilities.msr == 0u || value == 0) {
        return 0u;
    }
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    *value = ((uint64_t)high << 32u) | low;
    return 1u;
}

void cpu_wrmsr(uint32_t msr, uint64_t value)
{
    if (detected_capabilities.msr != 0u) {
        __asm__ volatile ("wrmsr" : : "a"((uint32_t)value),
                          "d"((uint32_t)(value >> 32u)), "c"(msr));
    }
}

static void detect_cache(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t index;
    uint32_t type;
    uint32_t level;
    uint32_t size;
    cpuid(0u, 0u, &eax, &ebx, &ecx, &edx);
    if (eax >= 4u) {
        for (index = 0u; index < 16u; index++) {
            cpuid(4u, index, &eax, &ebx, &ecx, &edx);
            type = eax & 0x1Fu;
            if (type == 0u) break;
            level = (eax >> 5u) & 7u;
            size = (((ebx >> 22u) & 0x3FFu) + 1u) *
                   (((ebx >> 12u) & 0x3FFu) + 1u) *
                   ((ebx & 0xFFFu) + 1u) * (ecx + 1u) / 1024u;
            if (detected_info.cache_line_bytes == 0u) {
                detected_info.cache_line_bytes = (ebx & 0xFFFu) + 1u;
            }
            if (level == 1u && type == 1u) detected_info.l1_data_kib = size;
            if (level == 1u && type == 2u) detected_info.l1_instruction_kib = size;
            if (level == 2u) detected_info.l2_kib = size;
            if (level == 3u) detected_info.l3_kib = size;
        }
    }
    cpuid(0x80000000u, 0u, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000005u && detected_info.l1_data_kib == 0u) {
        cpuid(0x80000005u, 0u, &eax, &ebx, &ecx, &edx);
        detected_info.l1_data_kib = ecx >> 24u;
        detected_info.l1_instruction_kib = edx >> 24u;
        detected_info.cache_line_bytes = ecx & 0xFFu;
    }
    cpuid(0x80000000u, 0u, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000006u && detected_info.l2_kib == 0u) {
        cpuid(0x80000006u, 0u, &eax, &ebx, &ecx, &edx);
        detected_info.l2_kib = ecx >> 16u;
        detected_info.l3_kib = ((edx >> 18u) & 0x3FFFu) * 512u;
    }
}

static uint8_t temperature_valid(int32_t millicelsius)
{
    return (uint8_t)(millicelsius >= -40000 && millicelsius <= 150000);
}

static uint8_t read_amd_legacy_temperature(int32_t *millicelsius)
{
    uint32_t identity = pci_config_read32(0u, 0x18u, 3u, 0u);
    uint32_t value;
    int32_t temperature;
    if ((identity & 0xFFFFu) != 0x1022u) return 0u;
    value = pci_config_read32(0u, 0x18u, 3u, 0xA4u);
    temperature = (int32_t)((value >> 21u) & 0x7FFu) * 125;
    if (temperature_valid(temperature) == 0u) return 0u;
    *millicelsius = temperature;
    return 1u;
}

static uint8_t read_amd_smn_temperature(int32_t *millicelsius)
{
    uint32_t identity = pci_config_read32(0u, 0u, 0u, 0u);
    uint32_t value;
    int32_t temperature;
    if ((identity & 0xFFFFu) != 0x1022u) return 0u;
    pci_config_write32(0u, 0u, 0u, 0x60u, 0x00059800u);
    value = pci_config_read32(0u, 0u, 0u, 0x64u);
    temperature = (int32_t)((value >> 21u) & 0x7FFu) * 125;
    if (temperature_valid(temperature) == 0u) return 0u;
    *millicelsius = temperature;
    return 1u;
}

void cpu_init(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t max_leaf;
    uint32_t max_extended;
    uint64_t control;
    uint64_t cr0;
    uint64_t cr4;
    uint32_t base_family;
    uint32_t base_model;
    char vendor[13];
    uint64_t microcode_value;
    uint64_t apic_base;
    uint64_t tsc_start;
    uint64_t tick_start;
    uint64_t tick_end;
    cpuid(0u, 0u, &max_leaf, &ebx, &ecx, &edx);
    cpuid(0x80000000u, 0u, &max_extended, &ebx, &ecx, &edx);
    cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);
    base_family = (eax >> 8u) & 0xFu;
    base_model = (eax >> 4u) & 0xFu;
    detected_info.stepping = eax & 0xFu;
    detected_info.family = base_family == 0xFu ? base_family + ((eax >> 20u) & 0xFFu) : base_family;
    detected_info.model = (base_family == 0x6u || base_family == 0xFu) ?
                          base_model | ((eax >> 12u) & 0xF0u) : base_model;
    detected_info.apic_id = ebx >> 24u;
    detected_info.logical_cpus = (ebx >> 16u) & 0xFFu;
    if (detected_info.logical_cpus == 0u) detected_info.logical_cpus = 1u;
    detected_capabilities.fpu = (uint8_t)((edx >> 0u) & 1u);
    detected_capabilities.tsc = (uint8_t)((edx >> 4u) & 1u);
    detected_capabilities.msr = (uint8_t)((edx >> 5u) & 1u);
    detected_capabilities.apic = (uint8_t)((edx >> 9u) & 1u);
    detected_capabilities.mtrr = (uint8_t)((edx >> 12u) & 1u);
    detected_capabilities.pat = (uint8_t)((edx >> 16u) & 1u);
    detected_capabilities.sse = (uint8_t)((edx >> 25u) & 1u);
    detected_capabilities.sse2 = (uint8_t)((edx >> 26u) & 1u);
    detected_capabilities.x2apic = (uint8_t)((ecx >> 21u) & 1u);
    detected_capabilities.xsave = (uint8_t)((ecx >> 26u) & 1u);
    detected_capabilities.osxsave = (uint8_t)((ecx >> 27u) & 1u);
    detected_capabilities.avx = (uint8_t)((ecx >> 28u) & 1u);
    detected_capabilities.rdrand = (uint8_t)((ecx >> 30u) & 1u);
    if (max_leaf >= 7u) {
        cpuid(7u, 0u, &eax, &ebx, &ecx, &edx);
        detected_capabilities.avx2 = (uint8_t)((ebx >> 5u) & 1u);
        detected_capabilities.smep = (uint8_t)((ebx >> 7u) & 1u);
        detected_capabilities.avx512f = (uint8_t)((ebx >> 16u) & 1u);
        detected_capabilities.rdseed = (uint8_t)((ebx >> 18u) & 1u);
        detected_capabilities.smap = (uint8_t)((ebx >> 20u) & 1u);
        detected_capabilities.umip = (uint8_t)((ecx >> 2u) & 1u);
    }
    if (max_extended >= 0x80000001u) {
        cpuid(0x80000001u, 0u, &eax, &ebx, &ecx, &edx);
        detected_capabilities.nx = (uint8_t)((edx >> 20u) & 1u);
    }
    if (max_extended >= 0x80000007u) {
        cpuid(0x80000007u, 0u, &eax, &ebx, &ecx, &edx);
        detected_capabilities.invariant_tsc = (uint8_t)((edx >> 8u) & 1u);
    }
    if (max_leaf >= 0xDu && detected_capabilities.xsave != 0u) {
        cpuid(0xDu, 0u, &eax, &ebx, &ecx, &edx);
        detected_info.xsave_bytes = ebx;
    }
    if (max_leaf >= 0x16u) {
        cpuid(0x16u, 0u, &eax, &ebx, &ecx, &edx);
        detected_info.tsc_hz = (uint64_t)eax * 1000000u;
    }
    detect_cache();
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ull << 2u);
    cr0 |= (1ull << 1u) | (1ull << 5u) | (1ull << 16u);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    security_state.write_protect = 1u;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    if (detected_capabilities.sse != 0u) cr4 |= (1ull << 9u) | (1ull << 10u);
    if (detected_capabilities.xsave != 0u) {
        cr4 |= (1ull << 18u);
        __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
        control = xgetbv(0u) | 3u;
        if (detected_capabilities.avx != 0u) control |= 4u;
        xsetbv(0u, control);
        detected_capabilities.osxsave = 1u;
    }
    if (detected_capabilities.smep != 0u) {
        cr4 |= 1ull << 20u;
        security_state.smep = 1u;
    }
    if (detected_capabilities.smap != 0u) {
        cr4 |= 1ull << 21u;
        security_state.smap = 1u;
    }
    if (detected_capabilities.umip != 0u) {
        cr4 |= 1ull << 11u;
        security_state.umip = 1u;
    }
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
    if (detected_capabilities.nx != 0u && detected_capabilities.msr != 0u) {
        cpu_rdmsr(0xC0000080u, &control);
        cpu_wrmsr(0xC0000080u, control | (1ull << 11u));
        security_state.nx = 1u;
    }
    if (detected_capabilities.apic != 0u && detected_capabilities.msr != 0u &&
        cpu_rdmsr(0x1Bu, &apic_base) != 0u) {
        detected_info.local_apic_base = apic_base & 0x000FFFFFFFFFF000ull;
        detected_info.local_apic_enabled = (uint8_t)((apic_base >> 11u) & 1u);
        detected_info.x2apic_enabled = (uint8_t)((apic_base >> 10u) & 1u);
    }
    if (detected_capabilities.tsc != 0u && detected_info.tsc_hz == 0u) {
        tick_start = timer_ticks();
        tsc_start = cpu_read_tsc();
        tick_end = tick_start + 10u;
        while (timer_ticks() < tick_end) __asm__ volatile ("hlt");
        tick_end = timer_ticks();
        if (tick_end > tick_start) {
            detected_info.tsc_hz = (cpu_read_tsc() - tsc_start) * timer_frequency() /
                                   (tick_end - tick_start);
        }
    }
    cpu_vendor(vendor);
    detected_info.microcode_available = 0u;
    detected_info.temperature_available = 0u;
    intel_digital_thermal_sensor = 0u;
    amd_temperature_sensor = 0u;
    if (text_equal(vendor, "GenuineIntel") != 0u) {
        if (max_leaf >= 6u) {
            cpuid(6u, 0u, &eax, &ebx, &ecx, &edx);
            intel_digital_thermal_sensor = (uint8_t)(eax & 1u);
        }
        if (detected_capabilities.msr != 0u) {
            cpu_wrmsr(0x8Bu, 0u);
            cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);
            if (cpu_rdmsr(0x8Bu, &microcode_value) != 0u) {
                detected_info.microcode = (uint32_t)(microcode_value >> 32u);
                detected_info.microcode_available = 1u;
            }
        }
    } else if (text_equal(vendor, "AuthenticAMD") != 0u) {
        if (detected_info.family >= 0x10u && detected_info.family <= 0x16u) {
            amd_temperature_sensor = 1u;
        } else if (detected_info.family >= 0x17u && detected_info.family <= 0x1Au) {
            amd_temperature_sensor = 2u;
        }
    }
}

void cpu_get_info(cpu_info_t *info)
{
    uint64_t therm_status;
    uint64_t temperature_target;
    int32_t millicelsius;
    detected_info.temperature_available = 0u;
    detected_info.temperature_source = "unavailable";
    if (intel_digital_thermal_sensor != 0u &&
        cpu_rdmsr(0x19Cu, &therm_status) != 0u && (therm_status & (1ull << 31u)) != 0u &&
        cpu_rdmsr(0x1A2u, &temperature_target) != 0u) {
        detected_info.temperature_celsius =
            (int32_t)((temperature_target >> 16u) & 0xFFu) -
            (int32_t)((therm_status >> 16u) & 0x7Fu);
        detected_info.temperature_millicelsius = detected_info.temperature_celsius * 1000;
        if (temperature_valid(detected_info.temperature_millicelsius) != 0u) {
            detected_info.temperature_available = 1u;
            detected_info.temperature_source = "Intel DTS/MSR";
        }
    } else if (amd_temperature_sensor == 1u &&
               read_amd_legacy_temperature(&millicelsius) != 0u) {
        detected_info.temperature_millicelsius = millicelsius;
        detected_info.temperature_celsius = millicelsius / 1000;
        detected_info.temperature_available = 1u;
        detected_info.temperature_source = "AMD northbridge Tctl";
    } else if (amd_temperature_sensor == 2u &&
               read_amd_smn_temperature(&millicelsius) != 0u) {
        detected_info.temperature_millicelsius = millicelsius;
        detected_info.temperature_celsius = millicelsius / 1000;
        detected_info.temperature_available = 1u;
        detected_info.temperature_source = "AMD SMN Tctl";
    }
    *info = detected_info;
}

void cpu_get_capabilities(cpu_capabilities_t *capabilities)
{
    *capabilities = detected_capabilities;
}

void cpu_get_security_state(cpu_security_state_t *state)
{
    *state = security_state;
}

uint8_t cpu_random64(uint64_t *value)
{
    uint8_t success;
    uint32_t attempt;
    if (value == 0 || detected_capabilities.rdrand == 0u) return 0u;
    for (attempt = 0u; attempt < 10u; attempt++) {
        __asm__ volatile ("rdrand %0; setc %1" : "=r"(*value), "=qm"(success));
        if (success != 0u) return 1u;
    }
    return 0u;
}

void cpu_vendor(char *vendor)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(0u, 0u, &eax, &ebx, &ecx, &edx);
    store_u32(vendor, ebx);
    store_u32(vendor + 4, edx);
    store_u32(vendor + 8, ecx);
    vendor[12] = '\0';
}

void cpu_brand(char *brand)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    unsigned int index;
    cpuid(0x80000000u, 0u, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000004u) {
        brand[0] = '\0';
        return;
    }
    for (index = 0; index < 3u; index++) {
        cpuid(0x80000002u + index, 0u, &eax, &ebx, &ecx, &edx);
        store_u32(brand + index * 16u, eax);
        store_u32(brand + index * 16u + 4u, ebx);
        store_u32(brand + index * 16u + 8u, ecx);
        store_u32(brand + index * 16u + 12u, edx);
    }
    brand[48] = '\0';
}

uint32_t cpu_logical_count(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t count;
    cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);
    count = (ebx >> 16u) & 0xFFu;
    return count == 0u ? 1u : count;
}

uint32_t cpu_feature_ecx(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);
    return ecx;
}

uint32_t cpu_feature_edx(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(1u, 0u, &eax, &ebx, &ecx, &edx);
    return edx;
}

uint32_t cpu_extended_feature_edx(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    cpuid(0x80000000u, 0u, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001u) {
        return 0u;
    }
    cpuid(0x80000001u, 0u, &eax, &ebx, &ecx, &edx);
    return edx;
}

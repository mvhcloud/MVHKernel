#include <stdint.h>
#include "mvh/cpu.h"

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

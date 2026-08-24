#include <stdint.h>
#include "mvh/cpu.h"
#include "mvh/gdt.h"
#include "mvh/smp.h"

#define GDT_ENTRIES 7u
#define IST_STACK_SIZE 16384u
#define IA32_FS_BASE 0xC0000100u

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} tss64_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} gdtr_t;

static uint64_t cpu_gdt[SMP_MAX_CPUS][GDT_ENTRIES] __attribute__((aligned(16)));
static tss64_t cpu_tss[SMP_MAX_CPUS] __attribute__((aligned(16)));
static uint8_t double_fault_stacks[SMP_MAX_CPUS][IST_STACK_SIZE] __attribute__((aligned(4096)));
static uint8_t nmi_stacks[SMP_MAX_CPUS][IST_STACK_SIZE] __attribute__((aligned(4096)));
static uint8_t machine_check_stacks[SMP_MAX_CPUS][IST_STACK_SIZE] __attribute__((aligned(4096)));

static void clear_bytes(void *address, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)address;
    uint32_t index;
    for (index = 0u; index < size; index++) bytes[index] = 0u;
}

static void set_tss_descriptor(uint64_t *gdt, const tss64_t *tss)
{
    uint64_t base = (uint64_t)(uintptr_t)tss;
    uint64_t limit = sizeof(*tss) - 1u;
    gdt[3] = (limit & 0xFFFFu) | ((base & 0xFFFFFFu) << 16u) |
             (0x89ull << 40u) | (((limit >> 16u) & 0xFu) << 48u) |
             (((base >> 24u) & 0xFFu) << 56u);
    gdt[4] = base >> 32u;
}

int gdt_init_cpu(uint32_t cpu_id, uintptr_t rsp0)
{
    gdtr_t gdtr;
    uint16_t tss_selector = GDT_TSS;
    uint64_t *gdt;
    tss64_t *tss;
    if (cpu_id >= SMP_MAX_CPUS || rsp0 == 0u) return -1;
    gdt = cpu_gdt[cpu_id];
    tss = &cpu_tss[cpu_id];
    clear_bytes(gdt, sizeof(cpu_gdt[cpu_id]));
    clear_bytes(tss, sizeof(*tss));
    gdt[0] = 0u;
    gdt[1] = 0x00AF9A000000FFFFull;
    gdt[2] = 0x00CF92000000FFFFull;
    gdt[5] = 0x00CFF2000000FFFFull;
    gdt[6] = 0x00AFFA000000FFFFull;
    tss->rsp0 = rsp0;
    tss->ist[0] = (uintptr_t)&double_fault_stacks[cpu_id][IST_STACK_SIZE];
    tss->ist[1] = (uintptr_t)&nmi_stacks[cpu_id][IST_STACK_SIZE];
    tss->ist[2] = (uintptr_t)&machine_check_stacks[cpu_id][IST_STACK_SIZE];
    tss->iomap_base = sizeof(*tss);
    set_tss_descriptor(gdt, tss);
    gdtr.limit = sizeof(cpu_gdt[cpu_id]) - 1u;
    gdtr.base = (uint64_t)(uintptr_t)gdt;
    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");
    __asm__ volatile ("mov %0, %%ds; mov %0, %%es; mov %0, %%ss" : :
                      "r"((uint16_t)GDT_KERNEL_DATA) : "memory");
    __asm__ volatile ("ltr %0" : : "r"(tss_selector) : "memory");
    cpu_wrmsr(IA32_FS_BASE, 0u);
    return 0;
}

void gdt_set_rsp0(uint32_t cpu_id, uintptr_t rsp0)
{
    if (cpu_id < SMP_MAX_CPUS) cpu_tss[cpu_id].rsp0 = rsp0;
}

uintptr_t gdt_ist_top(uint32_t cpu_id, uint8_t ist_index)
{
    if (cpu_id >= SMP_MAX_CPUS || ist_index == 0u || ist_index > 3u) return 0u;
    return (uintptr_t)cpu_tss[cpu_id].ist[ist_index - 1u];
}

#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/apic.h"
#include "mvh/cpu.h"
#include "mvh/gdt.h"
#include "mvh/interrupt.h"
#include "mvh/memory.h"
#include "mvh/serial.h"
#include "mvh/smp.h"
#include "mvh/sync.h"

#define AP_TRAMPOLINE_ADDRESS 0x8000u
#define AP_TRAMPOLINE_VECTOR (AP_TRAMPOLINE_ADDRESS >> 12u)
#define AP_STACK_PAGES 4u
#define IA32_GS_BASE 0xC0000101u
#define IA32_KERNEL_GS_BASE 0xC0000102u

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];
extern uint8_t ap_trampoline_cr3[];
extern uint8_t ap_trampoline_stack[];
extern uint8_t ap_trampoline_cpu[];
extern uint8_t ap_trampoline_entry[];

static smp_cpu_t cpus[SMP_MAX_CPUS];
static volatile uint32_t discovered_cpus;
static volatile uint32_t online_cpus;
static spinlock_t tlb_lock;
static volatile uintptr_t tlb_address;
static volatile uint64_t tlb_size;
static volatile uint32_t tlb_acknowledged;

static void serial_number(uint32_t value)
{
    char digits[10];
    uint32_t length = 0u;
    if (value == 0u) {
        serial_put('0');
        return;
    }
    while (value != 0u) {
        digits[length++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (length != 0u) serial_put(digits[--length]);
}

static void serial_text(const char *text)
{
    while (*text != '\0') serial_put(*text++);
}

static uintptr_t trampoline_offset(const uint8_t *symbol)
{
    return (uintptr_t)(symbol - ap_trampoline_start);
}

static void copy_trampoline(void)
{
    uint8_t *target = (uint8_t *)(uintptr_t)AP_TRAMPOLINE_ADDRESS;
    uintptr_t size = (uintptr_t)(ap_trampoline_end - ap_trampoline_start);
    uintptr_t index;
    for (index = 0u; index < size; index++) target[index] = ap_trampoline_start[index];
}

static void trampoline_write64(const uint8_t *symbol, uint64_t value)
{
    volatile uint64_t *target = (volatile uint64_t *)(uintptr_t)
        (AP_TRAMPOLINE_ADDRESS + trampoline_offset(symbol));
    *target = value;
}

static void configure_gs(smp_cpu_t *cpu)
{
    cpu_wrmsr(IA32_GS_BASE, (uint64_t)(uintptr_t)cpu);
    cpu_wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)(uintptr_t)cpu);
}

const smp_cpu_t *smp_current_cpu(void)
{
    const smp_cpu_t *cpu;
    __asm__ volatile ("mov %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

void smp_ap_entry(smp_cpu_t *cpu)
{
    configure_gs(cpu);
    if (gdt_init_cpu(cpu->logical_id, cpu->stack_top) != 0) {
        for (;;) __asm__ volatile ("cli; hlt");
    }
    interrupt_load_idt();
    apic_init_local_cpu();
    interrupt_enable();
    __atomic_store_n(&cpu->online, 1u, __ATOMIC_RELEASE);
    __atomic_add_fetch(&online_cpus, 1u, __ATOMIC_ACQ_REL);
    for (;;) {
        cpu->idle_halts++;
        __asm__ volatile ("hlt");
    }
}

int smp_init(void)
{
    const apic_status_t *runtime = apic_status();
    acpi_cpu_info_t firmware_cpu;
    uint64_t cr3;
    uint32_t index;
    if (runtime->available == 0u || runtime->local_apic_enabled == 0u) return -1;

    for (index = 0u; index < SMP_MAX_CPUS; index++) cpus[index].online = 0u;
    cpus[0].logical_id = 0u;
    cpus[0].self = (uintptr_t)&cpus[0];
    cpus[0].apic_id = runtime->local_apic_id;
    cpus[0].processor_uid = 0u;
    cpus[0].bootstrap = 1u;
    cpus[0].online = 1u;
    discovered_cpus = 1u;
    online_cpus = 1u;
    spinlock_init(&tlb_lock);
    configure_gs(&cpus[0]);
    {
        uintptr_t rsp;
        __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
        if (gdt_init_cpu(0u, rsp) != 0) return -1;
    }
    copy_trampoline();
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    trampoline_write64(ap_trampoline_cr3, cr3);
    trampoline_write64(ap_trampoline_entry, (uint64_t)(uintptr_t)smp_ap_entry);

    for (index = 0u; index < acpi_cpu_count() && discovered_cpus < SMP_MAX_CPUS; index++) {
        smp_cpu_t *cpu;
        uint32_t wait;
        void *stack;
        if (acpi_cpu_info(index, &firmware_cpu) != 0 ||
            (firmware_cpu.enabled == 0u && firmware_cpu.online_capable == 0u) ||
            firmware_cpu.apic_id == runtime->local_apic_id) continue;
        if (firmware_cpu.x2apic != 0u && firmware_cpu.apic_id > 255u) continue;
        cpu = &cpus[discovered_cpus];
        stack = pmm_alloc_pages(AP_STACK_PAGES);
        if (stack == 0) continue;
        cpu->logical_id = discovered_cpus;
        cpu->self = (uintptr_t)cpu;
        cpu->apic_id = firmware_cpu.apic_id;
        cpu->processor_uid = firmware_cpu.processor_uid;
        cpu->x2apic = firmware_cpu.x2apic;
        cpu->bootstrap = 0u;
        cpu->stack_base = (uintptr_t)stack;
        cpu->stack_top = cpu->stack_base + AP_STACK_PAGES * 4096u;
        cpu->idle_halts = 0u;
        cpu->reschedule_pending = 0u;
        cpu->ipi_count = 0u;
        cpu->tlb_shootdowns = 0u;
        cpu->online = 0u;
        discovered_cpus++;
        trampoline_write64(ap_trampoline_stack, cpu->stack_top);
        trampoline_write64(ap_trampoline_cpu, (uint64_t)(uintptr_t)cpu);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        serial_text("[INFO] SMP starting APIC ID ");
        serial_number(cpu->apic_id);
        serial_text("\n");
        if (apic_start_application_processor(cpu->apic_id,
                                             (uint8_t)AP_TRAMPOLINE_VECTOR) != 0) continue;
        for (wait = 0u; wait < 10000000u &&
             __atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) == 0u; wait++)
            __asm__ volatile ("pause");
    }
    return online_cpus != 0u && smp_ipi_self_test() == 0 ? 0 : -1;
}

uint32_t smp_cpu_count(void)
{
    return __atomic_load_n(&discovered_cpus, __ATOMIC_ACQUIRE);
}

uint32_t smp_online_count(void)
{
    return __atomic_load_n(&online_cpus, __ATOMIC_ACQUIRE);
}

const smp_cpu_t *smp_cpu_info(uint32_t logical_id)
{
    return logical_id < smp_cpu_count() ? &cpus[logical_id] : 0;
}

int smp_request_reschedule(uint32_t logical_id)
{
    smp_cpu_t *cpu;
    const smp_cpu_t *current;
    if (logical_id >= smp_cpu_count()) return -1;
    cpu = &cpus[logical_id];
    if (cpu->online == 0u) return -1;
    current = smp_current_cpu();
    if (current != 0 && logical_id == current->logical_id) {
        cpu->reschedule_pending = 1u;
        return 0;
    }
    return apic_send_fixed(cpu->apic_id, SMP_IPI_RESCHEDULE);
}

int smp_stop_cpu(uint32_t logical_id)
{
    smp_cpu_t *cpu;
    if (logical_id == 0u || logical_id >= smp_cpu_count()) return -1;
    cpu = &cpus[logical_id];
    return cpu->online != 0u ? apic_send_fixed(cpu->apic_id, SMP_IPI_STOP) : -1;
}

static void invalidate_range(uintptr_t address, uint64_t size)
{
    uintptr_t page = address & ~4095ull;
    uintptr_t end = (address + size + 4095u) & ~4095ull;
    while (page < end) {
        __asm__ volatile ("invlpg (%0)" : : "r"(page) : "memory");
        page += 4096u;
    }
}

int smp_tlb_shootdown(uintptr_t address, uint64_t size)
{
    uint32_t expected;
    uint32_t wait;
    if (size == 0u || size > UINTPTR_MAX - address ||
        address + size > UINTPTR_MAX - 4095u) return -1;
    spinlock_lock(&tlb_lock);
    tlb_address = address;
    tlb_size = size;
    tlb_acknowledged = 0u;
    memory_barrier();
    expected = smp_online_count() - 1u;
    if (expected != 0u && apic_broadcast_fixed(SMP_IPI_TLB_SHOOTDOWN) != 0) {
        spinlock_unlock(&tlb_lock);
        return -1;
    }
    invalidate_range(address, size);
    for (wait = 0u; wait < 10000000u &&
         __atomic_load_n(&tlb_acknowledged, __ATOMIC_ACQUIRE) < expected; wait++)
        __asm__ volatile ("pause");
    spinlock_unlock(&tlb_lock);
    return tlb_acknowledged == expected ? 0 : -1;
}

void smp_reschedule_ipi(void)
{
    smp_cpu_t *cpu = (smp_cpu_t *)smp_current_cpu();
    cpu->reschedule_pending = 1u;
    cpu->ipi_count++;
    apic_eoi();
}

void smp_tlb_ipi(void)
{
    smp_cpu_t *cpu = (smp_cpu_t *)smp_current_cpu();
    memory_barrier();
    invalidate_range(tlb_address, tlb_size);
    cpu->tlb_shootdowns++;
    cpu->ipi_count++;
    __atomic_add_fetch(&tlb_acknowledged, 1u, __ATOMIC_RELEASE);
    apic_eoi();
}

void smp_stop_ipi(void)
{
    smp_cpu_t *cpu = (smp_cpu_t *)smp_current_cpu();
    cpu->ipi_count++;
    cpu->online = 0u;
    __atomic_sub_fetch(&online_cpus, 1u, __ATOMIC_ACQ_REL);
    apic_eoi();
    for (;;) __asm__ volatile ("cli; hlt");
}

int smp_ipi_self_test(void)
{
    uint32_t index;
    for (index = 1u; index < smp_cpu_count(); index++) {
        uint32_t wait;
        cpus[index].reschedule_pending = 0u;
        if (cpus[index].online == 0u || smp_request_reschedule(index) != 0) return -1;
        for (wait = 0u; wait < 1000000u && cpus[index].reschedule_pending == 0u; wait++)
            __asm__ volatile ("pause");
        if (cpus[index].reschedule_pending == 0u) return -1;
        cpus[index].reschedule_pending = 0u;
    }
    return smp_tlb_shootdown((uintptr_t)cpus, sizeof(cpus)) == 0 ? 0 : -1;
}

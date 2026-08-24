#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/apic.h"
#include "mvh/cpu.h"
#include "mvh/interrupt.h"
#include "mvh/io.h"
#include "mvh/memory.h"
#include "mvh/timer.h"

#define IA32_APIC_BASE 0x1Bu
#define IA32_APIC_BASE_ENABLE (1ull << 11u)
#define LAPIC_ID 0x20u
#define LAPIC_VERSION 0x30u
#define LAPIC_EOI 0xB0u
#define LAPIC_SPURIOUS 0xF0u
#define LAPIC_SOFTWARE_ENABLE (1u << 8u)
#define LAPIC_SPURIOUS_VECTOR 0xFFu
#define LAPIC_ICR_LOW 0x300u
#define LAPIC_ICR_HIGH 0x310u
#define LAPIC_ICR_DELIVERY_PENDING (1u << 12u)
#define IOAPIC_REGISTER_SELECT 0x00u
#define IOAPIC_REGISTER_WINDOW 0x10u
#define IOAPIC_ID 0x00u
#define IOAPIC_VERSION 0x01u
#define IOAPIC_REDIRECTION_BASE 0x10u

volatile uint32_t *interrupt_lapic_eoi;
static volatile uint32_t *lapic;
static volatile uint32_t *ioapic;
static apic_status_t state;

static void clear_state(void)
{
    uint8_t *bytes = (uint8_t *)&state;
    uint32_t index;
    for (index = 0u; index < sizeof(state); index++) bytes[index] = 0u;
    lapic = 0;
    ioapic = 0;
    interrupt_lapic_eoi = 0;
}

static uint32_t lapic_read(uint32_t offset)
{
    return lapic[offset / 4u];
}

static void lapic_write(uint32_t offset, uint32_t value)
{
    lapic[offset / 4u] = value;
    (void)lapic[LAPIC_ID / 4u];
}

static int lapic_wait_delivery(void)
{
    uint32_t timeout;
    for (timeout = 0u; timeout < 1000000u; timeout++) {
        if ((lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_DELIVERY_PENDING) == 0u) return 0;
        __asm__ volatile ("pause");
    }
    return -1;
}

static int lapic_send_ipi(uint32_t apic_id, uint32_t command)
{
    if (lapic == 0 || apic_id > 255u || lapic_wait_delivery() != 0) return -1;
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24u);
    lapic_write(LAPIC_ICR_LOW, command);
    return lapic_wait_delivery();
}

static uint32_t ioapic_read(uint8_t reg)
{
    ioapic[IOAPIC_REGISTER_SELECT / 4u] = reg;
    return ioapic[IOAPIC_REGISTER_WINDOW / 4u];
}

static void ioapic_write(uint8_t reg, uint32_t value)
{
    ioapic[IOAPIC_REGISTER_SELECT / 4u] = reg;
    ioapic[IOAPIC_REGISTER_WINDOW / 4u] = value;
}

static void ioapic_route(uint32_t pin, uint8_t vector, uint32_t destination,
                         uint8_t active_low, uint8_t level_triggered, uint8_t masked)
{
    uint32_t low = vector;
    if (active_low != 0u) low |= 1u << 13u;
    if (level_triggered != 0u) low |= 1u << 15u;
    if (masked != 0u) low |= 1u << 16u;
    ioapic_write((uint8_t)(IOAPIC_REDIRECTION_BASE + pin * 2u + 1u),
                 (destination & 0xFFu) << 24u);
    ioapic_write((uint8_t)(IOAPIC_REDIRECTION_BASE + pin * 2u), low);
}

int apic_init(void)
{
    const acpi_status_t *firmware = acpi_status();
    acpi_ioapic_info_t controller;
    acpi_interrupt_override_t override;
    cpu_capabilities_t capabilities;
    uint64_t apic_base;
    uint32_t index;
    uint32_t timer_gsi = 0u;
    uint16_t timer_flags = 0u;
    uint32_t max_redirection;
    clear_state();
    cpu_get_capabilities(&capabilities);
    if (capabilities.apic == 0u || firmware->available == 0u ||
        firmware->lapic_address == 0u || acpi_ioapic_info(0u, &controller) != 0)
        return -1;
    if ((firmware->lapic_address & 0xFFFu) != 0u ||
        (controller.address & 0xFFFu) != 0u) return -1;

    interrupt_disable();
    if (vmm_map_page((uintptr_t)firmware->lapic_address,
                     (uintptr_t)firmware->lapic_address,
                     VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) != 0 ||
        vmm_map_page((uintptr_t)controller.address, (uintptr_t)controller.address,
                     VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) != 0) {
        interrupt_enable();
        return -1;
    }
    lapic = (volatile uint32_t *)(uintptr_t)firmware->lapic_address;
    ioapic = (volatile uint32_t *)(uintptr_t)controller.address;
    if (cpu_rdmsr(IA32_APIC_BASE, &apic_base) == 0u) {
        interrupt_enable();
        clear_state();
        return -1;
    }
    cpu_wrmsr(IA32_APIC_BASE, apic_base | IA32_APIC_BASE_ENABLE);
    lapic_write(LAPIC_SPURIOUS, LAPIC_SOFTWARE_ENABLE | LAPIC_SPURIOUS_VECTOR);

    state.local_apic_id = lapic_read(LAPIC_ID) >> 24u;
    state.local_apic_version = lapic_read(LAPIC_VERSION) & 0xFFu;
    state.ioapic_id = ioapic_read(IOAPIC_ID) >> 24u;
    state.ioapic_version = ioapic_read(IOAPIC_VERSION) & 0xFFu;
    max_redirection = (ioapic_read(IOAPIC_VERSION) >> 16u) & 0xFFu;
    state.ioapic_redirections = max_redirection + 1u;
    for (index = 0u; index <= max_redirection; index++)
        ioapic_route(index, (uint8_t)(0x20u + (index & 0x5Fu)),
                     state.local_apic_id, 0u, 0u, 1u);
    for (index = 0u; index < acpi_interrupt_override_count(); index++) {
        if (acpi_interrupt_override_info(index, &override) == 0 &&
            override.bus == 0u && override.source_irq == 0u) {
            timer_gsi = override.global_interrupt;
            timer_flags = override.flags;
            break;
        }
    }
    if (timer_gsi < controller.global_interrupt_base ||
        timer_gsi - controller.global_interrupt_base > max_redirection) {
        interrupt_enable();
        clear_state();
        return -1;
    }
    ioapic_route(timer_gsi - controller.global_interrupt_base, 0x20u,
                 state.local_apic_id, (timer_flags & 3u) == 3u,
                 ((timer_flags >> 2u) & 3u) == 3u, 0u);
    io_out8(0x21u, 0xFFu);
    io_out8(0xA1u, 0xFFu);
    interrupt_lapic_eoi = &lapic[LAPIC_EOI / 4u];
    state.available = 1u;
    state.local_apic_enabled = 1u;
    state.ioapic_enabled = 1u;
    state.legacy_pic_disabled = 1u;
    state.local_apic_address = firmware->lapic_address;
    state.ioapic_address = controller.address;
    state.timer_gsi = timer_gsi;
    state.timer_vector = 0x20u;
    interrupt_enable();
    return 0;
}

const apic_status_t *apic_status(void)
{
    return &state;
}

void apic_eoi(void)
{
    if (interrupt_lapic_eoi != 0) *interrupt_lapic_eoi = 0u;
}

void apic_init_local_cpu(void)
{
    uint64_t apic_base;
    if (lapic == 0 || cpu_rdmsr(IA32_APIC_BASE, &apic_base) == 0u) return;
    cpu_wrmsr(IA32_APIC_BASE, apic_base | IA32_APIC_BASE_ENABLE);
    lapic_write(LAPIC_SPURIOUS, LAPIC_SOFTWARE_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

int apic_start_application_processor(uint32_t apic_id, uint8_t startup_vector)
{
    if (startup_vector == 0u) return -1;
    if (lapic_send_ipi(apic_id, 0x0000C500u) != 0) return -1;
    timer_sleep_ms(10u);
    if (lapic_send_ipi(apic_id, 0x00008500u) != 0) return -1;
    timer_sleep_ms(10u);
    if (lapic_send_ipi(apic_id, 0x00000600u | startup_vector) != 0) return -1;
    timer_sleep_ms(1u);
    return lapic_send_ipi(apic_id, 0x00000600u | startup_vector);
}

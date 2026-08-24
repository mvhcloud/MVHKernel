#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/hpet.h"
#include "mvh/memory.h"

#define HPET_CAPABILITIES 0x000u
#define HPET_CONFIGURATION 0x010u
#define HPET_MAIN_COUNTER 0x0F0u
#define HPET_ENABLE 1ull

static volatile uint8_t *registers;
static hpet_status_t state;

static uint64_t read64(uint32_t offset)
{
    return *(volatile uint64_t *)(void *)(registers + offset);
}

static void write64(uint32_t offset, uint64_t value)
{
    *(volatile uint64_t *)(void *)(registers + offset) = value;
}

int hpet_init(void)
{
    const acpi_status_t *firmware = acpi_status();
    uint64_t capabilities;
    uint64_t period;
    uint8_t *bytes = (uint8_t *)&state;
    uint32_t index;
    for (index = 0u; index < sizeof(state); index++) bytes[index] = 0u;
    registers = 0;
    if (firmware->available == 0u || firmware->hpet_address == 0u ||
        (firmware->hpet_address & 0xFFFu) != 0u) return -1;
    if (vmm_map_page((uintptr_t)firmware->hpet_address,
                     (uintptr_t)firmware->hpet_address,
                     VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) != 0) return -1;
    registers = (volatile uint8_t *)(uintptr_t)firmware->hpet_address;
    capabilities = read64(HPET_CAPABILITIES);
    period = capabilities >> 32u;
    if (period == 0u || period > 1000000000ull) {
        registers = 0;
        return -1;
    }
    write64(HPET_CONFIGURATION, read64(HPET_CONFIGURATION) & ~HPET_ENABLE);
    write64(HPET_MAIN_COUNTER, 0u);
    write64(HPET_CONFIGURATION, read64(HPET_CONFIGURATION) | HPET_ENABLE);
    state.available = 1u;
    state.enabled = 1u;
    state.revision = (uint8_t)capabilities;
    state.timer_count = (uint8_t)(((capabilities >> 8u) & 0x1Fu) + 1u);
    state.counter_64bit = (capabilities & (1ull << 13u)) != 0u;
    state.legacy_route_capable = (capabilities & (1ull << 15u)) != 0u;
    state.address = firmware->hpet_address;
    state.period_femtoseconds = period;
    state.frequency_hz = 1000000000000000ull / period;
    return 0;
}

const hpet_status_t *hpet_status(void)
{
    return &state;
}

uint64_t hpet_ticks(void)
{
    return state.enabled != 0u ? read64(HPET_MAIN_COUNTER) : 0u;
}

uint64_t hpet_nanoseconds(void)
{
    uint64_t ticks = hpet_ticks();
    return (ticks / 1000000u) * state.period_femtoseconds +
           ((ticks % 1000000u) * state.period_femtoseconds) / 1000000u;
}

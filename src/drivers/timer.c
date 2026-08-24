#include <stdint.h>
#include "mvh/io.h"
#include "mvh/timer.h"

volatile uint64_t timer_ticks_value;
static uint32_t configured_frequency = 100u;

void timer_init(uint32_t frequency)
{
    uint32_t divisor;
    if (frequency < 19u) {
        frequency = 19u;
    }
    if (frequency > 1193182u) {
        frequency = 1193182u;
    }
    divisor = 1193182u / frequency;
    configured_frequency = 1193182u / divisor;
    timer_ticks_value = 0u;
    io_out8(0x43u, 0x36u);
    io_out8(0x40u, (uint8_t)(divisor & 0xFFu));
    io_out8(0x40u, (uint8_t)((divisor >> 8u) & 0xFFu));
}

uint64_t timer_ticks(void)
{
    return timer_ticks_value;
}

uint64_t timer_uptime_seconds(void)
{
    return timer_ticks_value / configured_frequency;
}

uint32_t timer_frequency(void)
{
    return configured_frequency;
}

void timer_sleep_ms(uint64_t milliseconds)
{
    uint64_t required = (milliseconds * configured_frequency + 999u) / 1000u;
    uint64_t target = timer_ticks_value + required;
    while (timer_ticks_value < target) {
        __asm__ volatile ("hlt");
    }
}

#include <stdint.h>
#include "mvh/hal.h"
#include "mvh/interrupt.h"
#include "mvh/io.h"
#include "mvh/keyboard.h"
#include "mvh/pci.h"
#include "mvh/rtc.h"
#include "mvh/serial.h"
#include "mvh/timer.h"
#include "mvh/vga.h"

void hal_init(void)
{
    serial_init();
    vga_init();
    interrupt_init();
    timer_init(100u);
    interrupt_enable();
}

char hal_keyboard_read(void)
{
    return keyboard_read_char();
}

void hal_clock_read(rtc_time_t *time)
{
    rtc_read(time);
}

uint32_t hal_pci_scan(pci_device_t *devices, uint32_t capacity)
{
    return pci_scan(devices, capacity);
}

uint64_t hal_ticks(void)
{
    return timer_ticks();
}

uint64_t hal_uptime_seconds(void)
{
    return timer_uptime_seconds();
}

uint32_t hal_timer_frequency(void)
{
    return timer_frequency();
}

void hal_sleep_ms(uint64_t milliseconds)
{
    timer_sleep_ms(milliseconds);
}

void hal_reboot(void)
{
    interrupt_disable();
    while ((io_in8(0x64u) & 2u) != 0u) {
    }
    io_out8(0x64u, 0xFEu);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

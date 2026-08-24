#ifndef MVH_HAL_H
#define MVH_HAL_H

#include <stdint.h>
#include "mvh/pci.h"
#include "mvh/rtc.h"

void hal_init(void);
char hal_keyboard_read(void);
void hal_clock_read(rtc_time_t *time);
uint32_t hal_pci_scan(pci_device_t *devices, uint32_t capacity);
uint64_t hal_ticks(void);
uint64_t hal_uptime_seconds(void);
uint32_t hal_timer_frequency(void);
void hal_sleep_ms(uint64_t milliseconds);
void hal_reboot(void) __attribute__((noreturn));

#endif

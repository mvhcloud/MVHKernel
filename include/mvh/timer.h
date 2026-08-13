#ifndef MVH_TIMER_H
#define MVH_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency);
uint64_t timer_ticks(void);
uint64_t timer_uptime_seconds(void);
uint32_t timer_frequency(void);
void timer_sleep_ms(uint64_t milliseconds);

#endif

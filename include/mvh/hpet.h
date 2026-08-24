#ifndef MVH_HPET_H
#define MVH_HPET_H

#include <stdint.h>

typedef struct {
    uint8_t available;
    uint8_t enabled;
    uint8_t counter_64bit;
    uint8_t legacy_route_capable;
    uint8_t timer_count;
    uint8_t revision;
    uint64_t address;
    uint64_t period_femtoseconds;
    uint64_t frequency_hz;
} hpet_status_t;

int hpet_init(void);
const hpet_status_t *hpet_status(void);
uint64_t hpet_ticks(void);
uint64_t hpet_nanoseconds(void);

#endif

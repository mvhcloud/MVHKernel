#ifndef MVH_INTERRUPT_H
#define MVH_INTERRUPT_H

#include <stdint.h>

void interrupt_init(void);
void interrupt_enable(void);
void interrupt_disable(void);
void exception_dispatch(void *frame) __attribute__((noreturn));
uint64_t interrupt_count(uint8_t vector);
uint64_t interrupt_total(void);
uint64_t interrupt_spurious_count(void);

#endif

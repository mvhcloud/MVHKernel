#ifndef MVH_INTERRUPT_H
#define MVH_INTERRUPT_H

void interrupt_init(void);
void interrupt_enable(void);
void interrupt_disable(void);
void exception_dispatch(void *frame) __attribute__((noreturn));

#endif

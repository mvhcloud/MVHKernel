#include <stdint.h>
#include "mvh/interrupt.h"
#include "mvh/io.h"
#include "mvh/panic.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_pointer_t;

static idt_entry_t idt[256];
volatile uint64_t interrupt_counters[256];
static uint64_t spurious_interrupts;

extern void irq_timer_entry(void);
extern void exception_stub_0(void);
extern void exception_stub_1(void);
extern void exception_stub_2(void);
extern void exception_stub_3(void);
extern void exception_stub_4(void);
extern void exception_stub_5(void);
extern void exception_stub_6(void);
extern void exception_stub_7(void);
extern void exception_stub_8(void);
extern void exception_stub_9(void);
extern void exception_stub_10(void);
extern void exception_stub_11(void);
extern void exception_stub_12(void);
extern void exception_stub_13(void);
extern void exception_stub_14(void);
extern void exception_stub_15(void);
extern void exception_stub_16(void);
extern void exception_stub_17(void);
extern void exception_stub_18(void);
extern void exception_stub_19(void);
extern void exception_stub_20(void);
extern void exception_stub_21(void);
extern void exception_stub_22(void);
extern void exception_stub_23(void);
extern void exception_stub_24(void);
extern void exception_stub_25(void);
extern void exception_stub_26(void);
extern void exception_stub_27(void);
extern void exception_stub_28(void);
extern void exception_stub_29(void);
extern void exception_stub_30(void);
extern void exception_stub_31(void);

static void (*const exception_stubs[32])(void) = {
    exception_stub_0, exception_stub_1, exception_stub_2, exception_stub_3,
    exception_stub_4, exception_stub_5, exception_stub_6, exception_stub_7,
    exception_stub_8, exception_stub_9, exception_stub_10, exception_stub_11,
    exception_stub_12, exception_stub_13, exception_stub_14, exception_stub_15,
    exception_stub_16, exception_stub_17, exception_stub_18, exception_stub_19,
    exception_stub_20, exception_stub_21, exception_stub_22, exception_stub_23,
    exception_stub_24, exception_stub_25, exception_stub_26, exception_stub_27,
    exception_stub_28, exception_stub_29, exception_stub_30, exception_stub_31
};

static void idt_set(uint8_t vector, void (*handler)(void))
{
    uint64_t address = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = 0x08u;
    idt[vector].ist = 0u;
    idt[vector].attributes = 0x8Eu;
    idt[vector].offset_middle = (uint16_t)(address >> 16u);
    idt[vector].offset_high = (uint32_t)(address >> 32u);
    idt[vector].reserved = 0u;
}

static void pic_remap(void)
{
    uint8_t master_mask = io_in8(0x21u);
    uint8_t slave_mask = io_in8(0xA1u);
    io_out8(0x20u, 0x11u);
    io_out8(0xA0u, 0x11u);
    io_out8(0x21u, 0x20u);
    io_out8(0xA1u, 0x28u);
    io_out8(0x21u, 0x04u);
    io_out8(0xA1u, 0x02u);
    io_out8(0x21u, 0x01u);
    io_out8(0xA1u, 0x01u);
    io_out8(0x21u, master_mask);
    io_out8(0xA1u, slave_mask);
}

void interrupt_init(void)
{
    idt_pointer_t pointer;
    uint16_t index;
    interrupt_disable();
    for (index = 0u; index < 256u; index++) {
        interrupt_counters[index] = 0u;
        idt[index].offset_low = 0u;
        idt[index].selector = 0u;
        idt[index].ist = 0u;
        idt[index].attributes = 0u;
        idt[index].offset_middle = 0u;
        idt[index].offset_high = 0u;
        idt[index].reserved = 0u;
    }
    for (index = 0u; index < 32u; index++) {
        idt_set((uint8_t)index, exception_stubs[index]);
    }
    idt_set(32u, irq_timer_entry);
    pointer.limit = (uint16_t)(sizeof(idt) - 1u);
    pointer.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(pointer));
    pic_remap();
    io_out8(0x21u, 0xFEu);
    io_out8(0xA1u, 0xFFu);
    spurious_interrupts = 0u;
}

uint64_t interrupt_count(uint8_t vector)
{
    return interrupt_counters[vector];
}

uint64_t interrupt_total(void)
{
    uint64_t total = 0u;
    uint16_t index;
    for (index = 0u; index < 256u; index++) total += interrupt_counters[index];
    return total;
}

uint64_t interrupt_spurious_count(void)
{
    return spurious_interrupts;
}

void exception_dispatch(void *frame)
{
    kernel_panic_exception((const exception_frame_t *)frame);
}

void interrupt_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupt_disable(void)
{
    __asm__ volatile ("cli");
}

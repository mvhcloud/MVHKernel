#include <stdint.h>
#include "mvh/interrupt.h"
#include "mvh/io.h"

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

extern void irq_timer_entry(void);

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
        idt[index].offset_low = 0u;
        idt[index].selector = 0u;
        idt[index].ist = 0u;
        idt[index].attributes = 0u;
        idt[index].offset_middle = 0u;
        idt[index].offset_high = 0u;
        idt[index].reserved = 0u;
    }
    idt_set(32u, irq_timer_entry);
    pointer.limit = (uint16_t)(sizeof(idt) - 1u);
    pointer.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(pointer));
    pic_remap();
    io_out8(0x21u, 0xFEu);
    io_out8(0xA1u, 0xFFu);
}

void interrupt_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupt_disable(void)
{
    __asm__ volatile ("cli");
}

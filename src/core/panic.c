#include <stdint.h>
#include "mvh/interrupt.h"
#include "mvh/log.h"
#include "mvh/panic.h"
#include "mvh/serial.h"
#include "mvh/vga.h"

static const char *exception_names[32] = {
    "Divide Error", "Debug", "Non-maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection Exception", "VMM Communication Exception", "Security Exception", "Reserved"
};

static void panic_put(char value)
{
    vga_put(value);
    serial_put(value);
}

static void panic_write(const char *text)
{
    while (*text != '\0') {
        panic_put(*text++);
    }
}

static void panic_hex(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    panic_write("0x");
    for (shift = 60; shift >= 0; shift -= 4) {
        panic_put(digits[(value >> (uint32_t)shift) & 0x0Fu]);
    }
}

static void panic_halt(void) __attribute__((noreturn));

static void panic_halt(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kernel_panic(const char *message)
{
    interrupt_disable();
    klog_write("PANIC", message);
    vga_cursor_disable();
    vga_set_color(0x4Fu);
    panic_write("\n================ MVH KERNEL PANIC ================\n");
    vga_set_color(0x0Fu);
    panic_write(message);
    panic_write("\nSystem halted safely.\n");
    panic_halt();
}

void kernel_panic_exception(const exception_frame_t *frame)
{
    const char *name = frame->vector < 32u ? exception_names[frame->vector] : "Unknown Exception";
    uint64_t fault_address = 0u;
    if (frame->vector == 14u) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
    }
    interrupt_disable();
    klog_write("PANIC", name);
    vga_cursor_disable();
    vga_set_color(0x4Fu);
    panic_write("\n================ MVH KERNEL PANIC ================\n");
    vga_set_color(0x0Fu);
    panic_write("CPU exception: ");
    panic_write(name);
    panic_write("\nVector: ");
    panic_hex(frame->vector);
    panic_write("  Error: ");
    panic_hex(frame->error_code);
    panic_write("\nRIP: ");
    panic_hex(frame->rip);
    panic_write("  RFLAGS: ");
    panic_hex(frame->rflags);
    if (frame->vector == 14u) {
        panic_write("\nCR2: ");
        panic_hex(fault_address);
    }
    panic_write("\nSystem halted safely.\n");
    panic_halt();
}

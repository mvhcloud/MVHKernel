#include <stdint.h>
#include "mvh/interrupt.h"
#include "mvh/assert.h"
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

static void panic_hex8(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    panic_put(digits[value >> 4u]);
    panic_put(digits[value & 0x0Fu]);
}

static void panic_number(uint64_t value)
{
    char digits[21];
    uint32_t length = 0u;
    if (value == 0u) {
        panic_put('0');
        return;
    }
    while (value != 0u) {
        digits[length++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (length != 0u) panic_put(digits[--length]);
}

static void panic_bit(const char *name, uint64_t value, uint32_t bit)
{
    panic_write(name);
    panic_write((value & (1ull << bit)) != 0u ? "=1 " : "=0 ");
}

static void panic_decode_error(const exception_frame_t *frame)
{
    uint64_t error = frame->error_code;
    if (frame->vector == 14u) {
        panic_write("\nPage fault: ");
        panic_write((error & 1u) != 0u ? "protection violation, " : "non-present page, ");
        panic_write((error & 2u) != 0u ? "write, " : "read, ");
        panic_write((error & 4u) != 0u ? "user, " : "supervisor, ");
        panic_write((error & 8u) != 0u ? "reserved-bit, " : "reserved-bits-ok, ");
        panic_write((error & 16u) != 0u ? "instruction fetch" : "data access");
        panic_write("\nPF flags: ");
        panic_bit("P", error, 0u); panic_bit("W", error, 1u);
        panic_bit("U", error, 2u); panic_bit("R", error, 3u);
        panic_bit("I", error, 4u); panic_bit("PK", error, 5u);
        panic_bit("SS", error, 6u); panic_bit("SGX", error, 15u);
    } else if (frame->vector == 10u || frame->vector == 11u ||
               frame->vector == 12u || frame->vector == 13u) {
        panic_write("\nSelector error: ");
        panic_bit("EXT", error, 0u); panic_bit("IDT", error, 1u);
        panic_bit("TI", error, 2u);
        panic_write("INDEX=");
        panic_hex(error >> 3u);
    } else if (frame->vector == 21u) {
        panic_write("\nControl-protection error: ");
        panic_hex(error);
    } else if (frame->vector == 8u || frame->vector == 17u) {
        panic_write("\nArchitectural error code is zero for this exception.");
    }
}

static void panic_stack_trace(const exception_frame_t *frame)
{
    const uint64_t *base = (const uint64_t *)(uintptr_t)frame->rbp;
    uint64_t previous = 0u;
    uint32_t depth;
    panic_write("\nStack trace:\n  #0 ");
    panic_hex(frame->rip);
    for (depth = 1u; depth < 12u; depth++) {
        uintptr_t address = (uintptr_t)base;
        uint64_t next;
        uint64_t return_address;
        if (address < 0x1000u || address >= 0x40000000u ||
            (address & (sizeof(uint64_t) - 1u)) != 0u || address <= previous) break;
        next = base[0];
        return_address = base[1];
        if (return_address < 0x200000u || return_address >= 0x40000000u) break;
        panic_write("\n  #");
        panic_put((char)('0' + depth % 10u));
        panic_write(" ");
        panic_hex(return_address);
        previous = address;
        base = (const uint64_t *)(uintptr_t)next;
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
    panic_write("Panic code: MVH-KERNEL-0001\n");
    panic_write(message);
    panic_write("\nSystem halted safely.\n");
    panic_halt();
}

void kernel_assert_fail(const char *expression, const char *file, unsigned int line)
{
    interrupt_disable();
    klog_write("FATAL", "kernel assertion failed");
    vga_cursor_disable();
    vga_set_color(0x4Fu);
    panic_write("\n================ MVH KERNEL PANIC ================\n");
    vga_set_color(0x0Fu);
    panic_write("Panic code: MVH-ASSERT-0001\nAssertion: ");
    panic_write(expression);
    panic_write("\nLocation: ");
    panic_write(file);
    panic_write(":");
    panic_number(line);
    panic_write("\nSystem halted safely.\n");
    panic_halt();
}

void kernel_panic_exception(const exception_frame_t *frame)
{
    const char *name = frame->vector < 32u ? exception_names[frame->vector] : "Unknown Exception";
    uint64_t fault_address = 0u;
    uint64_t cr0;
    uint64_t cr3;
    uint64_t cr4;
    if (frame->vector == 14u) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_address));
    }
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    interrupt_disable();
    klog_write("PANIC", name);
    vga_cursor_disable();
    vga_set_color(0x4Fu);
    panic_write("\n================ MVH KERNEL PANIC ================\n");
    vga_set_color(0x0Fu);
    panic_write("CPU exception: ");
    panic_write(name);
    panic_write("\nPanic code: MVH-EX-");
    panic_hex8((uint8_t)frame->vector);
    panic_write("\nVector: ");
    panic_hex(frame->vector);
    panic_write("  Error: ");
    panic_hex(frame->error_code);
    panic_decode_error(frame);
    panic_write("\nRIP: ");
    panic_hex(frame->rip);
    panic_write("  RFLAGS: ");
    panic_hex(frame->rflags);
    if (frame->vector == 14u) {
        panic_write("\nCR2: ");
        panic_hex(fault_address);
    }
    panic_write("\nCR0: ");
    panic_hex(cr0);
    panic_write("  CR3: ");
    panic_hex(cr3);
    panic_write("\nCR4: ");
    panic_hex(cr4);
    panic_write("\nRAX: "); panic_hex(frame->rax);
    panic_write("  RBX: "); panic_hex(frame->rbx);
    panic_write("\nRCX: "); panic_hex(frame->rcx);
    panic_write("  RDX: "); panic_hex(frame->rdx);
    panic_write("\nRSI: "); panic_hex(frame->rsi);
    panic_write("  RDI: "); panic_hex(frame->rdi);
    panic_write("\nRBP: "); panic_hex(frame->rbp);
    panic_write("  RSP*: "); panic_hex((uint64_t)(uintptr_t)frame);
    panic_write("\nR8 : "); panic_hex(frame->r8);
    panic_write("  R9 : "); panic_hex(frame->r9);
    panic_write("\nR10: "); panic_hex(frame->r10);
    panic_write("  R11: "); panic_hex(frame->r11);
    panic_write("\nR12: "); panic_hex(frame->r12);
    panic_write("  R13: "); panic_hex(frame->r13);
    panic_write("\nR14: "); panic_hex(frame->r14);
    panic_write("  R15: "); panic_hex(frame->r15);
    panic_stack_trace(frame);
    panic_write("\nSystem halted safely.\n");
    panic_halt();
}

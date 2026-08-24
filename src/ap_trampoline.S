.section .text.ap_trampoline,"ax",@progbits
.code16
.global ap_trampoline_start
.global ap_trampoline_end
.global ap_trampoline_cr3
.global ap_trampoline_stack
.global ap_trampoline_cpu
.global ap_trampoline_entry

.set AP_BASE, 0x8000
.set AP_PROTECTED, 0x40
.set AP_LONG, 0xA0
.set AP_DATA, 0x100
.set AP_GDT, 0x140

ap_trampoline_start:
    cli
    cld
    xor %ax, %ax
    mov %ax, %ss
    mov $0x7000, %sp
    mov $0x0800, %ax
    mov %ax, %ds
    lgdtl (AP_GDT + 24)
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmpl $0x08, $(AP_BASE + AP_PROTECTED)

.org AP_PROTECTED
.code32
ap_trampoline_protected:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov (AP_BASE + AP_DATA), %eax
    mov %eax, %cr3
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0
    ljmpl $0x08, $(AP_BASE + AP_LONG)

.org AP_LONG
.code64
ap_trampoline_long:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    movabs $(AP_BASE + AP_DATA + 8), %rax
    mov (%rax), %rsp
    and $-16, %rsp
    movabs $(AP_BASE + AP_DATA + 16), %rax
    mov (%rax), %rdi
    movabs $(AP_BASE + AP_DATA + 24), %rax
    mov (%rax), %rax
    jmp *%rax

.org AP_DATA
ap_trampoline_cr3:
    .quad 0
ap_trampoline_stack:
    .quad 0
ap_trampoline_cpu:
    .quad 0
ap_trampoline_entry:
    .quad 0

.org AP_GDT
ap_trampoline_gdt:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
ap_trampoline_gdt_end:
ap_trampoline_gdt_pointer:
    .word ap_trampoline_gdt_end - ap_trampoline_gdt - 1
    .long AP_BASE + AP_GDT

ap_trampoline_end:

.section .note.GNU-stack,"",@progbits

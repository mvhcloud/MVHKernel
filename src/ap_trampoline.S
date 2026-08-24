.section .text.ap_trampoline,"ax",@progbits
.code16
.global ap_trampoline_start
.global ap_trampoline_end
.global ap_trampoline_cr3
.global ap_trampoline_stack
.global ap_trampoline_cpu
.global ap_trampoline_entry

.set AP_BASE, 0x8000

ap_trampoline_start:
    cli
    cld
    xor %ax, %ax
    mov %ax, %ss
    mov $0x7000, %sp
    mov $0x0800, %ax
    mov %ax, %ds
    lgdtl (ap_trampoline_gdt_pointer-ap_trampoline_start)
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmpl $0x08, $(AP_BASE + ap_trampoline_protected-ap_trampoline_start)

.code32
ap_trampoline_protected:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov (AP_BASE + ap_trampoline_cr3-ap_trampoline_start), %eax
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
    ljmpl $0x08, $(AP_BASE + ap_trampoline_long-ap_trampoline_start)

.code64
ap_trampoline_long:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    movabs $(AP_BASE + ap_trampoline_stack-ap_trampoline_start), %rax
    mov (%rax), %rsp
    and $-16, %rsp
    movabs $(AP_BASE + ap_trampoline_cpu-ap_trampoline_start), %rax
    mov (%rax), %rdi
    movabs $(AP_BASE + ap_trampoline_entry-ap_trampoline_start), %rax
    mov (%rax), %rax
    jmp *%rax

.align 8
ap_trampoline_cr3:
    .quad 0
ap_trampoline_stack:
    .quad 0
ap_trampoline_cpu:
    .quad 0
ap_trampoline_entry:
    .quad 0

.align 8
ap_trampoline_gdt:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
ap_trampoline_gdt_end:
ap_trampoline_gdt_pointer:
    .word ap_trampoline_gdt_end - ap_trampoline_gdt - 1
    .long AP_BASE + ap_trampoline_gdt - ap_trampoline_start

ap_trampoline_end:

.section .note.GNU-stack,"",@progbits

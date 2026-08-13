.section .text,"ax",@progbits
.code64
.global irq_timer_entry
.type irq_timer_entry, @function
.extern timer_ticks_value
.extern interrupt_counters
irq_timer_entry:
    push %rax
    incq timer_ticks_value(%rip)
    incq interrupt_counters+256(%rip)
    mov $0x20, %al
    out %al, $0x20
    pop %rax
    iretq
.size irq_timer_entry, . - irq_timer_entry

.macro EXCEPTION_NO_ERROR vector
.global exception_stub_\vector
.type exception_stub_\vector, @function
exception_stub_\vector:
    pushq $0
    pushq $\vector
    jmp exception_common
.size exception_stub_\vector, . - exception_stub_\vector
.endm

.macro EXCEPTION_WITH_ERROR vector
.global exception_stub_\vector
.type exception_stub_\vector, @function
exception_stub_\vector:
    pushq $\vector
    jmp exception_common
.size exception_stub_\vector, . - exception_stub_\vector
.endm

exception_common:
    cld
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    lea interrupt_counters(%rip), %rax
    mov 120(%rsp), %rcx
    incq (%rax,%rcx,8)
    mov %rsp, %rdi
    and $-16, %rsp
    call exception_dispatch
    cli
1:
    hlt
    jmp 1b

EXCEPTION_NO_ERROR 0
EXCEPTION_NO_ERROR 1
EXCEPTION_NO_ERROR 2
EXCEPTION_NO_ERROR 3
EXCEPTION_NO_ERROR 4
EXCEPTION_NO_ERROR 5
EXCEPTION_NO_ERROR 6
EXCEPTION_NO_ERROR 7
EXCEPTION_WITH_ERROR 8
EXCEPTION_NO_ERROR 9
EXCEPTION_WITH_ERROR 10
EXCEPTION_WITH_ERROR 11
EXCEPTION_WITH_ERROR 12
EXCEPTION_WITH_ERROR 13
EXCEPTION_WITH_ERROR 14
EXCEPTION_NO_ERROR 15
EXCEPTION_NO_ERROR 16
EXCEPTION_WITH_ERROR 17
EXCEPTION_NO_ERROR 18
EXCEPTION_NO_ERROR 19
EXCEPTION_NO_ERROR 20
EXCEPTION_WITH_ERROR 21
EXCEPTION_NO_ERROR 22
EXCEPTION_NO_ERROR 23
EXCEPTION_NO_ERROR 24
EXCEPTION_NO_ERROR 25
EXCEPTION_NO_ERROR 26
EXCEPTION_NO_ERROR 27
EXCEPTION_NO_ERROR 28
EXCEPTION_WITH_ERROR 29
EXCEPTION_WITH_ERROR 30
EXCEPTION_NO_ERROR 31

.section .note.GNU-stack,"",@progbits

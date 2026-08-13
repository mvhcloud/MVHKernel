.section .text,"ax",@progbits
.code64
.global irq_timer_entry
.type irq_timer_entry, @function
.extern timer_ticks_value
irq_timer_entry:
    push %rax
    incq timer_ticks_value(%rip)
    mov $0x20, %al
    out %al, $0x20
    pop %rax
    iretq
.size irq_timer_entry, . - irq_timer_entry

.section .note.GNU-stack,"",@progbits

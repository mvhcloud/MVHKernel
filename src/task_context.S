.section .text,"ax",@progbits
.code64
.global task_context_switch
.type task_context_switch, @function
task_context_switch:
    mov %rsp, 0(%rdi)
    mov %rbx, 8(%rdi)
    mov %rbp, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %r13, 32(%rdi)
    mov %r14, 40(%rdi)
    mov %r15, 48(%rdi)
    mov 0(%rsi), %rsp
    mov 8(%rsi), %rbx
    mov 16(%rsi), %rbp
    mov 24(%rsi), %r12
    mov 32(%rsi), %r13
    mov 40(%rsi), %r14
    mov 48(%rsi), %r15
    ret
.size task_context_switch, . - task_context_switch

.section .note.GNU-stack,"",@progbits

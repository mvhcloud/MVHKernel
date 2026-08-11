.section .text.entry,"ax",@progbits
.code64
.global _kernel64_start
.type _kernel64_start, @function
_kernel64_start:
    cld
    xor %rbp, %rbp
    mov %cr0, %rax
    and $-5, %rax
    or $2, %rax
    mov %rax, %cr0
    mov %cr4, %rax
    or $0x600, %rax
    mov %rax, %cr4
    mov %edi, %r8d
    mov %esi, %r9d
    lea __bss_start(%rip), %rdi
    lea __bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %eax, %eax
    rep stosb
    mov %r8d, %edi
    mov %r9d, %esi
    call kernel_main

halt64:
    cli
    hlt
    jmp halt64

.size _kernel64_start, . - _kernel64_start

.section .note.GNU-stack,"",@progbits

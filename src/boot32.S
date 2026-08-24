.section .multiboot,"a",@progbits
.align 4
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)

.section .text.boot,"ax",@progbits
.code32
.global _multiboot_start
.type _multiboot_start, @function
_multiboot_start:
    cli
    cld
    mov $bootstrap_stack_top, %esp
    xor %ebp, %ebp

    /* Multiboot supplies upper memory in KiB when information flag 0 is set. */
    movl $131072, bootstrap_memory_kib
    cmpl $0x2BADB002, %eax
    jne 1f
    testl $1, (%ebx)
    jz 1f
    mov 8(%ebx), %eax
    add $1024, %eax
    mov %eax, bootstrap_memory_kib
1:
    /* Identity-map the first GiB with 2 MiB pages. */
    mov $bootstrap_pd, %edi
    xor %ecx, %ecx
2:
    mov %ecx, %eax
    shl $21, %eax
    or $0x83, %eax
    mov %eax, (%edi,%ecx,8)
    movl $0, 4(%edi,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne 2b

    mov $bootstrap_pd, %eax
    or $3, %eax
    mov %eax, bootstrap_pdpt
    movl $0, bootstrap_pdpt+4
    mov $bootstrap_pdpt, %eax
    or $3, %eax
    mov %eax, bootstrap_pml4
    movl $0, bootstrap_pml4+4

    lgdt bootstrap_gdt_pointer
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    mov $bootstrap_pml4, %eax
    mov %eax, %cr3
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    ljmp $0x08, $bootstrap_long_mode

.code64
bootstrap_long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    xor %eax, %eax
    mov %ax, %fs
    mov %ax, %gs
    mov $bootstrap_stack_top, %rsp
    mov bootstrap_memory_kib(%rip), %edi
    xor %esi, %esi
    jmp _kernel64_start
.size _multiboot_start, . - _multiboot_start

.section .rodata.boot,"a",@progbits
.align 8
bootstrap_gdt:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
bootstrap_gdt_end:
bootstrap_gdt_pointer:
    .word bootstrap_gdt_end - bootstrap_gdt - 1
    .long bootstrap_gdt

.section .boot.data,"aw",@progbits
.align 8
bootstrap_memory_kib:
    .long 131072

.section .boot.bss,"aw",@nobits
.align 4096
bootstrap_pml4:
    .skip 4096
bootstrap_pdpt:
    .skip 4096
bootstrap_pd:
    .skip 4096
.align 16
bootstrap_stack:
    .skip 32768
bootstrap_stack_top:

.section .note.GNU-stack,"",@progbits

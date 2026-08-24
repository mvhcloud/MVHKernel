.section .multiboot,"a",@progbits
.align 4
.long 0x1BADB002
.long 0x00000007
.long -(0x1BADB002 + 0x00000007)
.long 0
.long 1024
.long 768
.long 32

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
    movl $0, bootstrap_info_flags
    cmpl $0x2BADB002, %eax
    jne bootstrap_paging
    mov %ebx, %esi
    testl $1, (%ebx)
    jz bootstrap_optional_info
    mov 8(%ebx), %eax
    add $1024, %eax
    mov %eax, bootstrap_memory_kib
bootstrap_optional_info:
    testl $4, (%esi)
    jz bootstrap_framebuffer
    mov 16(%esi), %edi
    test %edi, %edi
    jz bootstrap_framebuffer
    mov %edi, bootstrap_command_line
    movl $0, bootstrap_command_line+4
    xor %ecx, %ecx
1:
    cmp $4095, %ecx
    je 2f
    cmpb $0, (%edi,%ecx)
    je 2f
    inc %ecx
    jmp 1b
2:
    mov %ecx, bootstrap_command_line_size
    orl $0x20, bootstrap_info_flags
bootstrap_framebuffer:
    testl $0x1000, (%esi)
    jz bootstrap_paging
    cmpb $1, 109(%esi)
    jne bootstrap_paging
    movzbl 108(%esi), %eax
    cmp $24, %eax
    je 3f
    cmp $32, %eax
    jne bootstrap_paging
3:
    mov 88(%esi), %eax
    mov %eax, bootstrap_framebuffer_address
    mov 92(%esi), %eax
    mov %eax, bootstrap_framebuffer_address+4
    mov 100(%esi), %eax
    mov %eax, bootstrap_framebuffer_width
    mov 104(%esi), %eax
    mov %eax, bootstrap_framebuffer_height
    mov 96(%esi), %eax
    mov %eax, bootstrap_framebuffer_pitch
    movzbl 108(%esi), %eax
    mov %eax, bootstrap_framebuffer_bpp
    orl $0x08, bootstrap_info_flags
bootstrap_paging:
    /* Identity-map the first GiB with 2 MiB pages. */
    mov $bootstrap_pd, %edi
    xor %ecx, %ecx
4:
    mov %ecx, %eax
    shl $21, %eax
    or $0x83, %eax
    mov %eax, (%edi,%ecx,8)
    movl $0, 4(%edi,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne 4b

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
    mov bootstrap_memory_kib(%rip), %eax
    mov %rax, bootstrap_bootinfo+24(%rip)
    lea bootstrap_bootinfo(%rip), %rdi
    mov $0x32564F464E494856, %rsi
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
bootstrap_bootinfo:
    .quad 0x324F464E4948564D
    .long 2
    .long 136
bootstrap_info_flags:
    .quad 0
    .quad 131072
    .quad 0
    .long 0
    .long 0
    .quad 0
    .quad 0
bootstrap_framebuffer_address:
    .quad 0
bootstrap_framebuffer_width:
    .long 0
bootstrap_framebuffer_height:
    .long 0
bootstrap_framebuffer_pitch:
    .long 0
bootstrap_framebuffer_bpp:
    .long 0
    .long 0
    .long 0
    .long 0
    .long 0
    .quad 0
    .long 0
    .long 0
bootstrap_command_line:
    .quad 0
bootstrap_command_line_size:
    .long 0
    .long 0

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

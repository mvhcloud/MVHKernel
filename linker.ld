ENTRY(_multiboot_start)

SECTIONS
{
    . = 2M;
    __kernel_start = .;

    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) :
    {
        __text_start = .;
        KEEP(*(.text.boot))
        KEEP(*(.text.entry))
        *(.text*)
        __text_end = .;
    }

    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    }

    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data*)
        __data_end = .;
    }

    .boot.bss ALIGN(4K) (NOLOAD) :
    {
        *(.boot.bss)
    }

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    __kernel_end = .;
}

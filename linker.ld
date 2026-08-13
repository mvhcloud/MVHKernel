ENTRY(_kernel64_start)

SECTIONS
{
    . = 2M;
    __kernel_start = .;

    .text ALIGN(4K) :
    {
        __text_start = .;
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

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    __kernel_end = .;
}

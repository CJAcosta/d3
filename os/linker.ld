/*
 * Link the kernel to run at 0x10000, which is where the boot sector
 * deposits it. Identity-mapped paging keeps this address valid after the
 * MMU comes up, so physical == virtual throughout the kernel.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    __kernel_start = .;

    .text : ALIGN(4096)
    {
        *(.text.boot)       /* _start must be the very first byte */
        *(.text*)
    }

    .rodata : ALIGN(4096)
    {
        *(.rodata*)
    }

    .data : ALIGN(4096)
    {
        *(.data*)
    }

    . = ALIGN(4096);
    __bss_start = .;
    .bss : ALIGN(4096)
    {
        *(COMMON)
        *(.bss*)
    }
    . = ALIGN(4096);
    __bss_end = .;

    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.comment*)
        *(.note*)
    }
}

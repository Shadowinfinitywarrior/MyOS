/* Linker script for user-space programs */
ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* User programs loaded at 0x08048000 (Linux convention) */
    . = 0x08048000;

    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}


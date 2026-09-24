/* User-space linker script: 64-bit, text base at 0x40000000.
 * Must stay within the kernel's user VMA band
 * [ELF_USER_VMA_MIN=0x40000000, ELF_USER_VMA_MAX=0xBFFFF000). */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* Align every segment to a page so the loader's page-granular
     * mappings fully contain each segment. */
    . = 0x40000000;
    .text : ALIGN(4K) {
        *(.text*)
        *(.rodata*)
    }
    . = ALIGN(4K);
    .data : ALIGN(4K) {
        *(.data*)
    }
    . = ALIGN(4K);
    .bss : ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
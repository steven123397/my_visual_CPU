ENTRY(_start)

SECTIONS {
    . = 0x80000000;

    .text : {
        *(.text.start)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss (NOLOAD) : {
        . = ALIGN(16);
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
    }
}

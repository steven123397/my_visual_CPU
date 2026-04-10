ENTRY(_start)

SECTIONS {
    . = 0x80000000;

    . = ALIGN(4096);
    .text : {
        *(.text.start)
        *(.text*)
        . = ALIGN(16);
    }

    . = ALIGN(16);
    .rodata : {
        *(.rodata*)
        . = ALIGN(16);
    }

    . = ALIGN(16);
    .data : {
        *(.data*)
        . = ALIGN(16);
    }

    . = ALIGN(16);
    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
    }
}

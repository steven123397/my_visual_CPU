ENTRY(_start)

SECTIONS {
    . = 0x80000000;
    __kernel_start = .;

    . = ALIGN(4096);
    .text : {
        __text_start = .;
        *(.text.start)
        *(.text*)
        . = ALIGN(4096);
        __text_end = .;
    }

    . = ALIGN(4096);
    .rodata : {
        __rodata_start = .;
        *(.rodata*)
        . = ALIGN(4096);
        __rodata_end = .;
    }

    . = ALIGN(4096);
    .data : {
        __data_start = .;
        *(.data*)
        . = ALIGN(4096);
        __data_end = .;
    }

    . = ALIGN(4096);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
        __bss_end = .;
    }

    __kernel_end = .;
    __heap_start = ALIGN(4096);
}

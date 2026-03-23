#include "console.h"

#include <stdint.h>

#include "platform.h"

void console_putc(char ch) {
    platform_uart_putc((uint8_t)ch);
}

void console_puts(const char* s) {
    while (*s != '\0') {
        console_putc(*s++);
    }
}

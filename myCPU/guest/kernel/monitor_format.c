#include "monitor.h"

#include <stddef.h>

#include "console.h"

void monitor_write_char(char ch) {
    char text[2];

    text[0] = ch;
    text[1] = '\0';
    monitor_write_text(text);
}

void monitor_write_text(const char* text) {
    console_puts(text);
}

void monitor_write_line(const char* text) {
    monitor_write_text(text);
    console_putc('\r');
    console_putc('\n');
}

void monitor_write_banner(void) {
    monitor_write_text("\r\ninteractive monitor\r\n");
}

void monitor_write_prompt(void) {
    monitor_write_text("monitor> ");
}

void monitor_write_uint64(uint64_t value) {
    char buffer[32];
    size_t index = sizeof(buffer) - 1U;

    buffer[index] = '\0';
    do {
        buffer[--index] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && index > 0U);

    monitor_write_text(&buffer[index]);
}

void monitor_write_hex64(uint64_t value) {
    char buffer[19];
    static const char kHexDigits[] = "0123456789abcdef";
    size_t start = 0U;
    size_t index;

    buffer[0] = '0';
    buffer[1] = 'x';
    for (index = 0; index < 16U; ++index) {
        const unsigned shift = (unsigned)((15U - index) * 4U);
        buffer[2U + index] = kHexDigits[(value >> shift) & 0xfU];
    }
    buffer[18] = '\0';

    while (start < 15U && buffer[2U + start] == '0') {
        ++start;
    }

    monitor_write_text("0x");
    monitor_write_text(&buffer[2U + start]);
}

void monitor_write_preview_ascii(const uint8_t* data, size_t length) {
    size_t index;

    for (index = 0; index < length; ++index) {
        const uint8_t byte = data[index];

        if (byte == 0U) {
            break;
        }
        if (byte < 0x20U || byte > 0x7eU) {
            monitor_write_char('.');
            continue;
        }
        monitor_write_char((char)byte);
    }
}

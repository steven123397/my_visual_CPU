#include "monitor.h"

#include "console.h"

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

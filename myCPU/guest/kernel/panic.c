#include "panic.h"

#include "console.h"
#include "platform.h"

void panic_shutdown(void) {
    console_putc('X');
    platform_shutdown(1);
}

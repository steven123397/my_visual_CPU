#include <stdint.h>

#include "console.h"
#include "panic.h"
#include "platform.h"
#include "storage.h"
#include "timer.h"
#include "trap.h"

static uint8_t storage_buffer[STORAGE_BLOCK_SIZE];

void kernel_main(void) {
    platform_plic_supervisor_init();
    trap_reset_state();

    if (storage_read_block(0, storage_buffer) != 0) {
        panic_shutdown();
    }

    if (storage_buffer[0] != 'S' ||
        storage_buffer[1] != 't' ||
        storage_buffer[2] != 'o' ||
        storage_buffer[3] != 'r') {
        panic_shutdown();
    }

    timer_schedule_delta(8);
    platform_uart_enable_thre_irq();

    while (!trap_timer_irq_seen() || !trap_external_irq_seen()) {
    }

    console_puts("KRN");
    platform_shutdown(0);
}

#include "timer.h"

#include <stdint.h>

#include "platform.h"
#include "riscv.h"

void timer_schedule_delta(uint64_t delta) {
    const uint64_t now = platform_clint_read_mtime();
    platform_clint_write_mtimecmp(now + delta);
}

void timer_handle_interrupt(void) {
    platform_clint_write_mtimecmp(UINT64_MAX);
    riscv_clear_sip_bits(RISCV_SIP_STIP);
}

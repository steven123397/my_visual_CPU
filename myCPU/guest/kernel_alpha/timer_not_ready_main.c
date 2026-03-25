#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"

typedef struct KernelAlphaTimerNotReadyState {
    volatile uint32_t external_interrupts;
} kernel_alpha_timer_not_ready_state_t;

static void kernel_alpha_timer_not_ready_external_post_handler(uint64_t cause,
                                                               uint32_t source_id,
                                                               void* context) {
    kernel_alpha_timer_not_ready_state_t* state =
        (kernel_alpha_timer_not_ready_state_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || state == NULL ||
        source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    state->external_interrupts += 1U;
    console_putc('E');
}

static void kernel_alpha_timer_not_ready_timer_post_handler(uint64_t cause,
                                                            void* context) {
    kernel_alpha_timer_not_ready_state_t* state =
        (kernel_alpha_timer_not_ready_state_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || state == NULL) {
        panic_shutdown();
    }

    panic_shutdown();
}

static bool kernel_alpha_timer_not_ready_install_policies(
    trap_context_t* trap_context,
    void* context) {
    kernel_alpha_timer_not_ready_state_t* state =
        (kernel_alpha_timer_not_ready_state_t*)context;

    return state != NULL &&
           trap_context_install_supervisor_timer_policy(
               trap_context,
               kernel_alpha_timer_not_ready_timer_post_handler,
               state) &&
           trap_context_install_supervisor_external_policy(
               trap_context,
               kernel_alpha_timer_not_ready_external_post_handler,
               state);
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    vm_address_space_t* kernel_address_space = NULL;
    kernel_alpha_timer_not_ready_state_t state = {0};
    uint64_t deadline = 0;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC,
        .pmm_probe_marker = UINT64_C(0x54494D4E52445921),
        .pre_vm_setup = kernel_alpha_timer_not_ready_install_policies,
        .pre_vm_context = &state,
    };

    if (!kernel_alpha_run_common_bringup(&supervisor_trap_context,
                                         &kernel_address_space,
                                         &options)) {
        panic_shutdown();
    }

    platform_plic_supervisor_init();
    console_putc('P');

    deadline = platform_clint_read_mtime() + 4096U;
    platform_uart_enable_thre_irq();
    while (state.external_interrupts == 0U) {
        if (platform_clint_read_mtime() > deadline) {
            panic_shutdown();
        }
    }

    platform_clint_write_mtimecmp(UINT64_MAX);
    riscv_clear_sip_bits(RISCV_SIP_STIP);

    deadline = platform_clint_read_mtime() + 4096U;
    while (platform_clint_read_mtime() <= deadline) {
    }

    console_putc('T');
    panic_shutdown();
}

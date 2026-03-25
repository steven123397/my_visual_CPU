#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "storage.h"
#include "timer.h"

typedef struct KernelAlphaState {
    volatile uint32_t external_interrupts;
    volatile uint32_t timer_interrupts;
} kernel_alpha_state_t;

static void kernel_alpha_timer_post_handler(uint64_t cause, void* context);
static void kernel_alpha_external_post_handler(uint64_t cause,
                                               uint32_t source_id,
                                               void* context);

static bool kernel_alpha_probe_storage_page(void) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();

    if (storage_page == NULL) {
        return false;
    }

    if (storage_read_block(0, storage_page) != 0 ||
        storage_page[0] != 'S' ||
        storage_page[1] != 't' ||
        storage_page[2] != 'o' ||
        storage_page[3] != 'r') {
        return false;
    }

    return pmm_free_page(storage_page);
}

static bool kernel_alpha_probe_storage_device(void) {
    storage_info_t storage_info = {0};

    return storage_probe(&storage_info) && storage_info.capacity_blocks > 0;
}

static bool kernel_alpha_install_policies(trap_context_t* trap_context,
                                          void* context) {
    kernel_alpha_state_t* state = (kernel_alpha_state_t*)context;

    return state != NULL &&
           trap_context_install_supervisor_timer_policy(
               trap_context,
               kernel_alpha_timer_post_handler,
               state) &&
           trap_context_install_supervisor_external_policy(
               trap_context,
               kernel_alpha_external_post_handler,
               state);
}

static void kernel_alpha_timer_post_handler(uint64_t cause, void* context) {
    kernel_alpha_state_t* state = (kernel_alpha_state_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || state == NULL) {
        panic_shutdown();
    }

    state->timer_interrupts += 1U;
    console_putc('T');
}

static void kernel_alpha_external_post_handler(uint64_t cause,
                                               uint32_t source_id,
                                               void* context) {
    kernel_alpha_state_t* state = (kernel_alpha_state_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || state == NULL ||
        source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    state->external_interrupts += 1U;
    console_putc('E');
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    vm_address_space_t* kernel_address_space = NULL;
    kernel_alpha_state_t state = {0};
    uint64_t deadline = 0;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = UINT64_C(0x4B41504D4D56414C),
        .pre_vm_setup = kernel_alpha_install_policies,
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

    deadline = platform_clint_read_mtime() + 4096U;
    timer_schedule_delta(64U);
    while (state.timer_interrupts == 0U) {
        if (platform_clint_read_mtime() > deadline) {
            panic_shutdown();
        }
    }

    if (!kernel_alpha_probe_storage_device()) {
        panic_shutdown();
    }
    console_putc('D');

    if (!kernel_alpha_probe_storage_page()) {
        panic_shutdown();
    }
    console_putc('S');

    platform_shutdown(0);
}

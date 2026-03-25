#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "storage.h"
#include "supervisor_runtime.h"

static void kernel_alpha_timer_post_handler(void* context);
static void kernel_alpha_external_post_handler(uint32_t source_id,
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

static void kernel_alpha_timer_post_handler(void* context) {
    if (context == NULL) {
        panic_shutdown();
    }

    console_putc('T');
}

static void kernel_alpha_external_post_handler(uint32_t source_id,
                                               void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    console_putc('E');
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    vm_address_space_t* kernel_address_space = NULL;
    supervisor_runtime_interrupt_state_t interrupts;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = UINT64_C(0x4B41504D4D56414C),
        .pre_vm_setup =
            supervisor_runtime_install_interrupt_counter_policies_adapter,
        .pre_vm_context = &interrupts,
    };

    supervisor_runtime_interrupt_state_init(&interrupts);
    supervisor_runtime_interrupt_state_bind_self_handlers(
        &interrupts,
        PLIC_SOURCE_UART_THRE,
        kernel_alpha_timer_post_handler,
        kernel_alpha_external_post_handler);

    if (!kernel_alpha_run_common_bringup(&supervisor_trap_context,
                                         &kernel_address_space,
                                         &options)) {
        panic_shutdown();
    }

    platform_plic_supervisor_init();
    console_putc('P');

    if (!supervisor_runtime_enable_uart_thre_and_wait(
            &interrupts.external_interrupts,
            4096U)) {
        panic_shutdown();
    }

    if (!supervisor_runtime_schedule_timer_and_wait(
            &interrupts.timer_interrupts,
            64U,
            4096U)) {
        panic_shutdown();
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

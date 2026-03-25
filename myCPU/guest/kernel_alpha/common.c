#include "kernel_alpha.h"

#include <stdint.h>

#include "console.h"
#include "platform.h"
#include "pmm.h"
#include "storage.h"
#include "supervisor_runtime.h"

bool kernel_alpha_run_common_bringup(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_alpha_bringup_options_t* options) {
    return kernel_bringup_run_common(trap_context, out_space, options);
}

void kernel_alpha_begin_plic_supervisor_phase(void) {
    platform_plic_supervisor_init();
    console_putc('P');
}

bool kernel_alpha_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta) {
    return runtime != NULL &&
           supervisor_runtime_enable_uart_thre_and_wait(
               &runtime->interrupts.external_interrupts,
               timeout_delta);
}

bool kernel_alpha_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta) {
    return runtime != NULL &&
           supervisor_runtime_schedule_timer_and_wait(
               &runtime->interrupts.timer_interrupts,
               timer_delta,
               timeout_delta);
}

bool kernel_alpha_complete_storage_probe(void) {
    storage_info_t storage_info = {0};

    if (!storage_probe(&storage_info) || storage_info.capacity_blocks == 0) {
        return false;
    }

    console_putc('D');
    return true;
}

bool kernel_alpha_complete_storage_signature_check(void) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool valid_signature =
        storage_page != NULL && storage_read_block(0, storage_page) == 0 &&
        storage_page[0] == 'S' && storage_page[1] == 't' &&
        storage_page[2] == 'o' && storage_page[3] == 'r';

    if (storage_page == NULL || !valid_signature || !pmm_free_page(storage_page)) {
        return false;
    }

    console_putc('S');
    return true;
}

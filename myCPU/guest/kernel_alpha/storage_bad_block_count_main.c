#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "storage.h"

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    vm_address_space_t* kernel_address_space = NULL;
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};
    uint8_t* storage_page = NULL;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    if (!kernel_alpha_run_common_bringup(&supervisor_trap_context,
                                         &kernel_address_space,
                                         &options) ||
        !storage_probe(&storage_info) ||
        storage_info.capacity_blocks == 0) {
        panic_shutdown();
    }

    storage_page = (uint8_t*)pmm_alloc_page();
    if (storage_page == NULL ||
        storage_read_block_with_count(0, 2, storage_page) !=
            STORAGE_ERR_BAD_BLOCK_COUNT ||
        (storage_status() & STORAGE_STATUS_ERROR) == 0 ||
        storage_error() != STORAGE_ERR_BAD_BLOCK_COUNT) {
        panic_shutdown();
    }

    storage_clear_error();
    if ((storage_status() & STORAGE_STATUS_ERROR) != 0 ||
        storage_error() != STORAGE_ERR_NONE ||
        !storage_probe(&storage_info_after_clear) ||
        !pmm_free_page(storage_page)) {
        panic_shutdown();
    }

    console_putc('B');
    panic_shutdown();
}

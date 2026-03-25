#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "storage.h"

void kernel_main(void) {
    kernel_runtime_t runtime;
    storage_info_t storage_info = {0};
    uint8_t* storage_page = NULL;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime, &options)) {
        panic_shutdown();
    }

    storage_page = (uint8_t*)pmm_alloc_page();
    if (storage_page == NULL ||
        storage_read_info(&storage_info) ||
        storage_info.capacity_blocks == 0 ||
        (storage_info.status & STORAGE_STATUS_ATTACHED) == 0 ||
        (storage_info.status & STORAGE_STATUS_READY) == 0 ||
        (storage_info.status & STORAGE_STATUS_ERROR) != 0 ||
        storage_probe(NULL) ||
        storage_read_block(0, storage_page) != 0 ||
        storage_page[0] != 'S' ||
        storage_page[1] != 't' ||
        storage_page[2] != 'o' ||
        storage_page[3] != 'r' ||
        !pmm_free_page(storage_page)) {
        panic_shutdown();
    }

    console_putc('G');
    panic_shutdown();
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"
#include "storage.h"

void kernel_main(void) {
    kernel_runtime_t runtime;
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime, &options) ||
        !storage_probe(&storage_info) ||
        storage_info.capacity_blocks == 0) {
        panic_shutdown();
    }

    platform_storage_write_u64(STORAGE_REG_LBA, 0);
    platform_storage_write_u64(STORAGE_REG_BLOCK_COUNT, 1);
    platform_storage_issue_command(STORAGE_CMD_WRITE + 1);
    if ((storage_status() & STORAGE_STATUS_ERROR) == 0 ||
        storage_error() != STORAGE_ERR_BAD_COMMAND) {
        panic_shutdown();
    }

    storage_clear_error();
    if ((storage_status() & STORAGE_STATUS_ERROR) != 0 ||
        storage_error() != STORAGE_ERR_NONE ||
        !storage_probe(&storage_info_after_clear)) {
        panic_shutdown();
    }

    console_putc('C');
    panic_shutdown();
}

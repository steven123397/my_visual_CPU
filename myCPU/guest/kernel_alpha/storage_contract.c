#include "kernel_alpha.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"
#include "pmm.h"
#include "storage.h"

static bool kernel_alpha_storage_expect_info(const storage_info_t* info,
                                             bool expect_nonzero_capacity,
                                             bool expect_zero_capacity,
                                             uint64_t required_status_bits,
                                             uint64_t forbidden_status_bits) {
    if (info == NULL ||
        (expect_nonzero_capacity && info->capacity_blocks == 0) ||
        (expect_zero_capacity && info->capacity_blocks != 0) ||
        (info->status & required_status_bits) != required_status_bits ||
        (info->status & forbidden_status_bits) != 0) {
        return false;
    }

    return true;
}

static bool kernel_alpha_storage_release_page(void* page, bool ok) {
    return page != NULL && pmm_free_page(page) && ok;
}

static bool kernel_alpha_storage_probe_live(storage_info_t* info) {
    return storage_probe(info) &&
           info != NULL &&
           kernel_alpha_storage_expect_info(info, true, false, 0, 0);
}

static bool kernel_alpha_storage_clear_error_and_probe(storage_info_t* info) {
    storage_clear_error();
    return (storage_status() & STORAGE_STATUS_ERROR) == 0 &&
           storage_error() == STORAGE_ERR_NONE &&
           kernel_alpha_storage_probe_live(info);
}

bool kernel_alpha_run_storage_bringup(kernel_runtime_t* runtime) {
    return kernel_runtime_run_bringup(runtime,
                                      KERNEL_ALPHA_MMIO_UART |
                                          KERNEL_ALPHA_MMIO_STORAGE,
                                      0,
                                      NULL);
}

bool kernel_alpha_validate_storage_no_media_contract(void) {
    storage_info_t storage_info = {0};
    storage_info_t storage_error_info = {0};
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool ok =
        storage_page != NULL &&
        storage_read_info(&storage_info) &&
        kernel_alpha_storage_expect_info(&storage_info,
                                         false,
                                         true,
                                         0,
                                         STORAGE_STATUS_ATTACHED |
                                             STORAGE_STATUS_ERROR) &&
        !storage_probe(NULL) &&
        storage_read_block(0, storage_page) == STORAGE_ERR_NO_MEDIA &&
        storage_read_info(&storage_error_info) &&
        kernel_alpha_storage_expect_info(&storage_error_info,
                                         false,
                                         true,
                                         STORAGE_STATUS_ERROR,
                                         STORAGE_STATUS_ATTACHED);

    return kernel_alpha_storage_release_page(storage_page, ok);
}

bool kernel_alpha_validate_storage_not_ready_contract(void) {
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool ok =
        storage_page != NULL &&
        storage_read_info(&storage_info) &&
        kernel_alpha_storage_expect_info(&storage_info,
                                         true,
                                         false,
                                         STORAGE_STATUS_ATTACHED,
                                         STORAGE_STATUS_READY |
                                             STORAGE_STATUS_ERROR) &&
        !storage_probe(NULL) &&
        storage_read_block(0, storage_page) == STORAGE_ERR_NOT_READY &&
        (storage_status() & STORAGE_STATUS_ERROR) != 0 &&
        storage_error() == STORAGE_ERR_NOT_READY;
    const bool ok_after_clear =
        ok &&
        (storage_clear_error(), true) &&
        storage_read_info(&storage_info_after_clear) &&
        kernel_alpha_storage_expect_info(&storage_info_after_clear,
                                         true,
                                         false,
                                         STORAGE_STATUS_ATTACHED,
                                         STORAGE_STATUS_READY |
                                             STORAGE_STATUS_ERROR) &&
        storage_error() == STORAGE_ERR_NONE &&
        !storage_probe(NULL);

    return kernel_alpha_storage_release_page(storage_page, ok_after_clear);
}

bool kernel_alpha_validate_storage_bad_magic_contract(void) {
    storage_info_t storage_info = {0};
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool ok =
        storage_page != NULL &&
        !storage_read_info(&storage_info) &&
        kernel_alpha_storage_expect_info(&storage_info,
                                         true,
                                         false,
                                         STORAGE_STATUS_ATTACHED |
                                             STORAGE_STATUS_READY,
                                         STORAGE_STATUS_ERROR) &&
        !storage_probe(NULL) &&
        storage_read_block(0, storage_page) == STORAGE_ERR_NONE &&
        storage_page[0] == 'S' && storage_page[1] == 't' &&
        storage_page[2] == 'o' && storage_page[3] == 'r';

    return kernel_alpha_storage_release_page(storage_page, ok);
}

bool kernel_alpha_validate_storage_bad_block_count_contract(void) {
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool ok =
        storage_page != NULL &&
        kernel_alpha_storage_probe_live(&storage_info) &&
        storage_read_block_with_count(0, 2, storage_page) ==
            STORAGE_ERR_BAD_BLOCK_COUNT &&
        (storage_status() & STORAGE_STATUS_ERROR) != 0 &&
        storage_error() == STORAGE_ERR_BAD_BLOCK_COUNT &&
        kernel_alpha_storage_clear_error_and_probe(&storage_info_after_clear);

    return kernel_alpha_storage_release_page(storage_page, ok);
}

bool kernel_alpha_validate_storage_lba_range_contract(void) {
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const uint64_t invalid_lba =
        kernel_alpha_storage_probe_live(&storage_info) ? storage_info.capacity_blocks
                                                       : 0;
    const bool ok =
        storage_page != NULL &&
        storage_info.capacity_blocks != 0 &&
        storage_read_block(invalid_lba, storage_page) == STORAGE_ERR_LBA_RANGE &&
        (storage_status() & STORAGE_STATUS_ERROR) != 0 &&
        storage_error() == STORAGE_ERR_LBA_RANGE &&
        kernel_alpha_storage_clear_error_and_probe(&storage_info_after_clear);

    return kernel_alpha_storage_release_page(storage_page, ok);
}

bool kernel_alpha_validate_storage_bad_command_contract(void) {
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};

    if (!kernel_alpha_storage_probe_live(&storage_info)) {
        return false;
    }

    platform_storage_write_u64(STORAGE_REG_LBA, 0);
    platform_storage_write_u64(STORAGE_REG_BLOCK_COUNT, 1);
    platform_storage_issue_command(STORAGE_CMD_WRITE + 1);

    return (storage_status() & STORAGE_STATUS_ERROR) != 0 &&
           storage_error() == STORAGE_ERR_BAD_COMMAND &&
           kernel_alpha_storage_clear_error_and_probe(&storage_info_after_clear);
}

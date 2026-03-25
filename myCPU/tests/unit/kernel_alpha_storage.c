#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/kernel_alpha.h"
#include "../../guest/include/kernel_runtime.h"
#include "../../guest/include/platform.h"
#include "../../guest/include/storage.h"

bool kernel_alpha_run_storage_bringup(kernel_runtime_t* runtime);
bool kernel_alpha_validate_storage_no_media_contract(void);
bool kernel_alpha_validate_storage_not_ready_contract(void);
bool kernel_alpha_validate_storage_bad_magic_contract(void);
bool kernel_alpha_validate_storage_bad_block_count_contract(void);
bool kernel_alpha_validate_storage_lba_range_contract(void);
bool kernel_alpha_validate_storage_bad_command_contract(void);

static kernel_runtime_t* g_bringup_runtime = NULL;
static const kernel_bringup_options_t* g_bringup_options = NULL;
static bool g_bringup_result = true;

static storage_info_t g_read_info_infos[4];
static bool g_read_info_results[4];
static size_t g_read_info_call_count = 0;
static size_t g_read_info_sequence_length = 0;

static storage_info_t g_probe_infos[4];
static bool g_probe_results[4];
static size_t g_probe_call_count = 0;
static size_t g_probe_sequence_length = 0;

static uint64_t g_status_values[4];
static size_t g_status_call_count = 0;
static size_t g_status_sequence_length = 0;

static uint64_t g_error_values[4];
static size_t g_error_call_count = 0;
static size_t g_error_sequence_length = 0;

static uint64_t g_read_block_results[4];
static size_t g_read_block_call_count = 0;
static size_t g_read_block_sequence_length = 0;
static uint64_t g_last_read_block_lba = UINT64_MAX;

static uint64_t g_read_block_with_count_results[4];
static size_t g_read_block_with_count_call_count = 0;
static size_t g_read_block_with_count_sequence_length = 0;
static uint64_t g_last_read_block_with_count_lba = UINT64_MAX;
static uint64_t g_last_read_block_with_count_value = 0;

static uint8_t g_storage_page[4096];
static bool g_alloc_page_available = false;
static void* g_last_freed_page = NULL;
static bool g_free_page_result = true;
static int g_clear_error_calls = 0;

static uint64_t g_storage_write_offsets[4];
static uint64_t g_storage_write_values[4];
static size_t g_storage_write_count = 0;
static uint64_t g_issue_command_value = UINT64_MAX;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_run_storage_bringup(void);
static int test_no_media_contract(void);
static int test_not_ready_contract(void);
static int test_bad_magic_contract(void);
static int test_bad_block_count_contract(void);
static int test_lba_range_contract(void);
static int test_bad_command_contract(void);

static size_t sequence_index(size_t call_count, size_t sequence_length) {
    return call_count < sequence_length ? call_count : sequence_length - 1U;
}

bool kernel_runtime_run_common_bringup(kernel_runtime_t* runtime,
                                       const kernel_bringup_options_t* options) {
    g_bringup_runtime = runtime;
    g_bringup_options = options;
    return g_bringup_result;
}

bool storage_read_info(storage_info_t* info) {
    const size_t index = sequence_index(g_read_info_call_count++,
                                        g_read_info_sequence_length);

    if (g_read_info_sequence_length == 0) {
        return false;
    }

    if (info != NULL) {
        *info = g_read_info_infos[index];
    }
    return g_read_info_results[index];
}

bool storage_probe(storage_info_t* info) {
    const size_t index =
        sequence_index(g_probe_call_count++, g_probe_sequence_length);

    if (g_probe_sequence_length == 0) {
        return false;
    }

    if (info != NULL) {
        *info = g_probe_infos[index];
    }
    return g_probe_results[index];
}

uint64_t storage_status(void) {
    const size_t index =
        sequence_index(g_status_call_count++, g_status_sequence_length);

    return g_status_sequence_length == 0 ? 0 : g_status_values[index];
}

uint64_t storage_error(void) {
    const size_t index =
        sequence_index(g_error_call_count++, g_error_sequence_length);

    return g_error_sequence_length == 0 ? STORAGE_ERR_NONE : g_error_values[index];
}

void storage_clear_error(void) {
    g_clear_error_calls += 1;
}

uint64_t storage_read_block(uint64_t lba, void* destination) {
    const size_t index = sequence_index(g_read_block_call_count++,
                                        g_read_block_sequence_length);

    g_last_read_block_lba = lba;
    if (destination != NULL) {
        memcpy(destination, g_storage_page, sizeof(g_storage_page));
    }

    return g_read_block_sequence_length == 0 ? STORAGE_ERR_NONE
                                             : g_read_block_results[index];
}

uint64_t storage_read_block_with_count(uint64_t lba,
                                       uint64_t block_count,
                                       void* destination) {
    const size_t index = sequence_index(g_read_block_with_count_call_count++,
                                        g_read_block_with_count_sequence_length);

    g_last_read_block_with_count_lba = lba;
    g_last_read_block_with_count_value = block_count;
    if (destination != NULL) {
        memcpy(destination, g_storage_page, sizeof(g_storage_page));
    }

    return g_read_block_with_count_sequence_length == 0
               ? STORAGE_ERR_NONE
               : g_read_block_with_count_results[index];
}

void* pmm_alloc_page(void) {
    return g_alloc_page_available ? g_storage_page : NULL;
}

bool pmm_free_page(void* page) {
    g_last_freed_page = page;
    return g_free_page_result;
}

void platform_storage_write_u64(uint64_t offset, uint64_t value) {
    if (g_storage_write_count < (sizeof(g_storage_write_offsets) /
                                 sizeof(g_storage_write_offsets[0]))) {
        g_storage_write_offsets[g_storage_write_count] = offset;
        g_storage_write_values[g_storage_write_count] = value;
    }
    g_storage_write_count += 1U;
}

void platform_storage_issue_command(uint64_t command) {
    g_issue_command_value = command;
}

static void reset_stub_state(void) {
    memset(g_read_info_infos, 0, sizeof(g_read_info_infos));
    memset(g_probe_infos, 0, sizeof(g_probe_infos));
    memset(g_status_values, 0, sizeof(g_status_values));
    memset(g_error_values, 0, sizeof(g_error_values));
    memset(g_read_block_results, 0, sizeof(g_read_block_results));
    memset(g_read_block_with_count_results,
           0,
           sizeof(g_read_block_with_count_results));
    memset(g_storage_page, 0, sizeof(g_storage_page));
    memset(g_storage_write_offsets, 0, sizeof(g_storage_write_offsets));
    memset(g_storage_write_values, 0, sizeof(g_storage_write_values));
    g_bringup_runtime = NULL;
    g_bringup_options = NULL;
    g_bringup_result = true;
    g_read_info_call_count = 0;
    g_read_info_sequence_length = 0;
    g_probe_call_count = 0;
    g_probe_sequence_length = 0;
    g_status_call_count = 0;
    g_status_sequence_length = 0;
    g_error_call_count = 0;
    g_error_sequence_length = 0;
    g_read_block_call_count = 0;
    g_read_block_sequence_length = 0;
    g_last_read_block_lba = UINT64_MAX;
    g_read_block_with_count_call_count = 0;
    g_read_block_with_count_sequence_length = 0;
    g_last_read_block_with_count_lba = UINT64_MAX;
    g_last_read_block_with_count_value = 0;
    g_alloc_page_available = false;
    g_last_freed_page = NULL;
    g_free_page_result = true;
    g_clear_error_calls = 0;
    g_storage_write_count = 0;
    g_issue_command_value = UINT64_MAX;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_run_storage_bringup(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    if (!kernel_alpha_run_storage_bringup(&runtime)) {
        return fail("expected storage bring-up helper to propagate success");
    }

    if (g_bringup_runtime != &runtime || g_bringup_options == NULL ||
        g_bringup_options->mmio_mask !=
            (KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_STORAGE) ||
        g_bringup_options->pmm_probe_marker != 0 ||
        g_bringup_options->pre_vm_setup != NULL ||
        g_bringup_options->pre_vm_context != NULL) {
        return fail("expected storage bring-up helper to forward storage-only options");
    }

    reset_stub_state();
    g_bringup_result = false;
    if (kernel_alpha_run_storage_bringup(&runtime)) {
        return fail("expected storage bring-up helper to propagate failure");
    }

    return 0;
}

static int test_no_media_contract(void) {
    reset_stub_state();
    g_alloc_page_available = true;
    g_read_info_sequence_length = 2;
    g_read_info_results[0] = true;
    g_read_info_infos[0].capacity_blocks = 0;
    g_read_info_results[1] = true;
    g_read_info_infos[1].capacity_blocks = 0;
    g_read_info_infos[1].status = STORAGE_STATUS_ERROR;
    g_probe_sequence_length = 1;
    g_probe_results[0] = false;
    g_read_block_sequence_length = 1;
    g_read_block_results[0] = STORAGE_ERR_NO_MEDIA;

    if (!kernel_alpha_validate_storage_no_media_contract()) {
        return fail("expected no-media contract helper to succeed");
    }

    if (g_last_read_block_lba != 0U || g_last_freed_page != g_storage_page) {
        return fail("expected no-media contract helper to read LBA0 and free page");
    }

    return 0;
}

static int test_not_ready_contract(void) {
    reset_stub_state();
    g_alloc_page_available = true;
    g_read_info_sequence_length = 2;
    g_read_info_results[0] = true;
    g_read_info_infos[0].capacity_blocks = 4U;
    g_read_info_infos[0].status = STORAGE_STATUS_ATTACHED;
    g_read_info_results[1] = true;
    g_read_info_infos[1].capacity_blocks = 4U;
    g_read_info_infos[1].status = STORAGE_STATUS_ATTACHED;
    g_probe_sequence_length = 2;
    g_probe_results[0] = false;
    g_probe_results[1] = false;
    g_read_block_sequence_length = 1;
    g_read_block_results[0] = STORAGE_ERR_NOT_READY;
    g_status_sequence_length = 1;
    g_status_values[0] = STORAGE_STATUS_ERROR;
    g_error_sequence_length = 2;
    g_error_values[0] = STORAGE_ERR_NOT_READY;
    g_error_values[1] = STORAGE_ERR_NONE;

    if (!kernel_alpha_validate_storage_not_ready_contract()) {
        return fail("expected not-ready contract helper to succeed");
    }

    if (g_clear_error_calls != 1 || g_last_freed_page != g_storage_page) {
        return fail("expected not-ready contract helper to clear error and free page");
    }

    return 0;
}

static int test_bad_magic_contract(void) {
    reset_stub_state();
    g_alloc_page_available = true;
    g_read_info_sequence_length = 1;
    g_read_info_results[0] = false;
    g_read_info_infos[0].capacity_blocks = 2U;
    g_read_info_infos[0].status =
        STORAGE_STATUS_ATTACHED | STORAGE_STATUS_READY;
    g_probe_sequence_length = 1;
    g_probe_results[0] = false;
    g_read_block_sequence_length = 1;
    g_read_block_results[0] = STORAGE_ERR_NONE;
    memcpy(g_storage_page, "Stor", 4);

    if (!kernel_alpha_validate_storage_bad_magic_contract()) {
        return fail("expected bad-magic contract helper to succeed");
    }

    if (g_last_freed_page != g_storage_page) {
        return fail("expected bad-magic contract helper to free page");
    }

    return 0;
}

static int test_bad_block_count_contract(void) {
    reset_stub_state();
    g_alloc_page_available = true;
    g_probe_sequence_length = 2;
    g_probe_results[0] = true;
    g_probe_infos[0].capacity_blocks = 1U;
    g_probe_results[1] = true;
    g_probe_infos[1].capacity_blocks = 1U;
    g_read_block_with_count_sequence_length = 1;
    g_read_block_with_count_results[0] = STORAGE_ERR_BAD_BLOCK_COUNT;
    g_status_sequence_length = 2;
    g_status_values[0] = STORAGE_STATUS_ERROR;
    g_status_values[1] = 0;
    g_error_sequence_length = 2;
    g_error_values[0] = STORAGE_ERR_BAD_BLOCK_COUNT;
    g_error_values[1] = STORAGE_ERR_NONE;

    if (!kernel_alpha_validate_storage_bad_block_count_contract()) {
        return fail("expected bad-block-count contract helper to succeed");
    }

    if (g_last_read_block_with_count_lba != 0U ||
        g_last_read_block_with_count_value != 2U ||
        g_clear_error_calls != 1 || g_last_freed_page != g_storage_page) {
        return fail("expected bad-block-count helper to issue custom read and clear");
    }

    return 0;
}

static int test_lba_range_contract(void) {
    reset_stub_state();
    g_alloc_page_available = true;
    g_probe_sequence_length = 2;
    g_probe_results[0] = true;
    g_probe_infos[0].capacity_blocks = 3U;
    g_probe_results[1] = true;
    g_probe_infos[1].capacity_blocks = 3U;
    g_read_block_sequence_length = 1;
    g_read_block_results[0] = STORAGE_ERR_LBA_RANGE;
    g_status_sequence_length = 2;
    g_status_values[0] = STORAGE_STATUS_ERROR;
    g_status_values[1] = 0;
    g_error_sequence_length = 2;
    g_error_values[0] = STORAGE_ERR_LBA_RANGE;
    g_error_values[1] = STORAGE_ERR_NONE;

    if (!kernel_alpha_validate_storage_lba_range_contract()) {
        return fail("expected LBA-range contract helper to succeed");
    }

    if (g_last_read_block_lba != 3U || g_clear_error_calls != 1 ||
        g_last_freed_page != g_storage_page) {
        return fail("expected LBA-range helper to read past capacity and clear");
    }

    return 0;
}

static int test_bad_command_contract(void) {
    reset_stub_state();
    g_probe_sequence_length = 2;
    g_probe_results[0] = true;
    g_probe_infos[0].capacity_blocks = 1U;
    g_probe_results[1] = true;
    g_probe_infos[1].capacity_blocks = 1U;
    g_status_sequence_length = 2;
    g_status_values[0] = STORAGE_STATUS_ERROR;
    g_status_values[1] = 0;
    g_error_sequence_length = 2;
    g_error_values[0] = STORAGE_ERR_BAD_COMMAND;
    g_error_values[1] = STORAGE_ERR_NONE;

    if (!kernel_alpha_validate_storage_bad_command_contract()) {
        return fail("expected bad-command contract helper to succeed");
    }

    if (g_storage_write_count < 2 ||
        g_storage_write_offsets[0] != STORAGE_REG_LBA ||
        g_storage_write_values[0] != 0U ||
        g_storage_write_offsets[1] != STORAGE_REG_BLOCK_COUNT ||
        g_storage_write_values[1] != 1U ||
        g_issue_command_value != STORAGE_CMD_WRITE + 1U ||
        g_clear_error_calls != 1) {
        return fail("expected bad-command helper to issue invalid command contract");
    }

    return 0;
}

int main(void) {
    if (test_run_storage_bringup() != 0 ||
        test_no_media_contract() != 0 ||
        test_not_ready_contract() != 0 ||
        test_bad_magic_contract() != 0 ||
        test_bad_block_count_contract() != 0 ||
        test_lba_range_contract() != 0 ||
        test_bad_command_contract() != 0) {
        return 1;
    }

    return 0;
}

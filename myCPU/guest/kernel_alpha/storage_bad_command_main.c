#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "runtime_context.h"
#include "storage.h"
#include "trap.h"
#include "vm.h"

typedef struct KernelAlphaStorageBadCommandState {
    vm_address_space_t* kernel_address_space;
} kernel_alpha_storage_bad_command_state_t;

static bool map_kernel_identity_if_present(vm_address_space_t* address_space,
                                           uintptr_t start,
                                           uintptr_t end,
                                           uint64_t flags) {
    const size_t size = start < end ? (size_t)(end - start) : 0U;

    return size == 0 ||
           vm_address_space_map_kernel_range(address_space,
                                             start,
                                             start,
                                             size,
                                             flags);
}

static bool register_kernel_identity_fault_range_if_present(
    vm_address_space_t* address_space,
    uintptr_t start,
    uintptr_t end,
    uint64_t flags) {
    const size_t size = start < end ? (size_t)(end - start) : 0U;

    return size == 0 ||
           vm_address_space_register_fault_range(address_space,
                                                start,
                                                start,
                                                size,
                                                flags);
}

static bool kernel_alpha_storage_bad_command_setup_vm(
    kernel_alpha_storage_bad_command_state_t* state) {
    const uint64_t text_flags = VM_PAGE_READ | VM_PAGE_EXEC;
    const uint64_t rodata_flags = VM_PAGE_READ;
    const uint64_t data_flags = VM_PAGE_READ | VM_PAGE_WRITE;
    const uintptr_t early_heap_start = memory_heap_start();
    const uintptr_t managed_start = pmm_managed_start();
    const uintptr_t managed_end = pmm_managed_end();

    if (state == NULL ||
        !vm_address_space_create(&state->kernel_address_space) ||
        !map_kernel_identity_if_present(state->kernel_address_space,
                                        memory_text_start(),
                                        memory_text_end(),
                                        text_flags) ||
        !map_kernel_identity_if_present(state->kernel_address_space,
                                        memory_rodata_start(),
                                        memory_rodata_end(),
                                        rodata_flags) ||
        !map_kernel_identity_if_present(state->kernel_address_space,
                                        memory_data_start(),
                                        memory_bss_end(),
                                        data_flags) ||
        !map_kernel_identity_if_present(state->kernel_address_space,
                                        early_heap_start,
                                        managed_start,
                                        data_flags) ||
        !map_kernel_identity_if_present(
            state->kernel_address_space,
            managed_start,
            managed_end,
            data_flags) ||
        !register_kernel_identity_fault_range_if_present(
            state->kernel_address_space,
            UART_BASE,
            UART_BASE + MEMORY_PAGE_SIZE,
            data_flags) ||
        !register_kernel_identity_fault_range_if_present(
            state->kernel_address_space,
            STORAGE_BASE,
            STORAGE_BASE + MEMORY_PAGE_SIZE,
            data_flags)) {
        return false;
    }

    return vm_address_space_enable(state->kernel_address_space) &&
           vm_address_space_is_enabled(state->kernel_address_space) &&
           vm_address_space_is_active(state->kernel_address_space);
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    kernel_alpha_storage_bad_command_state_t state = {0};
    storage_info_t storage_info = {0};
    storage_info_t storage_info_after_clear = {0};

    memory_init();
    runtime_context_reset();
    trap_context_init(&supervisor_trap_context);
    if (!trap_context_activate(&supervisor_trap_context) ||
        !trap_context_is_active(&supervisor_trap_context) ||
        trap_active_context() != &supervisor_trap_context) {
        panic_shutdown();
    }

    console_putc('K');

    pmm_init();
    if (pmm_total_pages() == 0 || pmm_free_pages() == 0) {
        panic_shutdown();
    }
    console_putc('M');

    if (!kernel_alpha_storage_bad_command_setup_vm(&state) ||
        !storage_probe(&storage_info) ||
        storage_info.capacity_blocks == 0) {
        panic_shutdown();
    }
    console_putc('V');

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

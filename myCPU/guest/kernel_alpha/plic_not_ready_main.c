#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "trap.h"
#include "vm.h"

typedef struct KernelAlphaPlicNotReadyState {
    volatile uint32_t external_interrupts;
    vm_address_space_t* kernel_address_space;
} kernel_alpha_plic_not_ready_state_t;

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

static bool kernel_alpha_plic_not_ready_setup_vm(
    kernel_alpha_plic_not_ready_state_t* state) {
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
            CLINT_BASE,
            CLINT_BASE + CLINT_SIZE,
            data_flags) ||
        !register_kernel_identity_fault_range_if_present(
            state->kernel_address_space,
            PLIC_BASE,
            PLIC_BASE + PLIC_SIZE,
            data_flags)) {
        return false;
    }

    return vm_address_space_enable(state->kernel_address_space) &&
           vm_address_space_is_enabled(state->kernel_address_space) &&
           vm_address_space_is_active(state->kernel_address_space) &&
           riscv_read_satp() == vm_address_space_satp_value(
                                    state->kernel_address_space);
}

static bool kernel_alpha_probe_pmm_page(void) {
    static const uint64_t marker = UINT64_C(0x504C49434E524459);
    uint64_t* page = (uint64_t*)pmm_alloc_page();

    if (page == NULL) {
        return false;
    }

    page[0] = marker;
    if (page[0] != marker) {
        return false;
    }

    return pmm_free_page(page);
}

static void kernel_alpha_plic_not_ready_external_post_handler(uint64_t cause,
                                                              uint32_t source_id,
                                                              void* context) {
    kernel_alpha_plic_not_ready_state_t* state =
        (kernel_alpha_plic_not_ready_state_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || state == NULL ||
        source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    state->external_interrupts += 1U;
    panic_shutdown();
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    kernel_alpha_plic_not_ready_state_t state = {0};
    uint64_t deadline = 0;

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

    if (!trap_context_install_supervisor_external_policy(
            &supervisor_trap_context,
            kernel_alpha_plic_not_ready_external_post_handler,
            &state) ||
        !kernel_alpha_plic_not_ready_setup_vm(&state) ||
        !kernel_alpha_probe_pmm_page()) {
        panic_shutdown();
    }

    console_putc('V');

    deadline = platform_clint_read_mtime() + 4096U;
    platform_uart_enable_thre_irq();
    while (state.external_interrupts == 0U) {
        if (platform_clint_read_mtime() > deadline) {
            platform_uart_disable_irq();
            console_putc('P');
            panic_shutdown();
        }
    }

    panic_shutdown();
}

#include <stdbool.h>
#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "timer.h"
#include "trap.h"
#include "vm.h"

typedef struct KernelAlphaState {
    volatile uint32_t timer_interrupts;
    vm_address_space_t* kernel_address_space;
} kernel_alpha_state_t;

static bool kernel_alpha_setup_vm(kernel_alpha_state_t* state) {
    const uint64_t ram_flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC;
    const uint64_t mmio_flags = VM_PAGE_READ | VM_PAGE_WRITE;

    if (state == NULL ||
        !vm_address_space_create(&state->kernel_address_space) ||
        !vm_address_space_map_identity_1g(state->kernel_address_space,
                                          MEM_BASE,
                                          ram_flags) ||
        !vm_address_space_register_fault_range(state->kernel_address_space,
                                               UART_BASE,
                                               UART_BASE,
                                               MEMORY_PAGE_SIZE,
                                               mmio_flags) ||
        !vm_address_space_register_fault_range(state->kernel_address_space,
                                               CLINT_BASE,
                                               CLINT_BASE,
                                               CLINT_SIZE,
                                               mmio_flags)) {
        return false;
    }

    return vm_address_space_enable(state->kernel_address_space) &&
           vm_address_space_is_enabled(state->kernel_address_space) &&
           vm_address_space_is_active(state->kernel_address_space) &&
           riscv_read_satp() == vm_address_space_satp_value(
                                    state->kernel_address_space);
}

static bool kernel_alpha_probe_pmm_page(void) {
    static const uint64_t marker = UINT64_C(0x4B41504D4D56414C);
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

static void kernel_alpha_timer_post_handler(uint64_t cause, void* context) {
    kernel_alpha_state_t* state = (kernel_alpha_state_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || state == NULL) {
        panic_shutdown();
    }

    state->timer_interrupts += 1U;
    console_putc('T');
}

void kernel_main(void) {
    trap_context_t supervisor_trap_context;
    kernel_alpha_state_t state = {0};
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

    if (!trap_context_install_supervisor_timer_policy(
            &supervisor_trap_context,
            kernel_alpha_timer_post_handler,
            &state) ||
        !kernel_alpha_setup_vm(&state) ||
        !kernel_alpha_probe_pmm_page()) {
        panic_shutdown();
    }

    console_putc('V');

    deadline = platform_clint_read_mtime() + 4096U;
    timer_schedule_delta(64U);
    while (state.timer_interrupts == 0U) {
        if (platform_clint_read_mtime() > deadline) {
            panic_shutdown();
        }
    }

    platform_shutdown(0);
}

#include "kernel_alpha.h"

#include <stdbool.h>
#include <stdint.h>

#include "panic.h"
#include "platform.h"
#include "console.h"
#include "supervisor_runtime.h"

bool kernel_alpha_run_interrupt_bringup(kernel_runtime_t* runtime,
                                        uint64_t pmm_probe_marker,
                                        kernel_alpha_pre_vm_setup_t pre_vm_setup) {
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC,
        .pmm_probe_marker = pmm_probe_marker,
        .pre_vm_setup = pre_vm_setup,
        .pre_vm_context = runtime,
    };

    return runtime != NULL && kernel_runtime_run_common_bringup(runtime, &options);
}

bool kernel_alpha_run_fault_bringup(kernel_runtime_t* runtime) {
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    return runtime != NULL && kernel_runtime_run_common_bringup(runtime, &options);
}

bool kernel_alpha_complete_platform_interrupt_readiness(kernel_runtime_t* runtime,
                                                        uint64_t timer_delta,
                                                        uint64_t timeout_delta) {
    if (runtime == NULL) {
        return false;
    }

    kernel_alpha_begin_plic_supervisor_phase();
    return kernel_alpha_wait_for_first_external_delivery(runtime, timeout_delta) &&
           kernel_alpha_wait_for_first_timer_delivery(runtime,
                                                      timer_delta,
                                                      timeout_delta);
}

bool kernel_alpha_validate_plic_not_ready_contract(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta,
                                                   char marker) {
    if (runtime == NULL ||
        kernel_alpha_wait_for_first_external_delivery(runtime, timeout_delta) ||
        runtime->interrupts.external_interrupts != 0U) {
        return false;
    }

    console_putc(marker);
    return true;
}

void kernel_alpha_timer_post_handler_emit_ready(void* context) {
    if (context == NULL) {
        panic_shutdown();
    }

    console_putc('T');
}

void kernel_alpha_external_post_handler_emit_ready(uint32_t source_id,
                                                   void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    console_putc('E');
}

void kernel_alpha_timer_post_handler_panic(void* context) {
    if (context == NULL) {
        panic_shutdown();
    }

    panic_shutdown();
}

void kernel_alpha_external_post_handler_panic_on_delivery(uint32_t source_id,
                                                          void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    panic_shutdown();
}

bool kernel_alpha_validate_timer_not_ready_contract(uint64_t timeout_delta,
                                                    char marker) {
    supervisor_runtime_cancel_timer_delivery();
    if (!supervisor_runtime_wait_timeout(timeout_delta)) {
        return false;
    }

    console_putc(marker);
    return true;
}

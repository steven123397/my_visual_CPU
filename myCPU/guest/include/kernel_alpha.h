#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_bringup.h"
#include "kernel_runtime.h"

enum {
    KERNEL_ALPHA_MMIO_UART = KERNEL_BRINGUP_MMIO_UART,
    KERNEL_ALPHA_MMIO_CLINT = KERNEL_BRINGUP_MMIO_CLINT,
    KERNEL_ALPHA_MMIO_PLIC = KERNEL_BRINGUP_MMIO_PLIC,
    KERNEL_ALPHA_MMIO_STORAGE = KERNEL_BRINGUP_MMIO_STORAGE,
};

typedef kernel_bringup_pre_vm_setup_t kernel_alpha_pre_vm_setup_t;
typedef kernel_bringup_options_t kernel_alpha_bringup_options_t;

bool kernel_alpha_run_common_bringup(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_alpha_bringup_options_t* options);
void kernel_alpha_begin_plic_supervisor_phase(void);
bool kernel_alpha_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta);
bool kernel_alpha_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta);
bool kernel_alpha_complete_storage_probe(void);
bool kernel_alpha_complete_storage_signature_check(void);

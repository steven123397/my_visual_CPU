#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trap.h"
#include "vm.h"

enum {
    KERNEL_BRINGUP_MMIO_UART = 1U << 0,
    KERNEL_BRINGUP_MMIO_CLINT = 1U << 1,
    KERNEL_BRINGUP_MMIO_PLIC = 1U << 2,
    KERNEL_BRINGUP_MMIO_STORAGE = 1U << 3,
};

typedef bool (*kernel_bringup_pre_vm_setup_t)(trap_context_t* trap_context,
                                              void* context);

typedef struct KernelBringupOptions {
    uint32_t mmio_mask;
    uint64_t pmm_probe_marker;
    kernel_bringup_pre_vm_setup_t pre_vm_setup;
    void* pre_vm_context;
    bool map_managed_memory;
} kernel_bringup_options_t;

bool kernel_bringup_run_common(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_bringup_options_t* options);

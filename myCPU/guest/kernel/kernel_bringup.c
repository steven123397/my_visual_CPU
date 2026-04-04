#include "kernel_bringup.h"

#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"

typedef struct KernelBringupMmioRange {
    uint32_t flag;
    uintptr_t start;
    uintptr_t end;
} kernel_bringup_mmio_range_t;

static const kernel_bringup_mmio_range_t k_kernel_bringup_mmio_ranges[] = {
    {
        .flag = KERNEL_BRINGUP_MMIO_UART,
        .start = UART_BASE,
        .end = UART_BASE + MEMORY_PAGE_SIZE,
    },
    {
        .flag = KERNEL_BRINGUP_MMIO_CLINT,
        .start = CLINT_BASE,
        .end = CLINT_BASE + CLINT_SIZE,
    },
    {
        .flag = KERNEL_BRINGUP_MMIO_PLIC,
        .start = PLIC_BASE,
        .end = PLIC_BASE + PLIC_SIZE,
    },
    {
        .flag = KERNEL_BRINGUP_MMIO_STORAGE,
        .start = STORAGE_BASE,
        .end = STORAGE_BASE + MEMORY_PAGE_SIZE,
    },
};

static bool kernel_bringup_map_identity_if_present(vm_address_space_t* address_space,
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

static bool kernel_bringup_register_fault_range_if_present(
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

static bool kernel_bringup_register_optional_mmio_range(
    vm_address_space_t* address_space,
    uint32_t mmio_mask,
    uint32_t mmio_flag,
    uintptr_t start,
    uintptr_t end,
    uint64_t flags) {
    return (mmio_mask & mmio_flag) == 0U ||
           kernel_bringup_register_fault_range_if_present(address_space,
                                                          start,
                                                          end,
                                                          flags);
}

static bool kernel_bringup_activate_trap_context(trap_context_t* trap_context) {
    return trap_context != NULL &&
           trap_context_activate(trap_context) &&
           trap_context_is_active(trap_context) &&
           trap_active_context() == trap_context;
}

static bool kernel_bringup_map_fixed_kernel_ranges(
    vm_address_space_t* address_space,
    bool map_managed_memory) {
    const uint64_t text_flags = VM_PAGE_READ | VM_PAGE_EXEC;
    const uint64_t rodata_flags = VM_PAGE_READ;
    const uint64_t data_flags = VM_PAGE_READ | VM_PAGE_WRITE;
    const uintptr_t early_heap_start = memory_heap_start();
    const uintptr_t managed_start = pmm_managed_start();
    const uintptr_t managed_end = pmm_managed_end();

    return kernel_bringup_map_identity_if_present(address_space,
                                                  memory_text_start(),
                                                  memory_text_end(),
                                                  text_flags) &&
           kernel_bringup_map_identity_if_present(address_space,
                                                  memory_rodata_start(),
                                                  memory_rodata_end(),
                                                  rodata_flags) &&
           kernel_bringup_map_identity_if_present(address_space,
                                                  memory_data_start(),
                                                  memory_bss_end(),
                                                  data_flags) &&
           kernel_bringup_map_identity_if_present(address_space,
                                                  early_heap_start,
                                                  managed_start,
                                                  data_flags) &&
           (!map_managed_memory ||
            kernel_bringup_map_identity_if_present(address_space,
                                                   managed_start,
                                                   managed_end,
                                                   data_flags));
}

static bool kernel_bringup_register_selected_mmio_fault_ranges(
    vm_address_space_t* address_space,
    uint32_t mmio_mask) {
    const uint64_t data_flags = VM_PAGE_READ | VM_PAGE_WRITE;

    for (size_t i = 0; i < (sizeof(k_kernel_bringup_mmio_ranges) /
                            sizeof(k_kernel_bringup_mmio_ranges[0]));
         ++i) {
        const kernel_bringup_mmio_range_t* range =
            &k_kernel_bringup_mmio_ranges[i];

        if (!kernel_bringup_register_optional_mmio_range(address_space,
                                                         mmio_mask,
                                                         range->flag,
                                                         range->start,
                                                         range->end,
                                                         data_flags)) {
            return false;
        }
    }

    return true;
}

static bool kernel_bringup_validate_active_address_space(
    const vm_address_space_t* address_space) {
    return address_space != NULL &&
           vm_address_space_is_enabled(address_space) &&
           vm_address_space_is_active(address_space) &&
           riscv_read_satp() == vm_address_space_satp_value(address_space);
}

static bool kernel_bringup_probe_pmm_page(uint64_t marker) {
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

static bool kernel_bringup_setup_vm(vm_address_space_t** out_space,
                                    const kernel_bringup_options_t* options) {
    vm_address_space_t* address_space = NULL;

    if (out_space == NULL || options == NULL ||
        !vm_address_space_create(&address_space)) {
        return false;
    }

    if (!kernel_bringup_map_fixed_kernel_ranges(address_space,
                                                options->map_managed_memory) ||
        !kernel_bringup_register_selected_mmio_fault_ranges(
            address_space,
            options->mmio_mask) ||
        !vm_address_space_enable(address_space) ||
        !kernel_bringup_validate_active_address_space(address_space)) {
        if (!vm_address_space_destroy(address_space)) {
            *out_space = address_space;
            return false;
        }
        *out_space = NULL;
        return false;
    }

    *out_space = address_space;
    return true;
}

bool kernel_bringup_run_common(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_bringup_options_t* options) {
    if (trap_context == NULL || out_space == NULL || options == NULL) {
        return false;
    }

    *out_space = NULL;

    memory_init();
    runtime_context_reset();
    trap_context_init(trap_context);
    if (!kernel_bringup_activate_trap_context(trap_context)) {
        return false;
    }
    console_putc('K');

    pmm_init();
    if (pmm_total_pages() == 0 || pmm_free_pages() == 0) {
        return false;
    }
    console_putc('M');

    if (options->pre_vm_setup != NULL &&
        !options->pre_vm_setup(trap_context, options->pre_vm_context)) {
        return false;
    }

    if (!kernel_bringup_setup_vm(out_space, options)) {
        return false;
    }

    if (options->pmm_probe_marker != 0 &&
        !kernel_bringup_probe_pmm_page(options->pmm_probe_marker)) {
        if (!vm_address_space_destroy(*out_space)) {
            return false;
        }
        *out_space = NULL;
        return false;
    }

    console_putc('V');
    return true;
}

#include "kernel_runtime.h"

#include <stddef.h>

#include "console.h"
#include "platform.h"
#include "pmm.h"
#include "storage.h"
#include "supervisor_runtime.h"

void kernel_runtime_init(kernel_runtime_t* runtime) {
    if (runtime == NULL) {
        return;
    }

    runtime->address_space = NULL;
    supervisor_runtime_interrupt_state_init(&runtime->interrupts);
}

trap_context_t* kernel_runtime_trap_context(kernel_runtime_t* runtime) {
    return runtime != NULL ? &runtime->trap_context : NULL;
}

vm_address_space_t* kernel_runtime_address_space(const kernel_runtime_t* runtime) {
    return runtime != NULL ? runtime->address_space : NULL;
}

supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state(
    kernel_runtime_t* runtime) {
    return runtime != NULL ? &runtime->interrupts : NULL;
}

void kernel_runtime_begin_plic_supervisor_phase(char marker) {
    platform_plic_supervisor_init();
    if (marker != '\0') {
        console_putc(marker);
    }
}

bool kernel_runtime_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                     uint64_t timeout_delta) {
    return runtime != NULL &&
           supervisor_runtime_enable_uart_thre_and_wait(
               &runtime->interrupts.external_interrupts,
               timeout_delta);
}

bool kernel_runtime_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                  uint64_t timer_delta,
                                                  uint64_t timeout_delta) {
    return runtime != NULL &&
           supervisor_runtime_schedule_timer_and_wait(
               &runtime->interrupts.timer_interrupts,
               timer_delta,
               timeout_delta);
}

bool kernel_runtime_complete_storage_probe(char marker) {
    storage_info_t storage_info = {0};

    if (!storage_probe(&storage_info) || storage_info.capacity_blocks == 0) {
        return false;
    }

    if (marker != '\0') {
        console_putc(marker);
    }
    return true;
}

bool kernel_runtime_complete_storage_signature_check(char marker) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool valid_signature =
        storage_page != NULL && storage_read_block(0, storage_page) == 0 &&
        storage_page[0] == 'S' && storage_page[1] == 't' &&
        storage_page[2] == 'o' && storage_page[3] == 'r';

    if (storage_page == NULL || !valid_signature || !pmm_free_page(storage_page)) {
        return false;
    }

    if (marker != '\0') {
        console_putc(marker);
    }
    return true;
}

bool kernel_runtime_bind_self_interrupt_handlers(
    kernel_runtime_t* runtime,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler) {
    supervisor_runtime_interrupt_state_t* interrupts =
        kernel_runtime_interrupt_state(runtime);

    if (interrupts == NULL) {
        return false;
    }

    supervisor_runtime_interrupt_state_bind_self_handlers(
        interrupts,
        expected_external_source_id,
        timer_post_handler,
        external_post_handler);
    return true;
}

bool kernel_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context) {
    kernel_runtime_t* runtime = (kernel_runtime_t*)context;

    return trap_context != NULL && runtime != NULL &&
           supervisor_runtime_install_external_counter_policy(
               trap_context,
               kernel_runtime_interrupt_state(runtime));
}

bool kernel_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context) {
    kernel_runtime_t* runtime = (kernel_runtime_t*)context;

    return trap_context != NULL && runtime != NULL &&
           supervisor_runtime_install_interrupt_counter_policies(
               trap_context,
               kernel_runtime_interrupt_state(runtime));
}

bool kernel_runtime_run_common_bringup(
    kernel_runtime_t* runtime,
    const kernel_bringup_options_t* options) {
    kernel_bringup_options_t bound_options;

    if (runtime == NULL) {
        return false;
    }

    if (options == NULL) {
        return false;
    }

    bound_options = *options;
    if (bound_options.pre_vm_setup != NULL &&
        bound_options.pre_vm_context == NULL) {
        bound_options.pre_vm_context = runtime;
    }

    return kernel_bringup_run_common(kernel_runtime_trap_context(runtime),
                                     &runtime->address_space,
                                     &bound_options);
}

bool kernel_runtime_run_bringup(
    kernel_runtime_t* runtime,
    uint32_t mmio_mask,
    uint64_t pmm_probe_marker,
    kernel_bringup_pre_vm_setup_t pre_vm_setup) {
    const kernel_bringup_options_t options = {
        .mmio_mask = mmio_mask,
        .pmm_probe_marker = pmm_probe_marker,
        .pre_vm_setup = pre_vm_setup,
        .pre_vm_context = NULL,
        .map_managed_memory = true,
    };

    return kernel_runtime_run_common_bringup(runtime, &options);
}

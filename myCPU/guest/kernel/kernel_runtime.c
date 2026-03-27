#include "kernel_runtime.h"

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
    if (runtime == NULL) {
        return false;
    }

    return kernel_bringup_run_common(kernel_runtime_trap_context(runtime),
                                     &runtime->address_space,
                                     options);
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
        .pre_vm_context = pre_vm_setup != NULL ? runtime : NULL,
        .map_managed_memory = true,
    };

    return kernel_runtime_run_common_bringup(runtime, &options);
}

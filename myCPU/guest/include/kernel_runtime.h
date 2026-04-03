#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_bringup.h"
#include "supervisor_runtime.h"
#include "trap.h"
#include "vm.h"

typedef struct KernelRuntime {
    trap_context_t trap_context;
    vm_address_space_t* address_space;
    supervisor_runtime_interrupt_state_t interrupts;
} kernel_runtime_t;

void kernel_runtime_init(kernel_runtime_t* runtime);
trap_context_t* kernel_runtime_trap_context(kernel_runtime_t* runtime);
vm_address_space_t* kernel_runtime_address_space(const kernel_runtime_t* runtime);
supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state(
    kernel_runtime_t* runtime);
bool kernel_runtime_run_entry_bringup(kernel_runtime_t* runtime);
bool kernel_runtime_run_identity_superpage_bringup(kernel_runtime_t* runtime);
void kernel_runtime_begin_plic_supervisor_phase(char marker);
bool kernel_runtime_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                     uint64_t timeout_delta);
bool kernel_runtime_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                  uint64_t timer_delta,
                                                  uint64_t timeout_delta);
bool kernel_runtime_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta);
bool kernel_runtime_complete_storage_probe(char marker);
bool kernel_runtime_complete_storage_signature_check(char marker);
bool kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta);
bool kernel_runtime_bind_self_interrupt_handlers(
    kernel_runtime_t* runtime,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler);
bool kernel_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context);
bool kernel_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context);
bool kernel_runtime_run_common_bringup(
    kernel_runtime_t* runtime,
    const kernel_bringup_options_t* options);
bool kernel_runtime_run_bringup(
    kernel_runtime_t* runtime,
    uint32_t mmio_mask,
    uint64_t pmm_probe_marker,
    kernel_bringup_pre_vm_setup_t pre_vm_setup);

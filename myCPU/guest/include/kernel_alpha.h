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
bool kernel_alpha_run_interrupt_bringup(kernel_runtime_t* runtime,
                                        uint64_t pmm_probe_marker,
                                        kernel_alpha_pre_vm_setup_t pre_vm_setup);
bool kernel_alpha_run_fault_bringup(kernel_runtime_t* runtime);
bool kernel_alpha_complete_platform_interrupt_readiness(kernel_runtime_t* runtime,
                                                        uint64_t timer_delta,
                                                        uint64_t timeout_delta);
bool kernel_alpha_validate_plic_not_ready_contract(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta,
                                                   char marker);
bool kernel_alpha_validate_timer_not_ready_contract(uint64_t timeout_delta,
                                                    char marker);
void kernel_alpha_timer_post_handler_emit_ready(void* context);
void kernel_alpha_external_post_handler_emit_ready(uint32_t source_id,
                                                   void* context);
void kernel_alpha_timer_post_handler_panic(void* context);
void kernel_alpha_external_post_handler_panic_on_delivery(uint32_t source_id,
                                                          void* context);
bool kernel_alpha_complete_storage_probe(void);
bool kernel_alpha_complete_storage_signature_check(void);
bool kernel_alpha_run_storage_bringup(kernel_runtime_t* runtime);
bool kernel_alpha_validate_storage_no_media_contract(void);
bool kernel_alpha_validate_storage_not_ready_contract(void);
bool kernel_alpha_validate_storage_bad_magic_contract(void);
bool kernel_alpha_validate_storage_bad_block_count_contract(void);
bool kernel_alpha_validate_storage_lba_range_contract(void);
bool kernel_alpha_validate_storage_bad_command_contract(void);

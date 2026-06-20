#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kernel_bringup.h"
#include "supervisor_runtime.h"
#include "trap.h"
#include "vm.h"

/* kernel runtime 对象：把 trap_context、地址空间与 supervisor interrupt state
   组合成 bring-up 复用单元，并沉淀 PLIC/storage/timer 等 phase helper。 */
typedef struct KernelRuntime {
    trap_context_t trap_context;
    vm_address_space_t* address_space;
    supervisor_runtime_interrupt_state_t interrupts;
} kernel_runtime_t;

typedef bool (*kernel_runtime_storage_lba0_predicate_t)(const uint8_t* block,
                                                        size_t block_size,
                                                        void* context);

/* 初始化 runtime：清 trap_context、地址空间与中断状态。 */
void kernel_runtime_init(kernel_runtime_t* runtime);
/* 取 trap_context 指针。 */
trap_context_t* kernel_runtime_trap_context(kernel_runtime_t* runtime);
/* 设置当前地址空间引用。 */
void kernel_runtime_set_address_space(kernel_runtime_t* runtime,
                                      vm_address_space_t* address_space);
/* 取当前地址空间引用。 */
vm_address_space_t* kernel_runtime_address_space(const kernel_runtime_t* runtime);
/* 取可写中断状态。 */
supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state(
    kernel_runtime_t* runtime);
/* 取只读中断状态。 */
const supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state_const(
    const kernel_runtime_t* runtime);
/* 跑 entry bring-up（K/M/V 骨架，入口级 trap bring-up）。 */
bool kernel_runtime_run_entry_bringup(kernel_runtime_t* runtime);
/* 跑 identity-superpage bring-up（1G identity 映射 + 入口级 trap）。 */
bool kernel_runtime_run_identity_superpage_bringup(kernel_runtime_t* runtime);
/* 打印 PLIC supervisor phase marker。 */
void kernel_runtime_begin_plic_supervisor_phase(char marker);
/* 等待首次 external delivery，超时失败。 */
bool kernel_runtime_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                     uint64_t timeout_delta);
/* 等待下一次 external delivery，超时失败。 */
bool kernel_runtime_wait_for_next_external_delivery(kernel_runtime_t* runtime,
                                                    uint64_t timeout_delta);
/* 等待首次 timer delivery，超时失败。 */
bool kernel_runtime_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                  uint64_t timer_delta,
                                                  uint64_t timeout_delta);
/* 等待 timer + external 平台中断都到达。 */
bool kernel_runtime_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta);
/* 完成 storage probe phase 并打印 marker。 */
bool kernel_runtime_complete_storage_probe(char marker);
/* 完成 storage LBA0 读 + 自定义校验 phase。 */
bool kernel_runtime_complete_storage_lba0_check(
    char marker,
    kernel_runtime_storage_lba0_predicate_t predicate,
    void* context);
/* 完成 demo storage signature guardrail phase。 */
bool kernel_runtime_complete_demo_storage_signature_guardrail(char marker);
/* 完成 storage signature 后再等待平台中断（合并 phase）。 */
bool kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta);
/* 绑定 self 中断 handler（timer + external post-handler）。 */
bool kernel_runtime_bind_self_interrupt_handlers(
    kernel_runtime_t* runtime,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler);
/* adapter：把 context 当 runtime，装 external counter policy。 */
bool kernel_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context);
/* adapter：把 context 当 runtime，装 timer + external counter policy。 */
bool kernel_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context);
/* 跑通用 common bringup（按 options 装配）。 */
bool kernel_runtime_run_common_bringup(
    kernel_runtime_t* runtime,
    const kernel_bringup_options_t* options);
/* 跑完整 bringup：MMIO 掩码 + PMM probe marker + pre-VM setup 回调。 */
bool kernel_runtime_run_bringup(
    kernel_runtime_t* runtime,
    uint32_t mmio_mask,
    uint64_t pmm_probe_marker,
    kernel_bringup_pre_vm_setup_t pre_vm_setup);

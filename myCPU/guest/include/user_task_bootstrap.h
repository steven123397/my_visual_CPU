#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user_task.h"

/* user_task 的 bootstrap 规划器：把符号地址转成 region/object/入口布局，
   并在 configure/bind/prepare 各阶段驱动 user_task 装配。 */
typedef struct UserTaskBootstrap {
    bool planned;
    bool configured;
    bool bound;
    user_task_t* user_task;
    uintptr_t exec_page_paddr;
    uintptr_t exec_vaddr;
    uintptr_t stack_vaddr;
    uintptr_t alias_vaddr;
    uintptr_t anon_vaddr;
    uintptr_t anon_tail_vaddr;
    uintptr_t entry_pc;
    uintptr_t expected_ecall_pc;
    uintptr_t user_sp;
    vm_user_region_t exec_region;
    vm_user_region_t stack_region;
    vm_user_region_t alias_region;
    vm_user_region_t anon_region;
    vm_user_region_t anon_tail_region;
    vm_object_t exec_object;
    vm_object_t stack_object;
    vm_object_t alias_object;
    vm_object_t anon_object;
} user_task_bootstrap_t;

/* 初始化 bootstrap 为干净未规划状态。 */
void user_task_bootstrap_init(user_task_bootstrap_t* bootstrap);
/* 复位 bootstrap：释放已绑 region/object 并回到未规划状态。 */
bool user_task_bootstrap_reset(user_task_bootstrap_t* bootstrap);
/* 规划标准布局：用 exec/ecall 符号地址算出各 region 虚拟地址与入口/栈顶。 */
bool user_task_bootstrap_plan_layout(user_task_bootstrap_t* bootstrap,
                                     uintptr_t exec_symbol,
                                     uintptr_t ecall_symbol);
/* 配置阶段：初始化各对象并把 region 与 user_task 关联。 */
bool user_task_bootstrap_configure(user_task_bootstrap_t* bootstrap,
                                   user_task_t* user_task,
                                   uintptr_t alias_backing_paddr,
                                   uintptr_t user_stack_paddr);
/* 绑定阶段：把 region 批量绑进 user_task 进程。 */
bool user_task_bootstrap_bind(user_task_bootstrap_t* bootstrap);
/* 标准准备：装 trap 上下文、校验回调与 timer/external post-handler。 */
bool user_task_bootstrap_prepare_standard(
    user_task_bootstrap_t* bootstrap,
    trap_context_t* trap_context,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);

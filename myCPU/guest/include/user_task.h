#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trap.h"
#include "vm.h"

/* 用户任务单元：把地址空间、进程与 trap user runtime 组合成
   create→prepare→activate→enter 的最小生命周期对象。 */
typedef struct UserTask {
    vm_address_space_t* address_space;
    vm_process_t process;
    trap_user_runtime_t runtime;
} user_task_t;

/* 初始化 user_task 为干净状态。 */
void user_task_init(user_task_t* user_task);
/* 创建地址空间与进程并绑定，失败回滚。 */
bool user_task_create(user_task_t* user_task);
/* 释放 runtime/进程/地址空间并回到 init 状态。 */
bool user_task_destroy(user_task_t* user_task);
/* 取地址空间指针（未创建返回 NULL）。 */
vm_address_space_t* user_task_address_space(user_task_t* user_task);
/* 取进程指针（未创建返回 NULL）。 */
vm_process_t* user_task_process(user_task_t* user_task);
/* 取 trap user runtime 指针。 */
trap_user_runtime_t* user_task_runtime(user_task_t* user_task);
/* 批量绑定 region 到进程，失败整体回滚。 */
bool user_task_bind_regions(
    user_task_t* user_task,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count);
/* 立即把对象映射到进程用户区（指定偏移）。 */
bool user_task_map_object_region_at(user_task_t* user_task,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object,
                                    size_t object_offset);
/* 同上，偏移为 0。 */
bool user_task_map_object_region(user_task_t* user_task,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags,
                                 vm_object_t* object);
/* 把对象设为进程用户区的 fault 对象（指定偏移）。 */
bool user_task_set_fault_object_region_at(user_task_t* user_task,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object,
                                          size_t object_offset);
/* 同上，偏移为 0。 */
bool user_task_set_fault_object_region(user_task_t* user_task,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object);
/* 标准准备：装 trap 上下文、设入口/栈/arg0、挂校验与中断 post-handler。 */
bool user_task_prepare_standard(
    user_task_t* user_task,
    trap_context_t* trap_context,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);
/* 激活 user runtime。 */
bool user_task_activate(user_task_t* user_task);
/* 取消激活。 */
bool user_task_deactivate(user_task_t* user_task);
/* user runtime 是否当前活跃。 */
bool user_task_is_active(const user_task_t* user_task);
/* 进程是否可运行。 */
bool user_task_is_runnable(const user_task_t* user_task);
/* 进入 U-mode 执行（sret）。 */
bool user_task_enter(const user_task_t* user_task);

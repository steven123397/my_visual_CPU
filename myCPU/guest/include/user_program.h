#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user_task_bootstrap.h"

/* 标准用户程序生命周期装配：把 user_task + bootstrap 组合成
   plan→create→prepare→activate→enter 的标准 smoke 编排单元。 */
typedef enum UserProgramValueId {
    USER_PROGRAM_VALUE_EXEC_PAGE_PADDR = 0,
    USER_PROGRAM_VALUE_EXEC_VADDR,
    USER_PROGRAM_VALUE_STACK_VADDR,
    USER_PROGRAM_VALUE_ALIAS_VADDR,
    USER_PROGRAM_VALUE_ANON_VADDR,
    USER_PROGRAM_VALUE_ANON_TAIL_VADDR,
    USER_PROGRAM_VALUE_ENTRY_PC,
    USER_PROGRAM_VALUE_EXPECTED_ECALL_PC,
    USER_PROGRAM_VALUE_USER_SP,
} user_program_value_id_t;

typedef enum UserProgramRegionId {
    USER_PROGRAM_REGION_EXEC = 0,
    USER_PROGRAM_REGION_STACK,
    USER_PROGRAM_REGION_ALIAS,
    USER_PROGRAM_REGION_ANON,
    USER_PROGRAM_REGION_ANON_TAIL,
} user_program_region_id_t;

typedef enum UserProgramObjectId {
    USER_PROGRAM_OBJECT_EXEC = 0,
    USER_PROGRAM_OBJECT_STACK,
    USER_PROGRAM_OBJECT_ALIAS,
    USER_PROGRAM_OBJECT_ANON,
} user_program_object_id_t;

typedef struct UserProgram {
    user_task_t user_task;
    user_task_bootstrap_t bootstrap;
} user_program_t;

/* 初始化 user_program（user_task + bootstrap 都置为干净状态）。 */
void user_program_init(user_program_t* program);
/* 复位生命周期并回到 init 状态。 */
bool user_program_destroy(user_program_t* program);
/* 规划标准布局：用 exec/ecall 符号地址算出各 region/object 的虚拟地址。 */
bool user_program_plan_standard(user_program_t* program,
                                uintptr_t exec_symbol,
                                uintptr_t ecall_symbol);
/* 创建 user_task 并按规划配置/绑定 region，失败自动回滚到 planned。 */
bool user_program_create(user_program_t* program,
                         uintptr_t alias_backing_paddr,
                         uintptr_t user_stack_paddr);
/* 取底层地址空间指针（未创建返回 NULL）。 */
vm_address_space_t* user_program_address_space(user_program_t* program);
/* 取底层进程指针（未创建返回 NULL）。 */
vm_process_t* user_program_process(user_program_t* program);
/* 取底层 trap user runtime 指针（未创建返回 NULL）。 */
trap_user_runtime_t* user_program_runtime(user_program_t* program);
/* 立即把对象映射到 user_task 用户区（指定偏移）。 */
bool user_program_map_object_region_at(user_program_t* program,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object,
                                       size_t object_offset);
/* 同上，偏移为 0。 */
bool user_program_map_object_region(user_program_t* program,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object);
/* 把对象设为 user_task 用户区的 fault 对象（指定偏移）。 */
bool user_program_set_fault_object_region_at(user_program_t* program,
                                             vm_user_region_t* region,
                                             uintptr_t vaddr,
                                             size_t size,
                                             uint64_t flags,
                                             vm_object_t* object,
                                             size_t object_offset);
/* 同上，偏移为 0。 */
bool user_program_set_fault_object_region(user_program_t* program,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object);
/* 标准准备：装 trap 上下文、校验回调与 timer/external post-handler。 */
bool user_program_prepare_standard(
    user_program_t* program,
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
/* 激活 user_task（开启地址空间并登记为当前进程）。 */
bool user_program_activate(user_program_t* program);
/* 取消激活。 */
bool user_program_deactivate(user_program_t* program);
/* user_task 是否当前活跃。 */
bool user_program_is_active(const user_program_t* program);
/* user_task 是否可运行（上下文有效）。 */
bool user_program_is_runnable(const user_program_t* program);
/* 进入 U-mode 执行用户程序（sret）。 */
bool user_program_enter(const user_program_t* program);
/* 按 value_id 取规划好的展示值（虚拟地址/入口/栈顶等）。 */
uintptr_t user_program_value(const user_program_t* program,
                             user_program_value_id_t value_id);
/* vaddr..+size 是否落在指定 region 内。 */
bool user_program_region_contains(const user_program_t* program,
                                  user_program_region_id_t region_id,
                                  uintptr_t vaddr,
                                  size_t size);
/* 解除指定 region 内某页映射。 */
bool user_program_unmap_region_page(user_program_t* program,
                                    user_program_region_id_t region_id,
                                    uintptr_t vaddr);
/* 解除指定 region 起始页映射。 */
bool user_program_unmap_region_base_page(user_program_t* program,
                                         user_program_region_id_t region_id);
/* 给指定 region 设 fault 对象。 */
bool user_program_set_region_fault_object(user_program_t* program,
                                          user_program_region_id_t region_id,
                                          vm_object_t* object);
/* 重新绑定指定 region 的 fault 对象，失败回滚原绑定。 */
bool user_program_rebind_region_fault_object(
    user_program_t* program,
    user_program_region_id_t region_id,
    vm_object_t* object);
/* 复位指定对象。 */
bool user_program_reset_object(user_program_t* program,
                               user_program_object_id_t object_id);
/* 按 region_id 取可写 region 指针。 */
vm_user_region_t* user_program_region(user_program_t* program,
                                      user_program_region_id_t region_id);
/* 按 object_id 取对象指针。 */
vm_object_t* user_program_object(user_program_t* program,
                                 user_program_object_id_t object_id);

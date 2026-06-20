#include "trap.h"

#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "riscv.h"
#include "runtime_context.h"
#include "vm.h"

extern void trap_user_runtime_arch_enter(trap_user_runtime_t* user_runtime,
                                         uintptr_t entry,
                                         uintptr_t arg0,
                                         uintptr_t user_sp);
extern void trap_user_runtime_arch_call(trap_user_runtime_t* user_runtime,
                                        uintptr_t entry,
                                        uintptr_t arg0,
                                        uintptr_t user_sp);
extern void trap_user_runtime_arch_resume(void);

/* 当前活跃 user runtime 单例。 */
static trap_user_runtime_t* active_user_runtime = NULL;
/* 前向声明：校验 user runtime 栈有效性。 */
static bool user_runtime_stack_valid(const trap_user_runtime_t* user_runtime);
typedef struct TrapUserRuntimeRollbackEntry {
    trap_user_runtime_t* runtime;
    trap_user_runtime_t snapshot;
} trap_user_runtime_rollback_entry_t;

typedef struct TrapContextBindingSnapshot {
    trap_supervisor_timer_policy_t supervisor_timer_policy;
    trap_supervisor_external_policy_t supervisor_external_policy;
    trap_user_ecall_policy_t user_ecall_policy;
} trap_context_binding_snapshot_t;

typedef struct TrapContextStandardHandlerSnapshot {
    trap_interrupt_handler_entry_t supervisor_timer_handler;
    trap_interrupt_handler_entry_t supervisor_external_handler;
    trap_exception_handler_entry_t user_ecall_handler;
} trap_context_standard_handler_snapshot_t;

typedef struct TrapUserRuntimeBindRollback {
    trap_context_t* target_trap_context;
    trap_context_binding_snapshot_t target_trap_context_snapshot;
    bool target_trap_context_valid;
    trap_context_t* previous_trap_context;
    trap_context_binding_snapshot_t previous_trap_context_snapshot;
    bool previous_trap_context_valid;
    trap_user_runtime_t* active_runtime_snapshot;
    trap_user_runtime_rollback_entry_t runtime_entries[4];
    size_t runtime_entry_count;
} trap_user_runtime_bind_rollback_t;

typedef struct TrapUserRuntimeStandardRollback {
    trap_user_runtime_bind_rollback_t bind;
    trap_context_standard_handler_snapshot_t target_handler_snapshot;
    bool target_handler_snapshot_valid;
} trap_user_runtime_standard_rollback_t;

_Static_assert(sizeof(trap_user_runtime_bind_rollback_t) <= 1024U,
               "trap runtime bind rollback grew too large for guest stack budget");
_Static_assert(sizeof(trap_user_runtime_standard_rollback_t) <= 1152U,
               "trap runtime standard rollback grew too large for guest stack budget");

/* user runtime 基本字段是否有效（已绑 context/process）。 */
static bool user_runtime_valid(const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL &&
           user_runtime->trap_context != NULL &&
           user_runtime->process != NULL;
}

/* bind 阶段参数是否可绑定（runtime/context/process 都非空）。 */
static bool user_runtime_prepare_binding_valid(trap_user_runtime_t* user_runtime,
                                               trap_context_t* trap_context,
                                               vm_process_t* process) {
    return user_runtime != NULL && trap_context != NULL && process != NULL &&
           process->address_space != NULL;
}

/* trap 栈参数是否合法（非空、对齐、够大）。 */
static bool user_runtime_stack_args_valid(void* trap_stack_base,
                                          size_t trap_stack_size) {
    const uintptr_t stack_base = (uintptr_t)trap_stack_base;

    return trap_stack_base != NULL &&
           trap_stack_size >= TRAP_USER_RUNTIME_MIN_STACK_SIZE &&
           (stack_base & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0 &&
           (trap_stack_size & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0 &&
           stack_base <= UINTPTR_MAX - (uintptr_t)trap_stack_size;
}

/* prepare 阶段全部参数是否合法。 */
static bool user_runtime_prepare_args_valid(trap_user_runtime_t* user_runtime,
                                            trap_context_t* trap_context,
                                            vm_process_t* process,
                                            void* trap_stack_base,
                                            size_t trap_stack_size,
                                            uintptr_t expected_ecall_pc) {
    return user_runtime_prepare_binding_valid(user_runtime, trap_context, process) &&
           user_runtime_stack_args_valid(trap_stack_base, trap_stack_size) &&
           expected_ecall_pc != 0;
}

/* 快照 context 的 timer/external policy，用于回滚。 */
static trap_context_binding_snapshot_t capture_context_binding_snapshot(
    const trap_context_t* trap_context) {
    trap_context_binding_snapshot_t snapshot;

    snapshot.supervisor_timer_policy.user_runtime = NULL;
    snapshot.supervisor_timer_policy.post_handler = NULL;
    snapshot.supervisor_timer_policy.post_context = NULL;
    snapshot.supervisor_external_policy.user_runtime = NULL;
    snapshot.supervisor_external_policy.post_handler = NULL;
    snapshot.supervisor_external_policy.post_context = NULL;
    snapshot.user_ecall_policy.user_runtime = NULL;
    snapshot.user_ecall_policy.syscalls = NULL;
    snapshot.user_ecall_policy.linux_runtime = NULL;
    snapshot.user_ecall_policy.validate = NULL;
    snapshot.user_ecall_policy.validate_context = NULL;
    snapshot.user_ecall_policy.resume_pc = 0;

    if (trap_context == NULL) {
        return snapshot;
    }

    snapshot.supervisor_timer_policy = trap_context->supervisor_timer_policy;
    snapshot.supervisor_external_policy =
        trap_context->supervisor_external_policy;
    snapshot.user_ecall_policy = trap_context->user_ecall_policy;
    return snapshot;
}

/* 用快照恢复 context 的 timer/external policy。 */
static void restore_context_binding_snapshot(
    trap_context_t* trap_context,
    const trap_context_binding_snapshot_t* snapshot) {
    if (trap_context == NULL || snapshot == NULL) {
        return;
    }

    trap_context->supervisor_timer_policy = snapshot->supervisor_timer_policy;
    trap_context->supervisor_external_policy =
        snapshot->supervisor_external_policy;
    trap_context->user_ecall_policy = snapshot->user_ecall_policy;
}

/* 快照 standard policy 安装前的 handler 槽，用于回滚。 */
static trap_context_standard_handler_snapshot_t capture_standard_handler_snapshot(
    const trap_context_t* trap_context) {
    trap_context_standard_handler_snapshot_t snapshot;

    snapshot.supervisor_timer_handler.handler = NULL;
    snapshot.supervisor_timer_handler.context = NULL;
    snapshot.supervisor_external_handler.handler = NULL;
    snapshot.supervisor_external_handler.context = NULL;
    snapshot.user_ecall_handler.handler = NULL;
    snapshot.user_ecall_handler.context = NULL;

    if (trap_context == NULL) {
        return snapshot;
    }

    snapshot.supervisor_timer_handler =
        trap_context->interrupt_handlers[RISCV_SUPERVISOR_TIMER_INTERRUPT];
    snapshot.supervisor_external_handler =
        trap_context->interrupt_handlers[RISCV_SUPERVISOR_EXTERNAL_INTERRUPT];
    snapshot.user_ecall_handler =
        trap_context->exception_handlers[RISCV_EXC_ECALL_FROM_U];
    return snapshot;
}

/* 用快照恢复 standard handler 槽。 */
static void restore_standard_handler_snapshot(
    trap_context_t* trap_context,
    const trap_context_standard_handler_snapshot_t* snapshot) {
    if (trap_context == NULL || snapshot == NULL) {
        return;
    }

    trap_context->interrupt_handlers[RISCV_SUPERVISOR_TIMER_INTERRUPT] =
        snapshot->supervisor_timer_handler;
    trap_context->interrupt_handlers[RISCV_SUPERVISOR_EXTERNAL_INTERRUPT] =
        snapshot->supervisor_external_handler;
    trap_context->exception_handlers[RISCV_EXC_ECALL_FROM_U] =
        snapshot->user_ecall_handler;
}

/* 记录 runtime 回滚入口（保存 runtime 指针与快照）。 */
static void capture_runtime_rollback_entry(
    trap_user_runtime_bind_rollback_t* rollback,
    trap_user_runtime_t* runtime) {
    size_t i = 0;

    if (rollback == NULL || runtime == NULL) {
        return;
    }

    for (i = 0; i < rollback->runtime_entry_count; ++i) {
        if (rollback->runtime_entries[i].runtime == runtime) {
            return;
        }
    }

    if (rollback->runtime_entry_count >=
        (sizeof(rollback->runtime_entries) /
         sizeof(rollback->runtime_entries[0]))) {
        return;
    }

    rollback->runtime_entries[rollback->runtime_entry_count].runtime = runtime;
    rollback->runtime_entries[rollback->runtime_entry_count].snapshot = *runtime;
    rollback->runtime_entry_count += 1U;
}

/* 准备 bind 阶段的回滚条目。 */
static void prepare_runtime_bind_rollback(
    trap_user_runtime_bind_rollback_t* rollback,
    trap_user_runtime_t* user_runtime,
    trap_context_t* trap_context) {
    if (rollback == NULL) {
        return;
    }

    rollback->target_trap_context = trap_context;
    rollback->target_trap_context_valid = trap_context != NULL;
    if (trap_context != NULL) {
        rollback->target_trap_context_snapshot =
            capture_context_binding_snapshot(trap_context);
    }

    rollback->previous_trap_context =
        user_runtime != NULL ? user_runtime->trap_context : NULL;
    rollback->previous_trap_context_valid =
        rollback->previous_trap_context != NULL &&
        rollback->previous_trap_context != trap_context;
    if (rollback->previous_trap_context_valid) {
        rollback->previous_trap_context_snapshot = capture_context_binding_snapshot(
            rollback->previous_trap_context);
    }

    rollback->active_runtime_snapshot = active_user_runtime;
    rollback->runtime_entry_count = 0;
    capture_runtime_rollback_entry(rollback, user_runtime);
    if (trap_context != NULL) {
        capture_runtime_rollback_entry(
            rollback,
            trap_context->supervisor_timer_policy.user_runtime);
        capture_runtime_rollback_entry(
            rollback,
            trap_context->supervisor_external_policy.user_runtime);
        capture_runtime_rollback_entry(rollback,
                                       trap_context->user_ecall_policy.user_runtime);
    }
}

/* 准备 standard 阶段的回滚条目。 */
static void prepare_runtime_standard_rollback(
    trap_user_runtime_standard_rollback_t* rollback,
    trap_user_runtime_t* user_runtime,
    trap_context_t* trap_context) {
    if (rollback == NULL) {
        return;
    }

    prepare_runtime_bind_rollback(&rollback->bind, user_runtime, trap_context);
    rollback->target_handler_snapshot_valid = trap_context != NULL;
    if (rollback->target_handler_snapshot_valid) {
        rollback->target_handler_snapshot =
            capture_standard_handler_snapshot(trap_context);
    }
}

/* 回滚 bind 阶段已改动的 runtime/context。 */
static void rollback_prepared_bind_runtime(
    const trap_user_runtime_bind_rollback_t* rollback) {
    size_t i = 0;

    if (rollback == NULL) {
        return;
    }

    for (i = 0; i < rollback->runtime_entry_count; ++i) {
        trap_user_runtime_rollback_entry_t entry = rollback->runtime_entries[i];

        if (entry.runtime != NULL) {
            *entry.runtime = entry.snapshot;
        }
    }

    if (rollback->previous_trap_context_valid) {
        restore_context_binding_snapshot(
            rollback->previous_trap_context,
            &rollback->previous_trap_context_snapshot);
    }
    if (rollback->target_trap_context_valid) {
        restore_context_binding_snapshot(
            rollback->target_trap_context,
            &rollback->target_trap_context_snapshot);
    }

    active_user_runtime = rollback->active_runtime_snapshot;
}

/* 回滚 standard 阶段已改动的 runtime/context/policy。 */
static void rollback_prepared_standard_runtime(
    const trap_user_runtime_standard_rollback_t* rollback) {
    if (rollback == NULL) {
        return;
    }

    rollback_prepared_bind_runtime(&rollback->bind);
    if (rollback->target_handler_snapshot_valid) {
        restore_standard_handler_snapshot(
            rollback->bind.target_trap_context,
            &rollback->target_handler_snapshot);
    }
}

/* user runtime 是否满足激活条件。 */
static bool user_runtime_activation_ready(const trap_user_runtime_t* user_runtime) {
    return user_runtime_valid(user_runtime) &&
           user_runtime_stack_valid(user_runtime) &&
           vm_process_is_runnable(user_runtime->process);
}

/* 清空一个 user signal 装配。 */
static void clear_user_signal(trap_user_signal_t* signal) {
    if (signal == NULL) {
        return;
    }

    signal->page = NULL;
    signal->word_index = 0;
    signal->value = 0;
    signal->armed = false;
    signal->delivered = false;
}

/* 清空全部中断 handler 槽。 */
static void clear_interrupt_handlers(trap_context_t* trap_context) {
    uint64_t i = 0;

    if (trap_context == NULL) {
        return;
    }

    for (i = 0; i < TRAP_MAX_INTERRUPT_CAUSE; ++i) {
        trap_context->interrupt_handlers[i].handler = 0;
        trap_context->interrupt_handlers[i].context = 0;
    }
}

/* 清空全部异常 handler 槽。 */
static void clear_exception_handlers(trap_context_t* trap_context) {
    uint64_t i = 0;

    if (trap_context == NULL) {
        return;
    }

    for (i = 0; i < TRAP_MAX_EXCEPTION_CAUSE; ++i) {
        trap_context->exception_handlers[i].handler = 0;
        trap_context->exception_handlers[i].context = 0;
    }
}

/* 复位 prepare 阶段写入的 runtime 状态。 */
static void reset_prepared_runtime_state(trap_user_runtime_t* user_runtime) {
    if (user_runtime == NULL) {
        return;
    }

    if (active_user_runtime == user_runtime) {
        active_user_runtime = NULL;
    }

    user_runtime->arch_state.saved_supervisor_sp = 0;
    user_runtime->arch_state.saved_supervisor_ra = 0;
    user_runtime->arch_state.supervisor_trap_stack_top = 0;
    user_runtime->arch_state.supervisor_trap_stack_size = 0;
    user_runtime->expected_ecall_pc = 0;
    user_runtime->resume_pc = 0;
    user_runtime->validate = NULL;
    user_runtime->validate_context = NULL;
    clear_user_signal(&user_runtime->timer_signal);
    clear_user_signal(&user_runtime->external_signal);
}

/* 清掉 context 里指向该 runtime 的 policy 引用。 */
static void clear_runtime_policy_refs(trap_context_t* trap_context,
                                      const trap_user_runtime_t* user_runtime) {
    if (trap_context == NULL || user_runtime == NULL) {
        return;
    }

    if (trap_context->supervisor_timer_policy.user_runtime == user_runtime) {
        trap_context->supervisor_timer_policy.user_runtime = NULL;
    }

    if (trap_context->supervisor_external_policy.user_runtime == user_runtime) {
        trap_context->supervisor_external_policy.user_runtime = NULL;
    }

    if (trap_context->user_ecall_policy.user_runtime == user_runtime) {
        trap_context->user_ecall_policy.user_runtime = NULL;
        trap_context->user_ecall_policy.linux_runtime = NULL;
        trap_context->user_ecall_policy.validate = NULL;
        trap_context->user_ecall_policy.validate_context = NULL;
        trap_context->user_ecall_policy.resume_pc = 0;
    }
}

/* 从 context 的 policy 绑定中摘除该 runtime。 */
static void detach_runtime_from_context_binding(trap_user_runtime_t* user_runtime,
                                                trap_context_t* trap_context) {
    if (user_runtime == NULL || trap_context == NULL) {
        return;
    }

    clear_runtime_policy_refs(trap_context, user_runtime);
    if (user_runtime->trap_context == trap_context) {
        reset_prepared_runtime_state(user_runtime);
        user_runtime->trap_context = NULL;
    }
}

/* 校验 user runtime 的 supervisor trap 栈是否有效（定义在文件后段）。 */
static bool user_runtime_stack_valid(const trap_user_runtime_t* user_runtime) {
    const uintptr_t stack_top =
        user_runtime != NULL ? user_runtime->arch_state.supervisor_trap_stack_top : 0;
    const size_t stack_size =
        user_runtime != NULL ? user_runtime->arch_state.supervisor_trap_stack_size : 0;
    const uintptr_t stack_base =
        stack_top >= (uintptr_t)stack_size ? stack_top - (uintptr_t)stack_size : 0;

    return user_runtime != NULL &&
           stack_top != 0 &&
           stack_size >= TRAP_USER_RUNTIME_MIN_STACK_SIZE &&
           (stack_base & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0 &&
           (stack_size & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0 &&
           (stack_top & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0;
}

void trap_context_init(trap_context_t* trap_context) {
    if (trap_context == NULL) {
        return;
    }

    clear_interrupt_handlers(trap_context);
    clear_exception_handlers(trap_context);
    trap_context->supervisor_timer_policy.user_runtime = NULL;
    trap_context->supervisor_timer_policy.post_handler = NULL;
    trap_context->supervisor_timer_policy.post_context = NULL;
    trap_context->supervisor_external_policy.user_runtime = NULL;
    trap_context->supervisor_external_policy.post_handler = NULL;
    trap_context->supervisor_external_policy.post_context = NULL;
    trap_context->user_ecall_policy.user_runtime = NULL;
    trap_context->user_ecall_policy.syscalls = NULL;
    trap_context->user_ecall_policy.linux_runtime = NULL;
    trap_context->user_ecall_policy.validate = NULL;
    trap_context->user_ecall_policy.validate_context = NULL;
    trap_context->user_ecall_policy.resume_pc = 0;
    trap_context->user_crash_policy.handler = NULL;
    trap_context->user_crash_policy.context = NULL;
}

void trap_user_runtime_init(trap_user_runtime_t* user_runtime) {
    trap_context_t* trap_context = NULL;

    if (user_runtime == NULL) {
        return;
    }

    trap_context = user_runtime->trap_context;
    if (active_user_runtime == user_runtime) {
        active_user_runtime = NULL;
    }

    clear_runtime_policy_refs(trap_context, user_runtime);
    user_runtime->arch_state.saved_supervisor_sp = 0;
    user_runtime->arch_state.saved_supervisor_ra = 0;
    user_runtime->arch_state.supervisor_trap_stack_top = 0;
    user_runtime->arch_state.supervisor_trap_stack_size = 0;
    user_runtime->trap_context = NULL;
    user_runtime->process = NULL;
    user_runtime->arg0 = 0;
    reset_prepared_runtime_state(user_runtime);
}

bool trap_context_activate(trap_context_t* trap_context) {
    if (trap_context == NULL) {
        return false;
    }

    if (active_user_runtime != NULL &&
        active_user_runtime->trap_context != trap_context) {
        active_user_runtime = NULL;
    }

    runtime_context_activate_trap_context(trap_context);
    return true;
}

bool trap_context_is_active(const trap_context_t* trap_context) {
    return runtime_context_trap_context_is_active(trap_context);
}

trap_context_t* trap_active_context(void) {
    return runtime_context_active_trap_context();
}

trap_user_runtime_t* trap_active_user_runtime(void) {
    return active_user_runtime;
}

bool trap_user_runtime_bind(trap_user_runtime_t* user_runtime,
                            trap_context_t* trap_context,
                            vm_process_t* process,
                            uintptr_t arg0) {
    if (user_runtime == NULL || trap_context == NULL || process == NULL ||
        process->address_space == NULL) {
        return false;
    }

    detach_runtime_from_context_binding(user_runtime, user_runtime->trap_context);
    detach_runtime_from_context_binding(
        trap_context->supervisor_timer_policy.user_runtime, trap_context);
    detach_runtime_from_context_binding(
        trap_context->supervisor_external_policy.user_runtime, trap_context);
    detach_runtime_from_context_binding(
        trap_context->user_ecall_policy.user_runtime, trap_context);

    user_runtime->trap_context = trap_context;
    user_runtime->process = process;
    user_runtime->arg0 = arg0;
    reset_prepared_runtime_state(user_runtime);
    return true;
}

bool trap_user_runtime_prepare(trap_user_runtime_t* user_runtime,
                               trap_context_t* trap_context,
                               vm_process_t* process,
                               uintptr_t arg0,
                               void* trap_stack_base,
                               size_t trap_stack_size,
                               uintptr_t expected_ecall_pc,
                               trap_user_runtime_validate_t validate,
                               void* validate_context) {
    trap_user_runtime_bind_rollback_t rollback;

    if (!user_runtime_prepare_args_valid(user_runtime,
                                         trap_context,
                                         process,
                                         trap_stack_base,
                                         trap_stack_size,
                                         expected_ecall_pc)) {
        return false;
    }

    prepare_runtime_bind_rollback(&rollback, user_runtime, trap_context);
    if (trap_user_runtime_bind(user_runtime, trap_context, process, arg0) &&
        trap_user_runtime_configure_supervisor_trap_stack(user_runtime,
                                                          trap_stack_base,
                                                          trap_stack_size) &&
        trap_user_runtime_configure_ecall_resume(user_runtime,
                                                 expected_ecall_pc,
                                                 validate,
                                                 validate_context)) {
        return true;
    }

    rollback_prepared_bind_runtime(&rollback);
    return false;
}

bool trap_user_runtime_prepare_standard(
    trap_user_runtime_t* user_runtime,
    trap_context_t* trap_context,
    vm_process_t* process,
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
    void* supervisor_external_post_context) {
    trap_user_runtime_standard_rollback_t rollback;
    const uintptr_t previous_entry_pc = process != NULL ? process->entry_pc : 0;
    const uintptr_t previous_user_sp = process != NULL ? process->user_sp : 0;

    if (!user_runtime_prepare_args_valid(user_runtime,
                                         trap_context,
                                         process,
                                         trap_stack_base,
                                         trap_stack_size,
                                         expected_ecall_pc)) {
        return false;
    }

    prepare_runtime_standard_rollback(&rollback, user_runtime, trap_context);
    if (vm_process_set_user_context(process, entry_pc, user_sp) &&
        trap_user_runtime_bind(user_runtime, trap_context, process, arg0) &&
        trap_user_runtime_configure_supervisor_trap_stack(user_runtime,
                                                          trap_stack_base,
                                                          trap_stack_size) &&
        trap_user_runtime_configure_ecall_resume(user_runtime,
                                                 expected_ecall_pc,
                                                 validate,
                                                 validate_context) &&
        trap_context_install_standard_user_runtime_policies(
            trap_context,
            user_runtime,
            supervisor_timer_post_handler,
            supervisor_timer_post_context,
            supervisor_external_post_handler,
            supervisor_external_post_context)) {
        return true;
    }

    if (process != NULL) {
        process->entry_pc = previous_entry_pc;
        process->user_sp = previous_user_sp;
    }
    rollback_prepared_standard_runtime(&rollback);
    return false;
}

bool trap_user_runtime_configure_supervisor_trap_stack(
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size) {
    const uintptr_t stack_base = (uintptr_t)trap_stack_base;

    if (!user_runtime_valid(user_runtime) || trap_stack_base == NULL ||
        trap_stack_size < TRAP_USER_RUNTIME_MIN_STACK_SIZE ||
        (stack_base & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) != 0 ||
        (trap_stack_size & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) != 0 ||
        stack_base > UINTPTR_MAX - (uintptr_t)trap_stack_size) {
        return false;
    }

    user_runtime->arch_state.saved_supervisor_sp = 0;
    user_runtime->arch_state.saved_supervisor_ra = 0;
    user_runtime->arch_state.supervisor_trap_stack_top =
        stack_base + (uintptr_t)trap_stack_size;
    user_runtime->arch_state.supervisor_trap_stack_size = trap_stack_size;
    return true;
}

bool trap_user_runtime_configure_ecall_resume(
    trap_user_runtime_t* user_runtime,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context) {
    if (!user_runtime_valid(user_runtime) || expected_ecall_pc == 0) {
        return false;
    }

    user_runtime->expected_ecall_pc = expected_ecall_pc;
    user_runtime->resume_pc = (uintptr_t)trap_user_runtime_arch_resume;
    user_runtime->validate = validate;
    user_runtime->validate_context = validate_context;
    return true;
}

bool trap_user_runtime_arm_timer_signal(trap_user_runtime_t* user_runtime,
                                        uint32_t* page,
                                        size_t word_index,
                                        uint32_t value) {
    if (!user_runtime_valid(user_runtime) || page == NULL ||
        word_index >= (MEMORY_PAGE_SIZE / sizeof(uint32_t))) {
        return false;
    }

    user_runtime->timer_signal.page = page;
    user_runtime->timer_signal.word_index = word_index;
    user_runtime->timer_signal.value = value;
    user_runtime->timer_signal.armed = true;
    user_runtime->timer_signal.delivered = false;
    return true;
}

bool trap_user_runtime_timer_signal_delivered(
    const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL && user_runtime->timer_signal.delivered;
}

bool trap_user_runtime_arm_external_signal(trap_user_runtime_t* user_runtime,
                                           uint32_t* page,
                                           size_t word_index,
                                           uint32_t value) {
    if (!user_runtime_valid(user_runtime) || page == NULL ||
        word_index >= (MEMORY_PAGE_SIZE / sizeof(uint32_t))) {
        return false;
    }

    user_runtime->external_signal.page = page;
    user_runtime->external_signal.word_index = word_index;
    user_runtime->external_signal.value = value;
    user_runtime->external_signal.armed = true;
    user_runtime->external_signal.delivered = false;
    return true;
}

bool trap_user_runtime_external_signal_delivered(
    const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL && user_runtime->external_signal.delivered;
}

bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime) {
    if (!user_runtime_activation_ready(user_runtime)) {
        return false;
    }

    if (!vm_process_activate(user_runtime->process)) {
        return false;
    }

    if (!trap_context_activate(user_runtime->trap_context)) {
        return false;
    }

    active_user_runtime = user_runtime;
    return true;
}

bool trap_user_runtime_is_active(const trap_user_runtime_t* user_runtime) {
    return user_runtime_valid(user_runtime) &&
           trap_active_user_runtime() == user_runtime &&
           vm_process_is_active(user_runtime->process) &&
           trap_context_is_active(user_runtime->trap_context);
}

bool trap_user_runtime_deactivate(trap_user_runtime_t* user_runtime) {
    if (!trap_user_runtime_is_active(user_runtime)) {
        return false;
    }

    clear_user_signal(&user_runtime->timer_signal);
    clear_user_signal(&user_runtime->external_signal);
    riscv_clear_sstatus_bits(RISCV_SSTATUS_SUM);
    active_user_runtime = NULL;
    return vm_address_space_disable(user_runtime->process->address_space);
}

bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime) {
    if (!trap_user_runtime_is_active(user_runtime) ||
        !user_runtime_stack_valid(user_runtime)) {
        return false;
    }

    trap_user_runtime_arch_call((trap_user_runtime_t*)user_runtime,
                                user_runtime->process->entry_pc,
                                user_runtime->arg0,
                                user_runtime->process->user_sp);
    return true;
}

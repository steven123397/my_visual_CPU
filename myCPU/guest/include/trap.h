#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* Trap / 中断子系统：trap_context 注册各类 handler 与 policy，
   trap_user_runtime 承载进入 U-mode 的上下文与信号投递，dispatch 统一入口分发。 */
#define TRAP_MAX_INTERRUPT_CAUSE 16U
#define TRAP_MAX_EXCEPTION_CAUSE 16U
#define TRAP_USER_RUNTIME_STACK_ALIGNMENT 16U
#define TRAP_USER_RUNTIME_MIN_STACK_SIZE 256U

typedef struct VmProcess vm_process_t;
typedef struct TrapUserRuntime trap_user_runtime_t;
typedef struct CourseSyscall course_syscall_t;
typedef struct LinuxCompatRuntime linux_compat_runtime_t;

typedef void (*trap_interrupt_handler_t)(uint64_t cause, void* context);
typedef void (*trap_exception_handler_t)(uint64_t cause,
                                         uint64_t epc,
                                         uint64_t tval,
                                         void* context);
typedef bool (*trap_user_ecall_validate_t)(uint64_t epc,
                                           uint64_t tval,
                                           void* context);
typedef bool (*trap_user_runtime_validate_t)(const trap_user_runtime_t* user_runtime,
                                             uint64_t epc,
                                             uint64_t tval,
                                             void* context);
typedef bool (*trap_user_crash_handler_t)(uint64_t cause,
                                          uint64_t epc,
                                          uint64_t tval,
                                          void* context);
typedef void (*trap_supervisor_external_post_handler_t)(uint64_t cause,
                                                        uint32_t source_id,
                                                        void* context);

typedef struct TrapInterruptHandlerEntry {
    trap_interrupt_handler_t handler;
    void* context;
} trap_interrupt_handler_entry_t;

typedef struct TrapExceptionHandlerEntry {
    trap_exception_handler_t handler;
    void* context;
} trap_exception_handler_entry_t;

typedef struct TrapSupervisorTimerPolicy {
    trap_user_runtime_t* user_runtime;
    trap_interrupt_handler_t post_handler;
    void* post_context;
} trap_supervisor_timer_policy_t;

typedef struct TrapUserEcallPolicy {
    trap_user_runtime_t* user_runtime;
    course_syscall_t* syscalls;
    linux_compat_runtime_t* linux_runtime;
    trap_user_ecall_validate_t validate;
    void* validate_context;
    uintptr_t resume_pc;
} trap_user_ecall_policy_t;

typedef struct TrapSupervisorExternalPolicy {
    trap_user_runtime_t* user_runtime;
    trap_supervisor_external_post_handler_t post_handler;
    void* post_context;
} trap_supervisor_external_policy_t;

typedef struct TrapUserCrashPolicy {
    trap_user_crash_handler_t handler;
    void* context;
} trap_user_crash_policy_t;

typedef struct TrapUserSignal {
    uint32_t* page;
    size_t word_index;
    uint32_t value;
    bool armed;
    bool delivered;
} trap_user_signal_t;

typedef struct TrapFrame {
    uint64_t ra;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
} trap_frame_t;

typedef struct TrapUserArchState {
    uintptr_t saved_supervisor_sp;
    uintptr_t saved_supervisor_ra;
    uintptr_t supervisor_trap_stack_top;
    size_t supervisor_trap_stack_size;
} trap_user_arch_state_t;

typedef struct TrapUserRuntime {
    trap_user_arch_state_t arch_state;
    struct TrapContext* trap_context;
    vm_process_t* process;
    uintptr_t arg0;
    uintptr_t expected_ecall_pc;
    uintptr_t resume_pc;
    trap_user_runtime_validate_t validate;
    void* validate_context;
    trap_user_signal_t timer_signal;
    trap_user_signal_t external_signal;
} trap_user_runtime_t;

typedef struct TrapContext {
    trap_interrupt_handler_entry_t interrupt_handlers[TRAP_MAX_INTERRUPT_CAUSE];
    trap_exception_handler_entry_t exception_handlers[TRAP_MAX_EXCEPTION_CAUSE];
    trap_supervisor_timer_policy_t supervisor_timer_policy;
    trap_supervisor_external_policy_t supervisor_external_policy;
    trap_user_ecall_policy_t user_ecall_policy;
    trap_user_crash_policy_t user_crash_policy;
} trap_context_t;

/* 初始化 trap_context：清空 handler 表与各 policy。 */
void trap_context_init(trap_context_t* trap_context);
/* 初始化 user runtime 为干净未绑定状态。 */
void trap_user_runtime_init(trap_user_runtime_t* user_runtime);
/* 把 trap_context 登记为当前活跃。 */
bool trap_context_activate(trap_context_t* trap_context);
/* trap_context 是否为当前活跃。 */
bool trap_context_is_active(const trap_context_t* trap_context);
/* 取当前活跃 trap_context。 */
trap_context_t* trap_active_context(void);
/* 取当前活跃 user runtime。 */
trap_user_runtime_t* trap_active_user_runtime(void);
/* 把 user runtime 绑到 trap_context 与进程，记录 arg0（不做完整 prepare）。 */
bool trap_user_runtime_bind(trap_user_runtime_t* user_runtime,
                            trap_context_t* trap_context,
                            vm_process_t* process,
                            uintptr_t arg0);
/* 准备 user runtime：装 trap 栈、设 ecall 预期 PC 与校验回调。 */
bool trap_user_runtime_prepare(trap_user_runtime_t* user_runtime,
                               trap_context_t* trap_context,
                               vm_process_t* process,
                               uintptr_t arg0,
                               void* trap_stack_base,
                               size_t trap_stack_size,
                               uintptr_t expected_ecall_pc,
                               trap_user_runtime_validate_t validate,
                               void* validate_context);
/* 标准准备：设入口/栈/arg0，并安装 timer/external post-handler policy。 */
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
    void* supervisor_external_post_context);
/* 单独配置 supervisor trap 栈。 */
bool trap_user_runtime_configure_supervisor_trap_stack(
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size);
/* 单独配置 ecall 预期 PC 与校验回调。 */
bool trap_user_runtime_configure_ecall_resume(
    trap_user_runtime_t* user_runtime,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context);
/* 装配 timer 信号：中断到达时把 value 写入 page[word_index]。 */
bool trap_user_runtime_arm_timer_signal(trap_user_runtime_t* user_runtime,
                                        uint32_t* page,
                                        size_t word_index,
                                        uint32_t value);
/* timer 信号是否已投递。 */
bool trap_user_runtime_timer_signal_delivered(
    const trap_user_runtime_t* user_runtime);
/* 装配 external 信号：外部中断到达时把 value 写入 page[word_index]。 */
bool trap_user_runtime_arm_external_signal(trap_user_runtime_t* user_runtime,
                                           uint32_t* page,
                                           size_t word_index,
                                           uint32_t value);
/* external 信号是否已投递。 */
bool trap_user_runtime_external_signal_delivered(
    const trap_user_runtime_t* user_runtime);
/* 激活 user runtime（登记为活跃）。 */
bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime);
/* user runtime 是否当前活跃。 */
bool trap_user_runtime_is_active(const trap_user_runtime_t* user_runtime);
/* 取消激活。 */
bool trap_user_runtime_deactivate(trap_user_runtime_t* user_runtime);
/* 进入 U-mode 执行（sret）。 */
bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime);
/* 把 user runtime 绑定为 supervisor timer policy 的目标。 */
bool trap_context_bind_supervisor_timer_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
/* 把 user runtime 绑定为 supervisor external policy 的目标。 */
bool trap_context_bind_supervisor_external_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
/* 安装 supervisor timer post-handler。 */
bool trap_context_install_supervisor_timer_policy(
    trap_context_t* trap_context,
    trap_interrupt_handler_t post_handler,
    void* post_context);
/* 安装 supervisor external post-handler。 */
bool trap_context_install_supervisor_external_policy(
    trap_context_t* trap_context,
    trap_supervisor_external_post_handler_t post_handler,
    void* post_context);
/* 一次性安装 timer + external + ecall resume 三套标准 policy。 */
bool trap_context_install_standard_user_runtime_policies(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);
/* 安装 user runtime resume policy（异常后回到 U-mode）。 */
bool trap_context_install_user_runtime_resume_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
/* 安装 ecall resume policy（校验后回到预期 PC）。 */
bool trap_context_install_user_ecall_resume_policy(
    trap_context_t* trap_context,
    uintptr_t resume_pc,
    trap_user_ecall_validate_t validate,
    void* validate_context);
/* 安装课程 syscall policy（ecall 走 course_syscall_dispatch）。 */
bool trap_context_install_user_syscall_policy(trap_context_t* trap_context,
                                              course_syscall_t* syscalls);
/* 安装 Linux compat syscall policy（ecall 走 linux_compat 旁路）。 */
bool trap_context_install_linux_compat_syscall_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    linux_compat_runtime_t* runtime);
/* 安装用户态崩溃 policy（异常时回调 handler）。 */
bool trap_context_install_user_crash_policy(trap_context_t* trap_context,
                                            trap_user_crash_handler_t handler,
                                            void* context);
/* 安装指定 cause 的中断 handler。 */
bool trap_context_install_interrupt_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_interrupt_handler_t handler,
                                            void* context);
/* 安装指定 cause 的异常 handler。 */
bool trap_context_install_exception_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_exception_handler_t handler,
                                            void* context);
/* supervisor trap 总入口（汇编 trampoline 调用）。 */
void supervisor_trap_dispatch(void);
/* 同上，但额外保存/恢复 trap frame（caller-saved 寄存器）。 */
void supervisor_trap_dispatch_with_frame(trap_frame_t* frame);

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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

void trap_context_init(trap_context_t* trap_context);
void trap_user_runtime_init(trap_user_runtime_t* user_runtime);
bool trap_context_activate(trap_context_t* trap_context);
bool trap_context_is_active(const trap_context_t* trap_context);
trap_context_t* trap_active_context(void);
trap_user_runtime_t* trap_active_user_runtime(void);
bool trap_user_runtime_bind(trap_user_runtime_t* user_runtime,
                            trap_context_t* trap_context,
                            vm_process_t* process,
                            uintptr_t arg0);
bool trap_user_runtime_prepare(trap_user_runtime_t* user_runtime,
                               trap_context_t* trap_context,
                               vm_process_t* process,
                               uintptr_t arg0,
                               void* trap_stack_base,
                               size_t trap_stack_size,
                               uintptr_t expected_ecall_pc,
                               trap_user_runtime_validate_t validate,
                               void* validate_context);
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
bool trap_user_runtime_configure_supervisor_trap_stack(
    trap_user_runtime_t* user_runtime,
    void* trap_stack_base,
    size_t trap_stack_size);
bool trap_user_runtime_configure_ecall_resume(
    trap_user_runtime_t* user_runtime,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context);
bool trap_user_runtime_arm_timer_signal(trap_user_runtime_t* user_runtime,
                                        uint32_t* page,
                                        size_t word_index,
                                        uint32_t value);
bool trap_user_runtime_timer_signal_delivered(
    const trap_user_runtime_t* user_runtime);
bool trap_user_runtime_arm_external_signal(trap_user_runtime_t* user_runtime,
                                           uint32_t* page,
                                           size_t word_index,
                                           uint32_t value);
bool trap_user_runtime_external_signal_delivered(
    const trap_user_runtime_t* user_runtime);
bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime);
bool trap_user_runtime_is_active(const trap_user_runtime_t* user_runtime);
bool trap_user_runtime_deactivate(trap_user_runtime_t* user_runtime);
bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime);
bool trap_context_bind_supervisor_timer_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
bool trap_context_bind_supervisor_external_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
bool trap_context_install_supervisor_timer_policy(
    trap_context_t* trap_context,
    trap_interrupt_handler_t post_handler,
    void* post_context);
bool trap_context_install_supervisor_external_policy(
    trap_context_t* trap_context,
    trap_supervisor_external_post_handler_t post_handler,
    void* post_context);
bool trap_context_install_standard_user_runtime_policies(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);
bool trap_context_install_user_runtime_resume_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
bool trap_context_install_user_ecall_resume_policy(
    trap_context_t* trap_context,
    uintptr_t resume_pc,
    trap_user_ecall_validate_t validate,
    void* validate_context);
bool trap_context_install_user_syscall_policy(trap_context_t* trap_context,
                                              course_syscall_t* syscalls);
bool trap_context_install_linux_compat_syscall_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    linux_compat_runtime_t* runtime);
bool trap_context_install_user_crash_policy(trap_context_t* trap_context,
                                            trap_user_crash_handler_t handler,
                                            void* context);
bool trap_context_install_interrupt_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_interrupt_handler_t handler,
                                            void* context);
bool trap_context_install_exception_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_exception_handler_t handler,
                                            void* context);
void supervisor_trap_dispatch(void);
void supervisor_trap_dispatch_with_frame(trap_frame_t* frame);

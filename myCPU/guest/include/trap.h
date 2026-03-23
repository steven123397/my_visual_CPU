#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define TRAP_MAX_INTERRUPT_CAUSE 16U
#define TRAP_MAX_EXCEPTION_CAUSE 16U

typedef struct VmProcess vm_process_t;
typedef struct TrapUserRuntime trap_user_runtime_t;

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
typedef void (*trap_supervisor_external_post_handler_t)(uint64_t cause,
                                                        uint32_t source_id,
                                                        void* context);
typedef void (*trap_user_enter_fn_t)(uintptr_t entry,
                                     uintptr_t arg0,
                                     uintptr_t user_sp);

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
    trap_user_ecall_validate_t validate;
    void* validate_context;
    uintptr_t resume_pc;
} trap_user_ecall_policy_t;

typedef struct TrapSupervisorExternalPolicy {
    trap_supervisor_external_post_handler_t post_handler;
    void* post_context;
} trap_supervisor_external_policy_t;

typedef struct TrapUserTimerSignal {
    uint32_t* page;
    size_t word_index;
    uint32_t value;
    bool armed;
    bool delivered;
} trap_user_timer_signal_t;

typedef struct TrapUserRuntime {
    struct TrapContext* trap_context;
    vm_process_t* process;
    uintptr_t arg0;
    uintptr_t expected_ecall_pc;
    uintptr_t resume_pc;
    trap_user_runtime_validate_t validate;
    void* validate_context;
    trap_user_timer_signal_t timer_signal;
} trap_user_runtime_t;

typedef struct TrapContext {
    trap_interrupt_handler_entry_t interrupt_handlers[TRAP_MAX_INTERRUPT_CAUSE];
    trap_exception_handler_entry_t exception_handlers[TRAP_MAX_EXCEPTION_CAUSE];
    trap_supervisor_timer_policy_t supervisor_timer_policy;
    trap_supervisor_external_policy_t supervisor_external_policy;
    trap_user_ecall_policy_t user_ecall_policy;
} trap_context_t;

void trap_context_init(trap_context_t* trap_context);
void trap_user_runtime_init(trap_user_runtime_t* user_runtime);
bool trap_context_activate(trap_context_t* trap_context);
bool trap_context_is_active(const trap_context_t* trap_context);
trap_context_t* trap_active_context(void);
bool trap_user_runtime_bind(trap_user_runtime_t* user_runtime,
                            trap_context_t* trap_context,
                            vm_process_t* process,
                            uintptr_t arg0);
bool trap_user_runtime_configure_ecall_resume(
    trap_user_runtime_t* user_runtime,
    uintptr_t expected_ecall_pc,
    uintptr_t resume_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context);
bool trap_user_runtime_arm_timer_signal(trap_user_runtime_t* user_runtime,
                                        uint32_t* page,
                                        size_t word_index,
                                        uint32_t value);
bool trap_user_runtime_timer_signal_delivered(
    const trap_user_runtime_t* user_runtime);
bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime);
bool trap_user_runtime_is_active(const trap_user_runtime_t* user_runtime);
bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime,
                             trap_user_enter_fn_t enter_user);
bool trap_context_bind_supervisor_timer_user_runtime(
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
bool trap_context_install_user_runtime_resume_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime);
bool trap_context_install_user_ecall_resume_policy(
    trap_context_t* trap_context,
    uintptr_t resume_pc,
    trap_user_ecall_validate_t validate,
    void* validate_context);
bool trap_context_install_interrupt_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_interrupt_handler_t handler,
                                            void* context);
bool trap_context_install_exception_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_exception_handler_t handler,
                                            void* context);
void supervisor_trap_dispatch(void);

#include "trap.h"

#include <stdint.h>

#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "runtime_context.h"
#include "timer.h"
#include "vm.h"

extern void trap_user_runtime_arch_enter(trap_user_runtime_t* user_runtime,
                                         uintptr_t entry,
                                         uintptr_t arg0,
                                         uintptr_t user_sp);
extern void trap_user_runtime_arch_resume(void);

static trap_user_runtime_t* active_user_runtime = NULL;

static bool is_page_fault_cause(uint64_t cause) {
    return cause == RISCV_EXC_INSN_PAGE_FAULT ||
           cause == RISCV_EXC_LOAD_PAGE_FAULT ||
           cause == RISCV_EXC_STORE_PAGE_FAULT;
}

static bool user_runtime_valid(const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL &&
           user_runtime->trap_context != NULL &&
           user_runtime->process != NULL;
}

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

static void reset_prepared_runtime_state(trap_user_runtime_t* user_runtime) {
    if (user_runtime == NULL) {
        return;
    }

    if (active_user_runtime == user_runtime) {
        active_user_runtime = NULL;
    }

    user_runtime->arch_state.saved_supervisor_sp = 0;
    user_runtime->arch_state.supervisor_trap_stack_top = 0;
    user_runtime->arch_state.supervisor_trap_stack_size = 0;
    user_runtime->expected_ecall_pc = 0;
    user_runtime->resume_pc = 0;
    user_runtime->validate = NULL;
    user_runtime->validate_context = NULL;
    clear_user_signal(&user_runtime->timer_signal);
    clear_user_signal(&user_runtime->external_signal);
}

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
        trap_context->user_ecall_policy.validate = NULL;
        trap_context->user_ecall_policy.validate_context = NULL;
        trap_context->user_ecall_policy.resume_pc = 0;
    }
}

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

static bool user_runtime_timer_signal_valid(
    const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL &&
           user_runtime->timer_signal.page != NULL &&
           user_runtime->timer_signal.word_index <
               (MEMORY_PAGE_SIZE / sizeof(uint32_t));
}

static bool user_runtime_external_signal_valid(
    const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL &&
           user_runtime->external_signal.page != NULL &&
           user_runtime->external_signal.word_index <
               (MEMORY_PAGE_SIZE / sizeof(uint32_t));
}

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

static bool handle_user_timer_signal(trap_context_t* trap_context) {
    trap_user_runtime_t* user_runtime = NULL;

    if (trap_context == NULL) {
        return false;
    }

    user_runtime = trap_context->supervisor_timer_policy.user_runtime;
    if (user_runtime == NULL || !user_runtime->timer_signal.armed) {
        return true;
    }

    if (!user_runtime_timer_signal_valid(user_runtime)) {
        return false;
    }

    user_runtime->timer_signal.page[user_runtime->timer_signal.word_index] =
        user_runtime->timer_signal.value;
    user_runtime->timer_signal.armed = false;
    user_runtime->timer_signal.delivered = true;
    return true;
}

static bool handle_user_external_signal(trap_context_t* trap_context) {
    trap_user_runtime_t* user_runtime = NULL;

    if (trap_context == NULL) {
        return false;
    }

    user_runtime = trap_context->supervisor_external_policy.user_runtime;
    if (user_runtime == NULL || !user_runtime->external_signal.armed) {
        return true;
    }

    if (!user_runtime_external_signal_valid(user_runtime)) {
        return false;
    }

    user_runtime->external_signal.page[user_runtime->external_signal.word_index] =
        user_runtime->external_signal.value;
    user_runtime->external_signal.armed = false;
    user_runtime->external_signal.delivered = true;
    return true;
}

static void default_supervisor_timer_handler(uint64_t cause, void* context) {
    trap_context_t* trap_context = (trap_context_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || trap_context == NULL) {
        panic_shutdown();
    }

    timer_handle_interrupt();
    if (!handle_user_timer_signal(trap_context)) {
        panic_shutdown();
    }
    if (trap_context->supervisor_timer_policy.post_handler != NULL) {
        trap_context->supervisor_timer_policy.post_handler(
            cause, trap_context->supervisor_timer_policy.post_context);
    }
}

static void default_supervisor_external_handler(uint64_t cause, void* context) {
    const uint32_t source_id = platform_plic_supervisor_claim();
    trap_context_t* trap_context = (trap_context_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || trap_context == NULL ||
        source_id == 0) {
        panic_shutdown();
    }

    if (!handle_user_external_signal(trap_context)) {
        panic_shutdown();
    }
    if (trap_context->supervisor_external_policy.post_handler != NULL) {
        trap_context->supervisor_external_policy.post_handler(
            cause,
            source_id,
            trap_context->supervisor_external_policy.post_context);
    }
    platform_plic_supervisor_complete(source_id);
}

static void default_user_ecall_resume_handler(uint64_t cause,
                                              uint64_t epc,
                                              uint64_t tval,
                                              void* context) {
    const uint64_t sstatus = riscv_read_sstatus();
    trap_context_t* trap_context = (trap_context_t*)context;
    trap_user_runtime_t* user_runtime = NULL;
    uintptr_t resume_pc = 0;
    bool validate_ok = true;

    if (trap_context == NULL || cause != RISCV_EXC_ECALL_FROM_U || tval != 0 ||
        (sstatus & RISCV_SSTATUS_SPP) != 0 ||
        (sstatus & RISCV_SSTATUS_SPIE) == 0) {
        panic_shutdown();
    }

    user_runtime = trap_context->user_ecall_policy.user_runtime;
    if (user_runtime != NULL) {
        if (!user_runtime_valid(user_runtime) ||
            trap_active_user_runtime() != user_runtime ||
            user_runtime->trap_context != trap_context ||
            user_runtime->expected_ecall_pc == 0 ||
            epc != user_runtime->expected_ecall_pc ||
            user_runtime->resume_pc == 0) {
            panic_shutdown();
        }

        if (user_runtime->validate != NULL) {
            validate_ok = user_runtime->validate(user_runtime,
                                                 epc,
                                                 tval,
                                                 user_runtime->validate_context);
        }
        resume_pc = user_runtime->resume_pc;
    } else {
        if (trap_context->user_ecall_policy.resume_pc == 0) {
            panic_shutdown();
        }

        if (trap_context->user_ecall_policy.validate != NULL) {
            validate_ok = trap_context->user_ecall_policy.validate(
                epc, tval, trap_context->user_ecall_policy.validate_context);
        }
        resume_pc = trap_context->user_ecall_policy.resume_pc;
    }

    if (!validate_ok) {
        panic_shutdown();
    }

    riscv_set_sstatus_bits(RISCV_SSTATUS_SPP);
    riscv_write_sepc(resume_pc);
}

void trap_context_init(trap_context_t* trap_context) {
    uint64_t i = 0;

    if (trap_context == NULL) {
        return;
    }

    for (i = 0; i < TRAP_MAX_INTERRUPT_CAUSE; ++i) {
        trap_context->interrupt_handlers[i].handler = 0;
        trap_context->interrupt_handlers[i].context = 0;
    }

    for (i = 0; i < TRAP_MAX_EXCEPTION_CAUSE; ++i) {
        trap_context->exception_handlers[i].handler = 0;
        trap_context->exception_handlers[i].context = 0;
    }

    trap_context->supervisor_timer_policy.user_runtime = NULL;
    trap_context->supervisor_timer_policy.post_handler = NULL;
    trap_context->supervisor_timer_policy.post_context = NULL;
    trap_context->supervisor_external_policy.user_runtime = NULL;
    trap_context->supervisor_external_policy.post_handler = NULL;
    trap_context->supervisor_external_policy.post_context = NULL;
    trap_context->user_ecall_policy.user_runtime = NULL;
    trap_context->user_ecall_policy.validate = NULL;
    trap_context->user_ecall_policy.validate_context = NULL;
    trap_context->user_ecall_policy.resume_pc = 0;
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
    return trap_user_runtime_bind(user_runtime, trap_context, process, arg0) &&
           trap_user_runtime_configure_supervisor_trap_stack(user_runtime,
                                                             trap_stack_base,
                                                             trap_stack_size) &&
           trap_user_runtime_configure_ecall_resume(user_runtime,
                                                    expected_ecall_pc,
                                                    validate,
                                                    validate_context);
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
    return vm_process_set_user_context(process, entry_pc, user_sp) &&
           trap_user_runtime_prepare(user_runtime,
                                     trap_context,
                                     process,
                                     arg0,
                                     trap_stack_base,
                                     trap_stack_size,
                                     expected_ecall_pc,
                                     validate,
                                     validate_context) &&
           trap_context_install_standard_user_runtime_policies(
               trap_context,
               user_runtime,
               supervisor_timer_post_handler,
               supervisor_timer_post_context,
               supervisor_external_post_handler,
               supervisor_external_post_context);
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
    if (!user_runtime_valid(user_runtime) ||
        !user_runtime_stack_valid(user_runtime) ||
        !vm_process_is_runnable(user_runtime->process)) {
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

    riscv_clear_sstatus_bits(RISCV_SSTATUS_SUM);
    active_user_runtime = NULL;
    return vm_address_space_disable(user_runtime->process->address_space);
}

bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime) {
    if (!trap_user_runtime_is_active(user_runtime) ||
        !user_runtime_stack_valid(user_runtime)) {
        return false;
    }

    trap_user_runtime_arch_enter((trap_user_runtime_t*)user_runtime,
                                 user_runtime->process->entry_pc,
                                 user_runtime->arg0,
                                 user_runtime->process->user_sp);
    return true;
}

bool trap_context_bind_supervisor_timer_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime) {
    if (trap_context == NULL || !user_runtime_valid(user_runtime) ||
        user_runtime->trap_context != trap_context) {
        return false;
    }

    trap_context->supervisor_timer_policy.user_runtime = user_runtime;
    return true;
}

bool trap_context_bind_supervisor_external_user_runtime(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime) {
    if (trap_context == NULL || !user_runtime_valid(user_runtime) ||
        user_runtime->trap_context != trap_context) {
        return false;
    }

    trap_context->supervisor_external_policy.user_runtime = user_runtime;
    return true;
}

bool trap_context_install_supervisor_timer_policy(
    trap_context_t* trap_context,
    trap_interrupt_handler_t post_handler,
    void* post_context) {
    if (trap_context == NULL) {
        return false;
    }

    trap_context->supervisor_timer_policy.post_handler = post_handler;
    trap_context->supervisor_timer_policy.post_context = post_context;
    return trap_context_install_interrupt_handler(
        trap_context,
        RISCV_SUPERVISOR_TIMER_INTERRUPT,
        default_supervisor_timer_handler,
        trap_context);
}

bool trap_context_install_supervisor_external_policy(
    trap_context_t* trap_context,
    trap_supervisor_external_post_handler_t post_handler,
    void* post_context) {
    if (trap_context == NULL) {
        return false;
    }

    trap_context->supervisor_external_policy.post_handler = post_handler;
    trap_context->supervisor_external_policy.post_context = post_context;
    return trap_context_install_interrupt_handler(
        trap_context,
        RISCV_SUPERVISOR_EXTERNAL_INTERRUPT,
        default_supervisor_external_handler,
        trap_context);
}

bool trap_context_install_standard_user_runtime_policies(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context) {
    if (trap_context == NULL ||
        !trap_context_install_supervisor_timer_policy(
            trap_context,
            supervisor_timer_post_handler,
            supervisor_timer_post_context) ||
        !trap_context_install_supervisor_external_policy(
            trap_context,
            supervisor_external_post_handler,
            supervisor_external_post_context)) {
        return false;
    }

    if (user_runtime == NULL) {
        return true;
    }

    return trap_context_bind_supervisor_timer_user_runtime(trap_context,
                                                           user_runtime) &&
           trap_context_bind_supervisor_external_user_runtime(trap_context,
                                                           user_runtime) &&
           trap_context_install_user_runtime_resume_policy(trap_context,
                                                           user_runtime);
}

bool trap_context_install_user_runtime_resume_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime) {
    if (trap_context == NULL || !user_runtime_valid(user_runtime) ||
        user_runtime->trap_context != trap_context ||
        user_runtime->resume_pc == 0 || user_runtime->expected_ecall_pc == 0) {
        return false;
    }

    trap_context->user_ecall_policy.user_runtime = user_runtime;
    trap_context->user_ecall_policy.validate = NULL;
    trap_context->user_ecall_policy.validate_context = NULL;
    trap_context->user_ecall_policy.resume_pc = 0;
    return trap_context_install_exception_handler(
        trap_context,
        RISCV_EXC_ECALL_FROM_U,
        default_user_ecall_resume_handler,
        trap_context);
}

bool trap_context_install_user_ecall_resume_policy(
    trap_context_t* trap_context,
    uintptr_t resume_pc,
    trap_user_ecall_validate_t validate,
    void* validate_context) {
    if (trap_context == NULL || resume_pc == 0) {
        return false;
    }

    trap_context->user_ecall_policy.user_runtime = NULL;
    trap_context->user_ecall_policy.validate = validate;
    trap_context->user_ecall_policy.validate_context = validate_context;
    trap_context->user_ecall_policy.resume_pc = resume_pc;
    return trap_context_install_exception_handler(
        trap_context,
        RISCV_EXC_ECALL_FROM_U,
        default_user_ecall_resume_handler,
        trap_context);
}

bool trap_context_install_interrupt_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_interrupt_handler_t handler,
                                            void* context) {
    if (trap_context == NULL || cause >= TRAP_MAX_INTERRUPT_CAUSE || handler == 0) {
        return false;
    }

    trap_context->interrupt_handlers[cause].handler = handler;
    trap_context->interrupt_handlers[cause].context = context;
    return true;
}

bool trap_context_install_exception_handler(trap_context_t* trap_context,
                                            uint64_t cause,
                                            trap_exception_handler_t handler,
                                            void* context) {
    if (trap_context == NULL || cause >= TRAP_MAX_EXCEPTION_CAUSE ||
        handler == 0) {
        return false;
    }

    trap_context->exception_handlers[cause].handler = handler;
    trap_context->exception_handlers[cause].context = context;
    return true;
}

void supervisor_trap_dispatch(void) {
    const uint64_t scause = riscv_read_scause();
    const uint64_t cause = scause & ~RISCV_INTERRUPT_BIT;
    const trap_context_t* trap_context = runtime_context_active_trap_context();
    const trap_interrupt_handler_entry_t* entry = 0;
    const trap_exception_handler_entry_t* exception_entry = 0;

    if (trap_context == NULL) {
        panic_shutdown();
    }

    if ((scause & RISCV_INTERRUPT_BIT) == 0) {
        const uint64_t epc = riscv_read_sepc();
        const uint64_t tval = riscv_read_stval();

        if (cause >= TRAP_MAX_EXCEPTION_CAUSE) {
            panic_shutdown();
        }

        if (is_page_fault_cause(cause) &&
            vm_handle_page_fault(runtime_context_active_process(),
                                 runtime_context_active_address_space(),
                                 cause,
                                 epc,
                                 tval)) {
            return;
        }

        exception_entry = &trap_context->exception_handlers[cause];
        if (exception_entry->handler == 0) {
            panic_shutdown();
        }

        exception_entry->handler(cause, epc, tval, exception_entry->context);
        return;
    }

    if (cause >= TRAP_MAX_INTERRUPT_CAUSE) {
        panic_shutdown();
    }

    entry = &trap_context->interrupt_handlers[cause];
    if (entry->handler == 0) {
        panic_shutdown();
    }
    entry->handler(cause, entry->context);
}

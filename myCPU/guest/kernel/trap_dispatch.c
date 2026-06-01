#include "trap.h"

#include <stdint.h>

#include "course_syscall.h"
#include "linux_compat.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "runtime_context.h"
#include "timer.h"
#include "vm.h"

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

static bool user_syscall_policy_valid(const trap_context_t* trap_context,
                                      const trap_frame_t* frame,
                                      uint64_t cause,
                                      uint64_t tval) {
    const uint64_t sstatus = riscv_read_sstatus();

    return trap_context != NULL &&
           trap_context->user_ecall_policy.syscalls != NULL &&
           frame != NULL &&
           cause == RISCV_EXC_ECALL_FROM_U &&
           tval == 0 &&
           (sstatus & RISCV_SSTATUS_SPP) == 0 &&
           (sstatus & RISCV_SSTATUS_SPIE) != 0;
}

static bool user_linux_compat_policy_valid(const trap_context_t* trap_context,
                                           const trap_frame_t* frame,
                                           uint64_t cause,
                                           uint64_t tval) {
    const uint64_t sstatus = riscv_read_sstatus();

    return trap_context != NULL &&
           trap_context->user_ecall_policy.linux_runtime != NULL &&
           frame != NULL &&
           cause == RISCV_EXC_ECALL_FROM_U &&
           tval == 0 &&
           (sstatus & RISCV_SSTATUS_SPP) == 0 &&
           (sstatus & RISCV_SSTATUS_SPIE) != 0;
}

static bool user_exception_from_u_mode(void) {
    const uint64_t sstatus = riscv_read_sstatus();

    return (sstatus & RISCV_SSTATUS_SPP) == 0 &&
           (sstatus & RISCV_SSTATUS_SPIE) != 0;
}

static bool handle_user_syscall_policy(const trap_context_t* trap_context,
                                       trap_frame_t* frame,
                                       uint64_t cause,
                                       uint64_t epc,
                                       uint64_t tval) {
    int64_t result = 0;

    if (trap_context == NULL ||
        trap_context->user_ecall_policy.syscalls == NULL) {
        return false;
    }
    if (!user_syscall_policy_valid(trap_context, frame, cause, tval)) {
        panic_shutdown();
    }

    result = course_syscall_dispatch(trap_context->user_ecall_policy.syscalls,
                                     (uint32_t)frame->a7,
                                     frame->a0,
                                     frame->a1,
                                     frame->a2,
                                     frame->a3);
    frame->a0 = (uint64_t)result;
    riscv_set_sstatus_bits(RISCV_SSTATUS_SPP);
    riscv_write_sepc(epc + 4U);
    return true;
}

static void build_linux_compat_request(const trap_frame_t* frame,
                                       uint64_t epc,
                                       linux_compat_syscall_request_t* request) {
    if (request == NULL) {
        return;
    }
    request->number = frame != NULL ? frame->a7 : 0;
    request->dirfd = frame != NULL ? (int32_t)frame->a0 : 0;
    request->fd = frame != NULL ? (int32_t)frame->a0 : 0;
    request->path = frame != NULL ? (const char*)frame->a1 : 0;
    request->write_buffer = frame != NULL ? (const void*)frame->a1 : 0;
    request->read_buffer = frame != NULL ? (void*)frame->a1 : 0;
    request->length = frame != NULL ? (size_t)frame->a2 : 0;
    request->offset = frame != NULL ? frame->a1 : 0;
    request->stat = frame != NULL ? (linux_compat_stat_t*)frame->a2 : 0;
    request->dirents = frame != NULL ? (linux_compat_dirent_t*)frame->a1 : 0;
    request->dirent_capacity = frame != NULL ? (size_t)frame->a2 : 0;
    request->addr = frame != NULL ? frame->a0 : 0;
    request->prot = frame != NULL ? (uint32_t)frame->a2 : 0;
    request->flags = frame != NULL ? (uint32_t)frame->a3 : 0;

    if (frame == NULL) {
        return;
    }
    switch (frame->a7) {
    case LINUX_COMPAT_SYS_MMAP:
        request->addr = frame->a0;
        request->length = (size_t)frame->a1;
        request->prot = (uint32_t)frame->a2;
        request->flags = (uint32_t)frame->a3;
        request->fd = (int32_t)frame->a4;
        request->offset = frame->a5;
        break;
    case LINUX_COMPAT_SYS_FCNTL:
        request->fd = (int32_t)frame->a0;
        request->command = (uint32_t)frame->a1;
        request->arg = frame->a2;
        break;
    case LINUX_COMPAT_SYS_IOCTL:
        request->fd = (int32_t)frame->a0;
        request->command = (uint32_t)frame->a1;
        request->arg = frame->a2;
        break;
    case LINUX_COMPAT_SYS_GETRANDOM:
        request->read_buffer = (void*)frame->a0;
        request->length = (size_t)frame->a1;
        request->flags = (uint32_t)frame->a2;
        break;
    case LINUX_COMPAT_SYS_CLOCK_GETTIME:
        request->fd = (int32_t)frame->a0;
        request->read_buffer = (void*)frame->a1;
        break;
    case LINUX_COMPAT_SYS_MUNMAP:
        request->addr = frame->a0;
        request->length = (size_t)frame->a1;
        break;
    case LINUX_COMPAT_SYS_WRITE:
        request->fd = (int32_t)frame->a0;
        request->write_buffer = (const void*)frame->a1;
        request->length = (size_t)frame->a2;
        break;
    case LINUX_COMPAT_SYS_READ:
        request->fd = (int32_t)frame->a0;
        request->read_buffer = (void*)frame->a1;
        request->length = (size_t)frame->a2;
        break;
    case LINUX_COMPAT_SYS_OPENAT:
        request->dirfd = (int32_t)frame->a0;
        request->path = (const char*)frame->a1;
        request->flags = (uint32_t)frame->a2;
        break;
    case LINUX_COMPAT_SYS_NEWFSTATAT:
        request->dirfd = (int32_t)frame->a0;
        request->path = (const char*)frame->a1;
        request->stat = (linux_compat_stat_t*)frame->a2;
        break;
    case LINUX_COMPAT_SYS_GETDENTS64:
        request->fd = (int32_t)frame->a0;
        request->dirents = (linux_compat_dirent_t*)frame->a1;
        request->dirent_capacity = (size_t)frame->a2;
        break;
    case LINUX_COMPAT_SYS_LSEEK:
        request->fd = (int32_t)frame->a0;
        request->offset = frame->a1;
        break;
    case LINUX_COMPAT_SYS_EXIT:
    case LINUX_COMPAT_SYS_EXIT_GROUP:
        request->fd = (int32_t)frame->a0;
        break;
    default:
        break;
    }
    request->addr = request->addr != 0U ? request->addr : epc;
}

static bool handle_linux_compat_syscall_policy(const trap_context_t* trap_context,
                                               trap_frame_t* frame,
                                               uint64_t cause,
                                               uint64_t epc,
                                               uint64_t tval) {
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    linux_compat_runtime_t* runtime = NULL;
    uintptr_t next_pc = epc + 4U;

    if (trap_context == NULL ||
        trap_context->user_ecall_policy.linux_runtime == NULL) {
        return false;
    }
    if (!user_linux_compat_policy_valid(trap_context, frame, cause, tval)) {
        panic_shutdown();
    }

    runtime = trap_context->user_ecall_policy.linux_runtime;
    build_linux_compat_request(frame, epc, &request);
    response.value = 0;
    (void)linux_compat_syscall_dispatch(runtime, &request, &response, &trace);
    frame->a0 = (uint64_t)response.value;
    if (runtime->exited &&
        trap_context->user_ecall_policy.user_runtime != NULL &&
        trap_context->user_ecall_policy.user_runtime->resume_pc != 0U) {
        next_pc = trap_context->user_ecall_policy.user_runtime->resume_pc;
        riscv_set_sstatus_bits(RISCV_SSTATUS_SPP);
    } else {
        riscv_clear_sstatus_bits(RISCV_SSTATUS_SPP);
    }
    riscv_write_sepc(next_pc);
    return true;
}

static bool handle_user_crash_policy(const trap_context_t* trap_context,
                                     uint64_t cause,
                                     uint64_t epc,
                                     uint64_t tval) {
    if (trap_context == NULL ||
        trap_context->user_crash_policy.handler == NULL ||
        !user_exception_from_u_mode()) {
        return false;
    }

    if (!trap_context->user_crash_policy.handler(
            cause, epc, tval, trap_context->user_crash_policy.context)) {
        return false;
    }

    riscv_set_sstatus_bits(RISCV_SSTATUS_SPP);
    riscv_write_sepc(epc + 4U);
    return true;
}

static bool user_runtime_signal_delivery_ready(
    const trap_context_t* trap_context,
    const trap_user_runtime_t* user_runtime) {
    const vm_process_t* process = NULL;
    const vm_address_space_t* address_space = NULL;

    if (trap_context == NULL || !user_runtime_valid(user_runtime) ||
        trap_active_user_runtime() != user_runtime ||
        user_runtime->trap_context != trap_context ||
        !trap_context_is_active(trap_context) ||
        !vm_process_is_active(user_runtime->process)) {
        return false;
    }

    process = user_runtime->process;
    address_space = process->address_space;
    return address_space != NULL &&
           runtime_context_active_process() == process &&
           runtime_context_active_address_space() == address_space &&
           vm_address_space_is_active(address_space) &&
           vm_address_space_is_enabled(address_space);
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

    if (!user_runtime_signal_delivery_ready(trap_context, user_runtime) ||
        !user_runtime_timer_signal_valid(user_runtime)) {
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

    if (!user_runtime_signal_delivery_ready(trap_context, user_runtime) ||
        !user_runtime_external_signal_valid(user_runtime)) {
        return false;
    }

    user_runtime->external_signal.page[user_runtime->external_signal.word_index] =
        user_runtime->external_signal.value;
    user_runtime->external_signal.armed = false;
    user_runtime->external_signal.delivered = true;
    return true;
}

static void dispatch_installed_exception(const trap_context_t* trap_context,
                                         uint64_t cause,
                                         uint64_t epc,
                                         uint64_t tval) {
    const trap_exception_handler_entry_t* exception_entry = NULL;

    if (trap_context == NULL || cause >= TRAP_MAX_EXCEPTION_CAUSE) {
        panic_shutdown();
    }

    exception_entry = &trap_context->exception_handlers[cause];
    if (exception_entry->handler == 0) {
        panic_shutdown();
    }

    exception_entry->handler(cause, epc, tval, exception_entry->context);
}

static void dispatch_installed_interrupt(const trap_context_t* trap_context,
                                         uint64_t cause) {
    const trap_interrupt_handler_entry_t* entry = NULL;

    if (trap_context == NULL || cause >= TRAP_MAX_INTERRUPT_CAUSE) {
        panic_shutdown();
    }

    entry = &trap_context->interrupt_handlers[cause];
    if (entry->handler == 0) {
        panic_shutdown();
    }

    entry->handler(cause, entry->context);
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
    trap_context->user_ecall_policy.syscalls = NULL;
    trap_context->user_ecall_policy.linux_runtime = NULL;
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
    trap_context->user_ecall_policy.syscalls = NULL;
    trap_context->user_ecall_policy.linux_runtime = NULL;
    trap_context->user_ecall_policy.validate = validate;
    trap_context->user_ecall_policy.validate_context = validate_context;
    trap_context->user_ecall_policy.resume_pc = resume_pc;
    return trap_context_install_exception_handler(
        trap_context,
        RISCV_EXC_ECALL_FROM_U,
        default_user_ecall_resume_handler,
        trap_context);
}

bool trap_context_install_user_syscall_policy(trap_context_t* trap_context,
                                              course_syscall_t* syscalls) {
    if (trap_context == NULL || syscalls == NULL) {
        return false;
    }

    trap_context->user_ecall_policy.user_runtime = NULL;
    trap_context->user_ecall_policy.syscalls = syscalls;
    trap_context->user_ecall_policy.linux_runtime = NULL;
    trap_context->user_ecall_policy.validate = NULL;
    trap_context->user_ecall_policy.validate_context = NULL;
    trap_context->user_ecall_policy.resume_pc = 0;
    return true;
}

bool trap_context_install_linux_compat_syscall_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    linux_compat_runtime_t* runtime) {
    if (trap_context == NULL || runtime == NULL ||
        (user_runtime != NULL &&
         (!user_runtime_valid(user_runtime) ||
          user_runtime->trap_context != trap_context ||
          user_runtime->resume_pc == 0))) {
        return false;
    }

    trap_context->user_ecall_policy.user_runtime = user_runtime;
    trap_context->user_ecall_policy.syscalls = NULL;
    trap_context->user_ecall_policy.linux_runtime = runtime;
    trap_context->user_ecall_policy.validate = NULL;
    trap_context->user_ecall_policy.validate_context = NULL;
    trap_context->user_ecall_policy.resume_pc = 0;
    return true;
}

bool trap_context_install_user_crash_policy(trap_context_t* trap_context,
                                            trap_user_crash_handler_t handler,
                                            void* context) {
    if (trap_context == NULL || handler == NULL) {
        return false;
    }

    trap_context->user_crash_policy.handler = handler;
    trap_context->user_crash_policy.context = context;
    return true;
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

void supervisor_trap_dispatch_with_frame(trap_frame_t* frame) {
    const uint64_t scause = riscv_read_scause();
    const uint64_t cause = scause & ~RISCV_INTERRUPT_BIT;
    const trap_context_t* trap_context = runtime_context_active_trap_context();

    if (trap_context == NULL) {
        panic_shutdown();
    }

    if ((scause & RISCV_INTERRUPT_BIT) == 0) {
        const uint64_t epc = riscv_read_sepc();
        const uint64_t tval = riscv_read_stval();

        if (is_page_fault_cause(cause) &&
            vm_handle_page_fault(runtime_context_active_process(),
                                 runtime_context_active_address_space(),
                                 cause,
                                 epc,
                                 tval)) {
            return;
        }

        if (cause == RISCV_EXC_ECALL_FROM_U &&
            trap_context->user_ecall_policy.syscalls != NULL &&
            handle_user_syscall_policy(trap_context, frame, cause, epc, tval)) {
            return;
        }

        if (cause == RISCV_EXC_ECALL_FROM_U &&
            trap_context->user_ecall_policy.syscalls == NULL &&
            trap_context->user_ecall_policy.linux_runtime != NULL &&
            handle_linux_compat_syscall_policy(trap_context,
                                               frame,
                                               cause,
                                               epc,
                                               tval)) {
            return;
        }

        if (handle_user_crash_policy(trap_context, cause, epc, tval)) {
            return;
        }

        dispatch_installed_exception(trap_context, cause, epc, tval);
        return;
    }

    dispatch_installed_interrupt(trap_context, cause);
}

void supervisor_trap_dispatch(void) {
    supervisor_trap_dispatch_with_frame(NULL);
}

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
#include "../../guest/include/course_syscall.h"
#include "../../guest/include/linux_compat.h"
#include "../../guest/include/trap.h"

static jmp_buf g_panic_env;
static bool g_panic_armed = false;
static trap_context_t* g_active_trap_context = NULL;
static vm_process_t* g_active_process = NULL;
static vm_address_space_t* g_active_address_space = NULL;
static trap_user_runtime_t* g_active_user_runtime = NULL;
static uint64_t g_scause = 0;
static uint64_t g_sepc = 0;
static uint64_t g_stval = 0;
static uint64_t g_sstatus = 0;
static int g_vm_handle_page_fault_calls = 0;
static bool g_vm_handle_page_fault_result = false;
static uint64_t g_last_fault_cause = 0;
static uint64_t g_last_fault_epc = 0;
static uint64_t g_last_fault_tval = 0;
static int g_timer_handle_interrupt_calls = 0;
static uint32_t g_claim_source_id = 0;
static int g_complete_calls = 0;
static uint32_t g_last_completed_source_id = 0;
static int g_clear_sstatus_bits_calls = 0;
static uint64_t g_last_clear_sstatus_bits = 0;
static int g_set_sstatus_bits_calls = 0;
static uint64_t g_last_set_sstatus_bits = 0;
static int g_write_sepc_calls = 0;
static uint64_t g_last_written_sepc = 0;
static int g_interrupt_handler_calls = 0;
static uint64_t g_last_interrupt_cause = 0;
static void* g_last_interrupt_context = NULL;
static int g_exception_handler_calls = 0;
static uint64_t g_last_exception_cause = 0;
static uint64_t g_last_exception_epc = 0;
static uint64_t g_last_exception_tval = 0;
static void* g_last_exception_context = NULL;
static int g_timer_post_calls = 0;
static uint64_t g_last_timer_post_cause = 0;
static void* g_last_timer_post_context = NULL;
static int g_external_post_calls = 0;
static uint64_t g_last_external_post_cause = 0;
static uint32_t g_last_external_post_source_id = 0;
static void* g_last_external_post_context = NULL;
static int g_user_ecall_validate_calls = 0;
static uint64_t g_last_validate_epc = 0;
static uint64_t g_last_validate_tval = 0;
static void* g_last_validate_context = NULL;
static bool g_user_ecall_validate_result = true;
static int g_course_syscall_dispatch_calls = 0;
static course_syscall_t* g_last_course_syscall_context = NULL;
static uint32_t g_last_course_syscall_number = 0;
static uint64_t g_last_course_syscall_arg0 = 0;
static uint64_t g_last_course_syscall_arg1 = 0;
static uint64_t g_last_course_syscall_arg2 = 0;
static uint64_t g_last_course_syscall_arg3 = 0;
static int64_t g_course_syscall_dispatch_result = 0;
static int g_linux_compat_dispatch_calls = 0;
static linux_compat_runtime_t* g_last_linux_compat_runtime = NULL;
static linux_compat_syscall_request_t g_last_linux_compat_request;
static int64_t g_linux_compat_dispatch_value = 0;
static bool g_linux_compat_dispatch_sets_exited = false;
static int g_user_crash_handler_calls = 0;
static uint64_t g_last_user_crash_cause = 0;
static uint64_t g_last_user_crash_epc = 0;
static uint64_t g_last_user_crash_tval = 0;
static void* g_last_user_crash_context = NULL;
static bool g_user_crash_handler_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int dispatch_linux_compat_frame(trap_context_t* trap_context,
                                       trap_frame_t* trap_frame,
                                       uint64_t epc,
                                       const char* panic_message);
static int test_install_standard_user_runtime_policies(void);
static int test_dispatch_page_fault_and_custom_handlers(void);
static int test_dispatch_user_ecall_resume_policy(void);
static int test_dispatch_user_ecall_syscall_policy(void);
static int test_dispatch_linux_compat_ecall_policy(void);
static int test_dispatch_linux_compat_user_fault_exits_fail_closed(void);
static int test_dispatch_user_crash_policy(void);
static int test_default_timer_and_external_handlers(void);
static void stub_interrupt_handler(uint64_t cause, void* context);
static void stub_exception_handler(uint64_t cause,
                                   uint64_t epc,
                                   uint64_t tval,
                                   void* context);
static void stub_timer_post_handler(uint64_t cause, void* context);
static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context);
static bool stub_user_ecall_validate(uint64_t epc,
                                     uint64_t tval,
                                     void* context);
static bool stub_user_crash_handler(uint64_t cause,
                                    uint64_t epc,
                                    uint64_t tval,
                                    void* context);

int64_t course_syscall_dispatch(course_syscall_t* syscalls,
                                uint32_t number,
                                uint64_t arg0,
                                uint64_t arg1,
                                uint64_t arg2,
                                uint64_t arg3) {
    g_course_syscall_dispatch_calls += 1;
    g_last_course_syscall_context = syscalls;
    g_last_course_syscall_number = number;
    g_last_course_syscall_arg0 = arg0;
    g_last_course_syscall_arg1 = arg1;
    g_last_course_syscall_arg2 = arg2;
    g_last_course_syscall_arg3 = arg3;
    return g_course_syscall_dispatch_result;
}

linux_compat_result_t linux_compat_syscall_dispatch(
    linux_compat_runtime_t* runtime,
    const linux_compat_syscall_request_t* request,
    linux_compat_syscall_response_t* response,
    linux_compat_trace_t* out_trace) {
    (void)out_trace;
    g_linux_compat_dispatch_calls += 1;
    g_last_linux_compat_runtime = runtime;
    if (request != NULL) {
        g_last_linux_compat_request = *request;
    }
    if (g_linux_compat_dispatch_sets_exited && runtime != NULL) {
        runtime->exited = true;
    }
    if (response != NULL) {
        response->value = g_linux_compat_dispatch_value;
    }
    return LINUX_COMPAT_OK;
}

void linux_compat_runtime_record_user_fault(linux_compat_runtime_t* runtime,
                                            uint64_t cause,
                                            uintptr_t pc,
                                            uintptr_t tval) {
    linux_compat_syscall_trace_record_t* record = NULL;

    if (runtime == NULL) {
        return;
    }

    runtime->exited = true;
    runtime->exit_code = 128;
    runtime->user_faulted = true;
    runtime->user_fault_cause = cause;
    runtime->user_fault_pc = pc;
    runtime->user_fault_tval = tval;
    if (runtime->trace_count >= LINUX_COMPAT_MAX_TRACE_RECORDS) {
        runtime->trace_truncated = true;
        return;
    }

    record = &runtime->trace_records[runtime->trace_count++];
    record->number = LINUX_COMPAT_TRACE_USER_FAULT;
    record->return_value = -14;
    record->errno_value = 14;
    record->pc = pc;
    snprintf(record->message,
             sizeof(record->message),
             "linux-compat: user fault");
}

void panic_shutdown(void) {
    if (g_panic_armed) {
        longjmp(g_panic_env, 1);
    }

    longjmp(g_panic_env, 1);
}

trap_context_t* runtime_context_active_trap_context(void) {
    return g_active_trap_context;
}

bool trap_context_is_active(const trap_context_t* trap_context) {
    return trap_context != NULL && trap_context == g_active_trap_context;
}

vm_process_t* runtime_context_active_process(void) {
    return g_active_process;
}

bool vm_process_is_active(const vm_process_t* process) {
    return process != NULL && process == g_active_process;
}

vm_address_space_t* runtime_context_active_address_space(void) {
    return g_active_address_space;
}

bool vm_address_space_is_active(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space == g_active_address_space;
}

bool vm_address_space_is_enabled(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space == g_active_address_space;
}

bool vm_handle_page_fault(vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval) {
    (void)process;
    (void)address_space;
    g_vm_handle_page_fault_calls += 1;
    g_last_fault_cause = cause;
    g_last_fault_epc = epc;
    g_last_fault_tval = tval;
    return g_vm_handle_page_fault_result;
}

void timer_handle_interrupt(void) {
    g_timer_handle_interrupt_calls += 1;
}

uint32_t platform_plic_supervisor_claim(void) {
    return g_claim_source_id;
}

void platform_plic_supervisor_complete(uint32_t source_id) {
    g_complete_calls += 1;
    g_last_completed_source_id = source_id;
}

uint64_t riscv_read_scause(void) {
    return g_scause;
}

uint64_t riscv_read_sepc(void) {
    return g_sepc;
}

uint64_t riscv_read_stval(void) {
    return g_stval;
}

uint64_t riscv_read_sstatus(void) {
    return g_sstatus;
}

void riscv_clear_sstatus_bits(uint64_t value) {
    g_clear_sstatus_bits_calls += 1;
    g_last_clear_sstatus_bits = value;
}

void riscv_set_sstatus_bits(uint64_t value) {
    g_set_sstatus_bits_calls += 1;
    g_last_set_sstatus_bits = value;
}

void riscv_write_sepc(uint64_t value) {
    g_write_sepc_calls += 1;
    g_last_written_sepc = value;
}

trap_user_runtime_t* trap_active_user_runtime(void) {
    return g_active_user_runtime;
}

static void reset_stub_state(void) {
    g_panic_armed = false;
    g_active_trap_context = NULL;
    g_active_process = NULL;
    g_active_address_space = NULL;
    g_active_user_runtime = NULL;
    g_scause = 0;
    g_sepc = 0;
    g_stval = 0;
    g_sstatus = 0;
    g_vm_handle_page_fault_calls = 0;
    g_vm_handle_page_fault_result = false;
    g_last_fault_cause = 0;
    g_last_fault_epc = 0;
    g_last_fault_tval = 0;
    g_timer_handle_interrupt_calls = 0;
    g_claim_source_id = 0;
    g_complete_calls = 0;
    g_last_completed_source_id = 0;
    g_clear_sstatus_bits_calls = 0;
    g_last_clear_sstatus_bits = 0;
    g_set_sstatus_bits_calls = 0;
    g_last_set_sstatus_bits = 0;
    g_write_sepc_calls = 0;
    g_last_written_sepc = 0;
    g_interrupt_handler_calls = 0;
    g_last_interrupt_cause = 0;
    g_last_interrupt_context = NULL;
    g_exception_handler_calls = 0;
    g_last_exception_cause = 0;
    g_last_exception_epc = 0;
    g_last_exception_tval = 0;
    g_last_exception_context = NULL;
    g_timer_post_calls = 0;
    g_last_timer_post_cause = 0;
    g_last_timer_post_context = NULL;
    g_external_post_calls = 0;
    g_last_external_post_cause = 0;
    g_last_external_post_source_id = 0;
    g_last_external_post_context = NULL;
    g_user_ecall_validate_calls = 0;
    g_last_validate_epc = 0;
    g_last_validate_tval = 0;
    g_last_validate_context = NULL;
    g_user_ecall_validate_result = true;
    g_course_syscall_dispatch_calls = 0;
    g_last_course_syscall_context = NULL;
    g_last_course_syscall_number = 0;
    g_last_course_syscall_arg0 = 0;
    g_last_course_syscall_arg1 = 0;
    g_last_course_syscall_arg2 = 0;
    g_last_course_syscall_arg3 = 0;
    g_course_syscall_dispatch_result = 0;
    g_linux_compat_dispatch_calls = 0;
    g_last_linux_compat_runtime = NULL;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    g_linux_compat_dispatch_value = 0;
    g_linux_compat_dispatch_sets_exited = false;
    g_user_crash_handler_calls = 0;
    g_last_user_crash_cause = 0;
    g_last_user_crash_epc = 0;
    g_last_user_crash_tval = 0;
    g_last_user_crash_context = NULL;
    g_user_crash_handler_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int dispatch_linux_compat_frame(trap_context_t* trap_context,
                                       trap_frame_t* trap_frame,
                                       uint64_t epc,
                                       const char* panic_message) {
    g_active_trap_context = trap_context;
    g_scause = RISCV_EXC_ECALL_FROM_U;
    g_sepc = epc;
    g_stval = 0;
    g_sstatus = RISCV_SSTATUS_SPIE;
    if (setjmp(g_panic_env) != 0) {
        return fail(panic_message);
    }
    g_panic_armed = true;
    supervisor_trap_dispatch_with_frame(trap_frame);
    g_panic_armed = false;
    return 0;
}

static void stub_interrupt_handler(uint64_t cause, void* context) {
    g_interrupt_handler_calls += 1;
    g_last_interrupt_cause = cause;
    g_last_interrupt_context = context;
}

static void stub_exception_handler(uint64_t cause,
                                   uint64_t epc,
                                   uint64_t tval,
                                   void* context) {
    g_exception_handler_calls += 1;
    g_last_exception_cause = cause;
    g_last_exception_epc = epc;
    g_last_exception_tval = tval;
    g_last_exception_context = context;
}

static void stub_timer_post_handler(uint64_t cause, void* context) {
    g_timer_post_calls += 1;
    g_last_timer_post_cause = cause;
    g_last_timer_post_context = context;
}

static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context) {
    g_external_post_calls += 1;
    g_last_external_post_cause = cause;
    g_last_external_post_source_id = source_id;
    g_last_external_post_context = context;
}

static bool stub_user_ecall_validate(uint64_t epc,
                                     uint64_t tval,
                                     void* context) {
    g_user_ecall_validate_calls += 1;
    g_last_validate_epc = epc;
    g_last_validate_tval = tval;
    g_last_validate_context = context;
    return g_user_ecall_validate_result;
}

static bool stub_user_crash_handler(uint64_t cause,
                                    uint64_t epc,
                                    uint64_t tval,
                                    void* context) {
    g_user_crash_handler_calls += 1;
    g_last_user_crash_cause = cause;
    g_last_user_crash_epc = epc;
    g_last_user_crash_tval = tval;
    g_last_user_crash_context = context;
    return g_user_crash_handler_result;
}

static int test_install_standard_user_runtime_policies(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    trap_user_runtime_t user_runtime = {
        .trap_context = &trap_context,
        .process = &process,
        .expected_ecall_pc = 0x4000,
        .resume_pc = 0x5000,
    };

    reset_stub_state();
    if (!trap_context_install_standard_user_runtime_policies(&trap_context,
                                                             &user_runtime,
                                                             stub_timer_post_handler,
                                                             &process,
                                                             stub_external_post_handler,
                                                             &address_space)) {
        return fail("expected standard runtime policies install to succeed");
    }

    if (trap_context.supervisor_timer_policy.user_runtime != &user_runtime ||
        trap_context.supervisor_external_policy.user_runtime != &user_runtime ||
        trap_context.interrupt_handlers[RISCV_SUPERVISOR_TIMER_INTERRUPT].handler == NULL ||
        trap_context.interrupt_handlers[RISCV_SUPERVISOR_EXTERNAL_INTERRUPT].handler == NULL ||
        trap_context.user_ecall_policy.user_runtime != &user_runtime ||
        trap_context.exception_handlers[RISCV_EXC_ECALL_FROM_U].handler == NULL) {
        return fail("expected standard policy install to bind runtime and handlers");
    }

    return 0;
}

static int test_dispatch_page_fault_and_custom_handlers(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    int context_cookie = 7;

    reset_stub_state();
    g_active_trap_context = &trap_context;
    g_active_process = &process;
    g_active_address_space = &address_space;
    g_scause = RISCV_EXC_LOAD_PAGE_FAULT;
    g_sepc = 0x1200;
    g_stval = 0x88;
    g_vm_handle_page_fault_result = true;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during page-fault dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (g_vm_handle_page_fault_calls != 1 || g_last_fault_cause != RISCV_EXC_LOAD_PAGE_FAULT ||
        g_last_fault_epc != 0x1200 || g_last_fault_tval != 0x88) {
        return fail("expected page-fault dispatch to forward into vm_handle_page_fault");
    }

    reset_stub_state();
    g_active_trap_context = &trap_context;
    if (!trap_context_install_interrupt_handler(&trap_context,
                                                3,
                                                stub_interrupt_handler,
                                                &context_cookie)) {
        return fail("expected custom interrupt handler install to succeed");
    }
    g_scause = RISCV_INTERRUPT_BIT | 3U;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during custom interrupt dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (g_interrupt_handler_calls != 1 || g_last_interrupt_cause != 3 ||
        g_last_interrupt_context != &context_cookie) {
        return fail("expected custom interrupt dispatch to call installed handler");
    }

    reset_stub_state();
    g_active_trap_context = &trap_context;
    if (!trap_context_install_exception_handler(&trap_context,
                                                4,
                                                stub_exception_handler,
                                                &context_cookie)) {
        return fail("expected custom exception handler install to succeed");
    }
    g_scause = 4U;
    g_sepc = 0x3333;
    g_stval = 0x4444;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during custom exception dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (g_exception_handler_calls != 1 || g_last_exception_cause != 4 ||
        g_last_exception_epc != 0x3333 || g_last_exception_tval != 0x4444 ||
        g_last_exception_context != &context_cookie) {
        return fail("expected custom exception dispatch to call installed handler");
    }

    return 0;
}

static int test_dispatch_user_ecall_resume_policy(void) {
    trap_context_t trap_context = {0};
    int validate_cookie = 9;

    reset_stub_state();
    if (!trap_context_install_user_ecall_resume_policy(&trap_context,
                                                       0x9000,
                                                       stub_user_ecall_validate,
                                                       &validate_cookie)) {
        return fail("expected user ecall resume policy install to succeed");
    }

    g_active_trap_context = &trap_context;
    g_scause = RISCV_EXC_ECALL_FROM_U;
    g_sepc = 0x7000;
    g_stval = 0;
    g_sstatus = RISCV_SSTATUS_SPIE;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during user ecall resume dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;

    if (g_user_ecall_validate_calls != 1 || g_last_validate_epc != 0x7000 ||
        g_last_validate_tval != 0 || g_last_validate_context != &validate_cookie ||
        g_set_sstatus_bits_calls != 1 || g_last_set_sstatus_bits != RISCV_SSTATUS_SPP ||
        g_write_sepc_calls != 1 || g_last_written_sepc != 0x9000) {
        return fail("expected ecall resume dispatch to validate and redirect sepc");
    }

    return 0;
}

static int test_dispatch_user_ecall_syscall_policy(void) {
    trap_context_t trap_context = {0};
    course_syscall_t syscalls = {0};
    trap_frame_t trap_frame = {0};

    reset_stub_state();
    if (!trap_context_install_user_syscall_policy(&trap_context, &syscalls)) {
        return fail("expected user syscall policy install to succeed");
    }

    trap_frame.a0 = 1U;
    trap_frame.a1 = 0x2000U;
    trap_frame.a2 = 5U;
    trap_frame.a3 = 9U;
    trap_frame.a7 = COURSE_SYSCALL_WRITE;
    g_course_syscall_dispatch_result = 5;
    g_active_trap_context = &trap_context;
    g_scause = RISCV_EXC_ECALL_FROM_U;
    g_sepc = 0x7000;
    g_stval = 0;
    g_sstatus = RISCV_SSTATUS_SPIE;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during user syscall dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch_with_frame(&trap_frame);
    g_panic_armed = false;

    if (g_course_syscall_dispatch_calls != 1 ||
        g_last_course_syscall_context != &syscalls ||
        g_last_course_syscall_number != COURSE_SYSCALL_WRITE ||
        g_last_course_syscall_arg0 != 1U ||
        g_last_course_syscall_arg1 != 0x2000U ||
        g_last_course_syscall_arg2 != 5U ||
        g_last_course_syscall_arg3 != 9U ||
        trap_frame.a0 != 5U ||
        g_write_sepc_calls != 1 ||
        g_last_written_sepc != 0x7004U) {
        return fail("expected user ecall to dispatch syscall and resume next pc");
    }

    return 0;
}

static int test_dispatch_linux_compat_ecall_policy(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    trap_user_runtime_t user_runtime = {
        .trap_context = &trap_context,
        .process = &process,
        .resume_pc = 0x9000,
    };
    linux_compat_runtime_t runtime = {0};
    trap_frame_t trap_frame = {0};

    reset_stub_state();
    if (!trap_context_install_linux_compat_syscall_policy(&trap_context,
                                                          &user_runtime,
                                                          &runtime)) {
        return fail("expected linux compat syscall policy install to succeed");
    }

    trap_frame.a0 = 1U;
    trap_frame.a1 = 0x2000U;
    trap_frame.a2 = 5U;
    trap_frame.a7 = LINUX_COMPAT_SYS_WRITE;
    g_linux_compat_dispatch_value = 5;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7000,
            "did not expect panic during linux compat write ecall dispatch") !=
        0) {
        return 1;
    }

    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_runtime != &runtime ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_WRITE ||
        g_last_linux_compat_request.fd != 1 ||
        g_last_linux_compat_request.write_buffer != (const void*)0x2000U ||
        g_last_linux_compat_request.length != 5U ||
        trap_frame.a0 != 5U ||
        g_clear_sstatus_bits_calls != 1 ||
        g_last_clear_sstatus_bits != RISCV_SSTATUS_SPP ||
        g_set_sstatus_bits_calls != 0 ||
        g_write_sepc_calls != 1 ||
        g_last_written_sepc != 0x7004U) {
        return fail("expected linux compat write ecall to return to U-mode next pc");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 4U;
    trap_frame.a1 = 0x2100U;
    trap_frame.a2 = 12U;
    trap_frame.a3 = 0x33U;
    trap_frame.a7 = LINUX_COMPAT_SYS_PWRITE64;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7020,
            "did not expect panic during linux compat pwrite64 ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_PWRITE64 ||
        g_last_linux_compat_request.fd != 4 ||
        g_last_linux_compat_request.write_buffer != (const void*)0x2100U ||
        g_last_linux_compat_request.length != 12U ||
        g_last_linux_compat_request.offset != 0x33U) {
        return fail("expected linux compat pwrite64 ecall to preserve offset argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 5U;
    trap_frame.a1 = (uint64_t)-7;
    trap_frame.a2 = 2U;
    trap_frame.a3 = 0;
    trap_frame.a7 = LINUX_COMPAT_SYS_LSEEK;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7028,
            "did not expect panic during linux compat lseek ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_LSEEK ||
        g_last_linux_compat_request.fd != 5 ||
        g_last_linux_compat_request.offset != (uint64_t)-7 ||
        g_last_linux_compat_request.command != 2U) {
        return fail("expected linux compat lseek ecall to preserve offset and whence arguments");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 4U;
    trap_frame.a1 = 64U;
    trap_frame.a2 = 0;
    trap_frame.a3 = 0;
    trap_frame.a7 = LINUX_COMPAT_SYS_FTRUNCATE;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7030,
            "did not expect panic during linux compat ftruncate ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_FTRUNCATE ||
        g_last_linux_compat_request.fd != 4 ||
        g_last_linux_compat_request.length != 64U) {
        return fail("expected linux compat ftruncate ecall to preserve length argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 0x20000000U;
    trap_frame.a1 = 0x1000U;
    trap_frame.a2 = 0x2000U;
    trap_frame.a3 = 1U;
    trap_frame.a4 = 0x21000000U;
    trap_frame.a7 = 216U;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7038,
            "did not expect panic during linux compat mremap ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != 216U ||
        g_last_linux_compat_request.addr != 0x20000000U ||
        g_last_linux_compat_request.length != 0x1000U ||
        g_last_linux_compat_request.offset != 0x2000U ||
        g_last_linux_compat_request.flags != 1U ||
        g_last_linux_compat_request.arg != 0x21000000U) {
        return fail("expected linux compat mremap ecall to preserve old and new mapping args");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 5U;
    trap_frame.a1 = 0x2200U;
    trap_frame.a2 = 0;
    trap_frame.a3 = 0;
    trap_frame.a7 = LINUX_COMPAT_SYS_FSTAT;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7040,
            "did not expect panic during linux compat fstat ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_FSTAT ||
        g_last_linux_compat_request.fd != 5 ||
        g_last_linux_compat_request.stat != (linux_compat_stat_t*)0x2200U) {
        return fail("expected linux compat fstat ecall to preserve stat buffer argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 0x2210U;
    trap_frame.a1 = 64U;
    trap_frame.a2 = 0;
    trap_frame.a3 = 0;
    trap_frame.a7 = 17U;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7048,
            "did not expect panic during linux compat getcwd ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != 17U ||
        g_last_linux_compat_request.read_buffer != (void*)0x2210U ||
        g_last_linux_compat_request.length != 64U) {
        return fail("expected linux compat getcwd ecall to preserve buffer and length");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = 0x2220U;
    trap_frame.a1 = 0;
    trap_frame.a2 = 0;
    trap_frame.a3 = 0;
    trap_frame.a7 = 49U;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x704c,
            "did not expect panic during linux compat chdir ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != 49U ||
        g_last_linux_compat_request.path != (const char*)0x2220U) {
        return fail("expected linux compat chdir ecall to preserve path argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = LINUX_COMPAT_AT_FDCWD;
    trap_frame.a1 = 0x2230U;
    trap_frame.a2 = 0644U;
    trap_frame.a3 = 0;
    trap_frame.a7 = 53U;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x704e,
            "did not expect panic during linux compat fchmodat ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != 53U ||
        g_last_linux_compat_request.dirfd != LINUX_COMPAT_AT_FDCWD ||
        g_last_linux_compat_request.path != (const char*)0x2230U ||
        g_last_linux_compat_request.flags != 0644U) {
        return fail("expected linux compat fchmodat ecall to preserve path and mode");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = LINUX_COMPAT_AT_FDCWD;
    trap_frame.a1 = 0x2300U;
    trap_frame.a2 = 0755U;
    trap_frame.a3 = 0;
    trap_frame.a7 = LINUX_COMPAT_SYS_MKDIRAT;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7050,
            "did not expect panic during linux compat mkdirat ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_MKDIRAT ||
        g_last_linux_compat_request.dirfd != LINUX_COMPAT_AT_FDCWD ||
        g_last_linux_compat_request.path != (const char*)0x2300U ||
        g_last_linux_compat_request.flags != 0755U) {
        return fail("expected linux compat mkdirat ecall to preserve mode argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    trap_frame.a0 = LINUX_COMPAT_AT_FDCWD;
    trap_frame.a1 = 0x2400U;
    trap_frame.a2 = LINUX_COMPAT_AT_FDCWD;
    trap_frame.a3 = 0x2500U;
    trap_frame.a7 = LINUX_COMPAT_SYS_RENAMEAT;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7060,
            "did not expect panic during linux compat renameat ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_RENAMEAT ||
        g_last_linux_compat_request.dirfd != LINUX_COMPAT_AT_FDCWD ||
        g_last_linux_compat_request.path != (const char*)0x2400U ||
        g_last_linux_compat_request.new_path != (const char*)0x2500U) {
        return fail("expected linux compat renameat ecall to preserve new path argument");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 0x120011U;
    trap_frame.a1 = 0x3000U;
    trap_frame.a7 = LINUX_COMPAT_SYS_CLONE;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7070,
            "did not expect panic during linux compat clone ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_CLONE ||
        g_last_linux_compat_request.flags != 0x120011U ||
        g_last_linux_compat_request.addr != 0x3000U) {
        return fail("expected linux compat clone ecall to preserve flags and child stack");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 0x3400U;
    trap_frame.a1 = 0x3500U;
    trap_frame.a2 = 0x3600U;
    trap_frame.a7 = LINUX_COMPAT_SYS_EXECVE;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7080,
            "did not expect panic during linux compat execve ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_EXECVE ||
        g_last_linux_compat_request.path != (const char*)0x3400U ||
        g_last_linux_compat_request.write_buffer != (const void*)0x3500U ||
        g_last_linux_compat_request.read_buffer != (void*)0x3600U) {
        return fail("expected linux compat execve ecall to preserve path argv and envp");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 42U;
    trap_frame.a1 = 0x3700U;
    trap_frame.a2 = 0x2U;
    trap_frame.a3 = 0x3800U;
    trap_frame.a7 = LINUX_COMPAT_SYS_WAIT4;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x7090,
            "did not expect panic during linux compat wait4 ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_WAIT4 ||
        g_last_linux_compat_request.fd != 42 ||
        g_last_linux_compat_request.read_buffer != (void*)0x3700U ||
        g_last_linux_compat_request.flags != 0x2U ||
        g_last_linux_compat_request.write_buffer != (const void*)0x3800U) {
        return fail("expected linux compat wait4 ecall to preserve pid status flags and rusage");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 0x3900U;
    trap_frame.a1 = 0x800U;
    trap_frame.a7 = LINUX_COMPAT_SYS_PIPE2;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x70a0,
            "did not expect panic during linux compat pipe2 ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_PIPE2 ||
        g_last_linux_compat_request.read_buffer != (void*)0x3900U ||
        g_last_linux_compat_request.flags != 0x800U) {
        return fail("expected linux compat pipe2 ecall to preserve pipefd and flags");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 3U;
    trap_frame.a1 = 6U;
    trap_frame.a2 = 0x80000U;
    trap_frame.a7 = LINUX_COMPAT_SYS_DUP3;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x70b0,
            "did not expect panic during linux compat dup3 ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_DUP3 ||
        g_last_linux_compat_request.fd != 3 ||
        g_last_linux_compat_request.command != 6U ||
        g_last_linux_compat_request.flags != 0x80000U) {
        return fail("expected linux compat dup3 ecall to preserve oldfd newfd and flags");
    }

    g_linux_compat_dispatch_calls = 0;
    memset(&g_last_linux_compat_request, 0, sizeof(g_last_linux_compat_request));
    memset(&trap_frame, 0, sizeof(trap_frame));
    trap_frame.a0 = 0U;
    trap_frame.a7 = LINUX_COMPAT_SYS_BRK;
    if (dispatch_linux_compat_frame(
            &trap_context,
            &trap_frame,
            0x70b8,
            "did not expect panic during linux compat brk(0) ecall dispatch") !=
        0) {
        return 1;
    }
    if (g_linux_compat_dispatch_calls != 1 ||
        g_last_linux_compat_request.number != LINUX_COMPAT_SYS_BRK ||
        g_last_linux_compat_request.addr != 0U ||
        g_last_linux_compat_request.pc != 0x70b8U) {
        return fail("expected linux compat brk(0) ecall to preserve zero address and separate pc");
    }

    g_clear_sstatus_bits_calls = 0;
    g_last_clear_sstatus_bits = 0;
    g_set_sstatus_bits_calls = 0;
    g_last_set_sstatus_bits = 0;
    g_write_sepc_calls = 0;
    g_last_written_sepc = 0;
    trap_frame.a0 = 7U;
    trap_frame.a7 = LINUX_COMPAT_SYS_EXIT_GROUP;
    g_linux_compat_dispatch_sets_exited = true;
    g_active_trap_context = &trap_context;
    g_scause = RISCV_EXC_ECALL_FROM_U;
    g_sepc = 0x7010;
    g_stval = 0;
    g_sstatus = RISCV_SSTATUS_SPIE;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during linux compat exit ecall dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch_with_frame(&trap_frame);
    g_panic_armed = false;
    if (g_last_linux_compat_request.number != LINUX_COMPAT_SYS_EXIT_GROUP ||
        g_last_linux_compat_request.fd != 7 ||
        g_clear_sstatus_bits_calls != 0 ||
        g_set_sstatus_bits_calls != 1 ||
        g_last_set_sstatus_bits != RISCV_SSTATUS_SPP ||
        g_write_sepc_calls != 1 ||
        g_last_written_sepc != user_runtime.resume_pc) {
        return fail("expected linux compat exit ecall to return to runtime resume pc");
    }

    return 0;
}

static int test_dispatch_linux_compat_user_fault_exits_fail_closed(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    trap_user_runtime_t user_runtime = {
        .trap_context = &trap_context,
        .process = &process,
        .resume_pc = 0x9000,
    };
    linux_compat_runtime_t runtime = {0};

    reset_stub_state();
    if (!trap_context_install_linux_compat_syscall_policy(&trap_context,
                                                          &user_runtime,
                                                          &runtime)) {
        return fail("expected linux compat syscall policy install to succeed");
    }

    g_active_trap_context = &trap_context;
    g_active_process = &process;
    g_active_address_space = &address_space;
    g_scause = RISCV_EXC_LOAD_PAGE_FAULT;
    g_sepc = 0x400123U;
    g_stval = 0x40U;
    g_sstatus = RISCV_SSTATUS_SPIE;
    g_vm_handle_page_fault_result = false;
    if (setjmp(g_panic_env) != 0) {
        return fail("expected linux compat user fault to exit fail-closed instead of panicking");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;

    if (g_vm_handle_page_fault_calls != 1 ||
        !runtime.exited ||
        runtime.exit_code == 0 ||
        !runtime.user_faulted ||
        runtime.user_fault_cause != RISCV_EXC_LOAD_PAGE_FAULT ||
        runtime.user_fault_pc != 0x400123U ||
        runtime.user_fault_tval != 0x40U ||
        runtime.trace_count != 1 ||
        runtime.trace_records[0].number != LINUX_COMPAT_TRACE_USER_FAULT ||
        runtime.trace_records[0].errno_value != 14 ||
        runtime.trace_records[0].pc != 0x400123U ||
        g_set_sstatus_bits_calls != 1 ||
        g_last_set_sstatus_bits != RISCV_SSTATUS_SPP ||
        g_write_sepc_calls != 1 ||
        g_last_written_sepc != user_runtime.resume_pc) {
        return fail("expected linux compat user fault to mark runtime exited and resume supervisor");
    }

    return 0;
}

static int test_dispatch_user_crash_policy(void) {
    trap_context_t trap_context = {0};
    int crash_cookie = 11;

    reset_stub_state();
    if (!trap_context_install_user_crash_policy(&trap_context,
                                                stub_user_crash_handler,
                                                &crash_cookie)) {
        return fail("expected user crash policy install to succeed");
    }

    g_active_trap_context = &trap_context;
    g_scause = RISCV_EXC_ILLEGAL_INSTRUCTION;
    g_sepc = 0x8000;
    g_stval = 0x1234;
    g_sstatus = RISCV_SSTATUS_SPIE;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during user crash policy dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;

    if (g_user_crash_handler_calls != 1 ||
        g_last_user_crash_cause != RISCV_EXC_ILLEGAL_INSTRUCTION ||
        g_last_user_crash_epc != 0x8000 ||
        g_last_user_crash_tval != 0x1234 ||
        g_last_user_crash_context != &crash_cookie ||
        g_set_sstatus_bits_calls != 1 ||
        g_last_set_sstatus_bits != RISCV_SSTATUS_SPP ||
        g_write_sepc_calls != 1 ||
        g_last_written_sepc != 0x8004U) {
        return fail("expected user crash policy to record fatal user exception");
    }

    reset_stub_state();
    if (!trap_context_install_user_crash_policy(&trap_context,
                                                stub_user_crash_handler,
                                                &crash_cookie)) {
        return fail("expected user crash policy reinstall to succeed");
    }
    g_active_trap_context = &trap_context;
    g_active_process = (vm_process_t*)&trap_context;
    g_active_address_space = (vm_address_space_t*)&trap_context;
    g_scause = RISCV_EXC_STORE_PAGE_FAULT;
    g_sepc = 0x9000;
    g_stval = 0x2222;
    g_sstatus = RISCV_SSTATUS_SPIE;
    g_vm_handle_page_fault_result = false;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during unhandled user page fault policy");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (g_vm_handle_page_fault_calls != 1 ||
        g_user_crash_handler_calls != 1 ||
        g_last_user_crash_cause != RISCV_EXC_STORE_PAGE_FAULT ||
        g_last_user_crash_tval != 0x2222) {
        return fail("expected unhandled user page fault to become crash event");
    }

    return 0;
}

static int test_default_timer_and_external_handlers(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    uint32_t signal_page[2] = {0};
    trap_user_runtime_t user_runtime = {
        .trap_context = &trap_context,
        .process = &process,
        .expected_ecall_pc = 0x4000,
        .resume_pc = 0x5000,
        .timer_signal = {
            .page = signal_page,
            .word_index = 0,
            .value = 1,
            .armed = true,
        },
        .external_signal = {
            .page = signal_page,
            .word_index = 1,
            .value = 2,
            .armed = true,
        },
    };

    reset_stub_state();
    if (!trap_context_install_standard_user_runtime_policies(&trap_context,
                                                             &user_runtime,
                                                             stub_timer_post_handler,
                                                             &process,
                                                             stub_external_post_handler,
                                                             &address_space)) {
        return fail("expected standard policies before default dispatch");
    }

    g_active_trap_context = &trap_context;
    g_active_user_runtime = &user_runtime;
    g_active_process = &process;
    g_active_address_space = &address_space;
    g_scause = RISCV_INTERRUPT_BIT | RISCV_SUPERVISOR_TIMER_INTERRUPT;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during default timer dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (g_timer_handle_interrupt_calls != 1 || signal_page[0] != 1 ||
        !user_runtime.timer_signal.delivered || g_timer_post_calls != 1 ||
        g_last_timer_post_cause != RISCV_SUPERVISOR_TIMER_INTERRUPT ||
        g_last_timer_post_context != &process) {
        return fail("expected default timer handler to deliver signal and call post handler");
    }

    g_claim_source_id = 5;
    g_active_trap_context = &trap_context;
    g_active_user_runtime = &user_runtime;
    g_active_process = &process;
    g_active_address_space = &address_space;
    g_scause = RISCV_INTERRUPT_BIT | RISCV_SUPERVISOR_EXTERNAL_INTERRUPT;
    if (setjmp(g_panic_env) != 0) {
        return fail("did not expect panic during default external dispatch");
    }
    g_panic_armed = true;
    supervisor_trap_dispatch();
    g_panic_armed = false;
    if (signal_page[1] != 2 || !user_runtime.external_signal.delivered ||
        g_external_post_calls != 1 ||
        g_last_external_post_cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT ||
        g_last_external_post_source_id != 5 ||
        g_last_external_post_context != &address_space ||
        g_complete_calls != 1 || g_last_completed_source_id != 5) {
        return fail("expected default external handler to deliver signal and complete source");
    }

    return 0;
}

static int test_signal_delivery_rejects_inactive_runtime(void) {
    trap_context_t trap_context = {0};
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_process_t process = {
        .address_space = &address_space,
    };
    trap_user_runtime_t user_runtime = {
        .trap_context = &trap_context,
        .process = &process,
        .expected_ecall_pc = 0x4000,
        .resume_pc = 0x5000,
    };
    uint32_t timer_page[MEMORY_PAGE_SIZE / sizeof(uint32_t)] = {0};

    reset_stub_state();
    if (!trap_context_install_standard_user_runtime_policies(&trap_context,
                                                             &user_runtime,
                                                             NULL,
                                                             NULL,
                                                             NULL,
                                                             NULL)) {
        return fail("expected standard runtime policy install before inactive delivery guard");
    }
    user_runtime.timer_signal.page = timer_page;
    user_runtime.timer_signal.word_index = 0;
    user_runtime.timer_signal.value = 0x55U;
    user_runtime.timer_signal.armed = true;
    g_active_trap_context = &trap_context;
    g_scause = RISCV_INTERRUPT_BIT | RISCV_SUPERVISOR_TIMER_INTERRUPT;
    if (setjmp(g_panic_env) == 0) {
        g_panic_armed = true;
        supervisor_trap_dispatch();
        g_panic_armed = false;
        return fail("expected inactive runtime signal delivery to panic");
    }
    g_panic_armed = false;

    if (timer_page[0] != 0U) {
        return fail("expected inactive runtime signal delivery to avoid touching stale page");
    }

    return 0;
}

int main(void) {
    if (test_install_standard_user_runtime_policies() != 0 ||
        test_dispatch_page_fault_and_custom_handlers() != 0 ||
        test_dispatch_user_ecall_resume_policy() != 0 ||
        test_dispatch_user_ecall_syscall_policy() != 0 ||
        test_dispatch_linux_compat_ecall_policy() != 0 ||
        test_dispatch_linux_compat_user_fault_exits_fail_closed() != 0 ||
        test_dispatch_user_crash_policy() != 0 ||
        test_default_timer_and_external_handlers() != 0 ||
        test_signal_delivery_rejects_inactive_runtime() != 0) {
        return 1;
    }

    return 0;
}

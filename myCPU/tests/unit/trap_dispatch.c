#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
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

static void reset_stub_state(void);
static int fail(const char* message);
static int test_install_standard_user_runtime_policies(void);
static int test_dispatch_page_fault_and_custom_handlers(void);
static int test_dispatch_user_ecall_resume_policy(void);
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
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
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
        test_default_timer_and_external_handlers() != 0 ||
        test_signal_delivery_rejects_inactive_runtime() != 0) {
        return 1;
    }

    return 0;
}

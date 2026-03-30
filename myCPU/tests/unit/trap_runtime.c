#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
#include "../../guest/include/trap.h"

static trap_context_t* g_active_trap_context = NULL;
static vm_process_t* g_active_process = NULL;
static int g_vm_process_set_user_context_calls = 0;
static uintptr_t g_last_entry_pc = 0;
static uintptr_t g_last_user_sp = 0;
static bool g_vm_process_set_user_context_result = true;
static int g_standard_policy_calls = 0;
static trap_context_t* g_last_policy_trap_context = NULL;
static trap_user_runtime_t* g_last_policy_user_runtime = NULL;
static bool g_standard_policy_result = true;
static bool g_vm_process_is_runnable_result = true;
static int g_vm_process_activate_calls = 0;
static bool g_vm_process_activate_result = true;
static int g_vm_address_space_disable_calls = 0;
static vm_address_space_t* g_last_disabled_address_space = NULL;
static bool g_vm_address_space_disable_result = true;
static uint64_t g_last_cleared_sstatus_bits = 0;
static int g_arch_enter_calls = 0;
static trap_user_runtime_t* g_last_arch_enter_runtime = NULL;
static uintptr_t g_last_arch_enter_entry = 0;
static uintptr_t g_last_arch_enter_arg0 = 0;
static uintptr_t g_last_arch_enter_user_sp = 0;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_trap_context_init_and_activate(void);
static int test_user_runtime_prepare_standard(void);
static int test_user_runtime_activate_enter_and_deactivate(void);
static bool stub_runtime_validate(const trap_user_runtime_t* user_runtime,
                                  uint64_t epc,
                                  uint64_t tval,
                                  void* context);
static void stub_timer_post_handler(uint64_t cause, void* context);
static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context);

void trap_user_runtime_arch_enter(trap_user_runtime_t* user_runtime,
                                  uintptr_t entry,
                                  uintptr_t arg0,
                                  uintptr_t user_sp) {
    g_arch_enter_calls += 1;
    g_last_arch_enter_runtime = user_runtime;
    g_last_arch_enter_entry = entry;
    g_last_arch_enter_arg0 = arg0;
    g_last_arch_enter_user_sp = user_sp;
}

void trap_user_runtime_arch_resume(void) {}

void runtime_context_activate_trap_context(trap_context_t* trap_context) {
    g_active_trap_context = trap_context;
}

bool runtime_context_trap_context_is_active(const trap_context_t* trap_context) {
    return trap_context != NULL && trap_context == g_active_trap_context;
}

trap_context_t* runtime_context_active_trap_context(void) {
    return g_active_trap_context;
}

bool vm_process_set_user_context(vm_process_t* process,
                                 uintptr_t entry_pc,
                                 uintptr_t user_sp) {
    g_vm_process_set_user_context_calls += 1;
    g_last_entry_pc = entry_pc;
    g_last_user_sp = user_sp;
    if (!g_vm_process_set_user_context_result || process == NULL) {
        return false;
    }

    process->entry_pc = entry_pc;
    process->user_sp = user_sp;
    return true;
}

bool trap_context_install_standard_user_runtime_policies(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context) {
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_standard_policy_calls += 1;
    g_last_policy_trap_context = trap_context;
    g_last_policy_user_runtime = user_runtime;
    return g_standard_policy_result;
}

bool vm_process_is_runnable(const vm_process_t* process) {
    return process != NULL && g_vm_process_is_runnable_result;
}

bool vm_process_activate(vm_process_t* process) {
    g_vm_process_activate_calls += 1;
    if (!g_vm_process_activate_result || process == NULL) {
        return false;
    }

    g_active_process = process;
    return true;
}

bool vm_process_is_active(const vm_process_t* process) {
    return process != NULL && process == g_active_process;
}

bool vm_address_space_disable(vm_address_space_t* address_space) {
    g_vm_address_space_disable_calls += 1;
    g_last_disabled_address_space = address_space;
    g_active_process = NULL;
    return address_space != NULL && g_vm_address_space_disable_result;
}

void riscv_clear_sstatus_bits(uint64_t value) {
    g_last_cleared_sstatus_bits = value;
}

static void reset_stub_state(void) {
    g_active_trap_context = NULL;
    g_active_process = NULL;
    g_vm_process_set_user_context_calls = 0;
    g_last_entry_pc = 0;
    g_last_user_sp = 0;
    g_vm_process_set_user_context_result = true;
    g_standard_policy_calls = 0;
    g_last_policy_trap_context = NULL;
    g_last_policy_user_runtime = NULL;
    g_standard_policy_result = true;
    g_vm_process_is_runnable_result = true;
    g_vm_process_activate_calls = 0;
    g_vm_process_activate_result = true;
    g_vm_address_space_disable_calls = 0;
    g_last_disabled_address_space = NULL;
    g_vm_address_space_disable_result = true;
    g_last_cleared_sstatus_bits = 0;
    g_arch_enter_calls = 0;
    g_last_arch_enter_runtime = NULL;
    g_last_arch_enter_entry = 0;
    g_last_arch_enter_arg0 = 0;
    g_last_arch_enter_user_sp = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool stub_runtime_validate(const trap_user_runtime_t* user_runtime,
                                  uint64_t epc,
                                  uint64_t tval,
                                  void* context) {
    (void)user_runtime;
    (void)epc;
    (void)tval;
    (void)context;
    return true;
}

static void stub_timer_post_handler(uint64_t cause, void* context) {
    (void)cause;
    (void)context;
}

static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context) {
    (void)cause;
    (void)source_id;
    (void)context;
}

static int test_trap_context_init_and_activate(void) {
    trap_context_t trap_context;

    reset_stub_state();
    memset(&trap_context, 0xAB, sizeof(trap_context));
    trap_context_init(&trap_context);

    if (trap_context.interrupt_handlers[0].handler != NULL ||
        trap_context.exception_handlers[0].handler != NULL ||
        trap_context.supervisor_timer_policy.user_runtime != NULL ||
        trap_context.supervisor_external_policy.user_runtime != NULL ||
        trap_context.user_ecall_policy.resume_pc != 0) {
        return fail("expected trap context init to clear handlers and policies");
    }

    if (!trap_context_activate(&trap_context) ||
        !trap_context_is_active(&trap_context) ||
        trap_active_context() != &trap_context) {
        return fail("expected trap context activate to bind runtime active context");
    }

    return 0;
}

static int test_user_runtime_prepare_standard(void) {
    trap_context_t trap_context;
    trap_user_runtime_t user_runtime;
    vm_address_space_t address_space;
    vm_process_t process;
    static uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));

    reset_stub_state();
    memset(&trap_context, 0, sizeof(trap_context));
    memset(&user_runtime, 0, sizeof(user_runtime));
    memset(&address_space, 0, sizeof(address_space));
    memset(&process, 0, sizeof(process));
    address_space.allocated = true;
    address_space.root_table = (uint64_t*)MEM_BASE;
    address_space.root_table_pa = MEM_BASE;
    process.address_space = &address_space;
    trap_context_init(&trap_context);
    trap_user_runtime_init(&user_runtime);

    if (!trap_user_runtime_prepare_standard(&user_runtime,
                                            &trap_context,
                                            &process,
                                            0x1000,
                                            0x2000,
                                            7,
                                            trap_stack,
                                            sizeof(trap_stack),
                                            0x3000,
                                            stub_runtime_validate,
                                            &process,
                                            stub_timer_post_handler,
                                            &trap_context,
                                            stub_external_post_handler,
                                            trap_stack)) {
        return fail("expected user runtime prepare_standard to succeed");
    }

    if (g_vm_process_set_user_context_calls != 1 || g_last_entry_pc != 0x1000 ||
        g_last_user_sp != 0x2000 || g_standard_policy_calls != 1 ||
        g_last_policy_trap_context != &trap_context ||
        g_last_policy_user_runtime != &user_runtime ||
        user_runtime.trap_context != &trap_context ||
        user_runtime.process != &process || user_runtime.arg0 != 7 ||
        user_runtime.arch_state.supervisor_trap_stack_top !=
            (uintptr_t)trap_stack + sizeof(trap_stack) ||
        user_runtime.arch_state.supervisor_trap_stack_size != sizeof(trap_stack) ||
        user_runtime.expected_ecall_pc != 0x3000 ||
        user_runtime.resume_pc != (uintptr_t)trap_user_runtime_arch_resume ||
        user_runtime.validate != stub_runtime_validate) {
        return fail("expected prepare_standard to configure process, runtime and policies");
    }

    return 0;
}

static int test_user_runtime_activate_enter_and_deactivate(void) {
    trap_context_t trap_context;
    trap_user_runtime_t user_runtime;
    vm_address_space_t address_space;
    vm_process_t process;
    static uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));

    reset_stub_state();
    memset(&trap_context, 0, sizeof(trap_context));
    memset(&user_runtime, 0, sizeof(user_runtime));
    memset(&address_space, 0, sizeof(address_space));
    memset(&process, 0, sizeof(process));
    address_space.allocated = true;
    address_space.root_table = (uint64_t*)MEM_BASE;
    address_space.root_table_pa = MEM_BASE;
    process.address_space = &address_space;
    trap_context_init(&trap_context);
    trap_user_runtime_init(&user_runtime);
    if (!trap_user_runtime_prepare_standard(&user_runtime,
                                            &trap_context,
                                            &process,
                                            0x4000,
                                            0x5000,
                                            9,
                                            trap_stack,
                                            sizeof(trap_stack),
                                            0x6000,
                                            stub_runtime_validate,
                                            NULL,
                                            stub_timer_post_handler,
                                            NULL,
                                            stub_external_post_handler,
                                            NULL)) {
        return fail("expected runtime preparation before activate to succeed");
    }

    if (!trap_user_runtime_activate(&user_runtime) ||
        !trap_user_runtime_is_active(&user_runtime) ||
        trap_active_user_runtime() != &user_runtime ||
        g_vm_process_activate_calls != 1 ||
        g_active_trap_context != &trap_context) {
        return fail("expected runtime activate to enable process and trap context");
    }

    if (!trap_user_runtime_enter(&user_runtime) || g_arch_enter_calls != 1 ||
        g_last_arch_enter_runtime != &user_runtime ||
        g_last_arch_enter_entry != process.entry_pc ||
        g_last_arch_enter_arg0 != user_runtime.arg0 ||
        g_last_arch_enter_user_sp != process.user_sp) {
        return fail("expected runtime enter to call arch enter with process context");
    }

    if (!trap_user_runtime_deactivate(&user_runtime) ||
        g_last_cleared_sstatus_bits != RISCV_SSTATUS_SUM ||
        g_vm_address_space_disable_calls != 1 ||
        g_last_disabled_address_space != &address_space ||
        trap_active_user_runtime() != NULL) {
        return fail("expected runtime deactivate to clear SUM and disable address space");
    }

    return 0;
}

int main(void) {
    if (test_trap_context_init_and_activate() != 0 ||
        test_user_runtime_prepare_standard() != 0 ||
        test_user_runtime_activate_enter_and_deactivate() != 0) {
        return 1;
    }

    return 0;
}

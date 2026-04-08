#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
#include "../../guest/include/user_program.h"

static vm_address_space_t g_address_space;
static bool g_user_task_active = false;
static int g_user_task_init_calls = 0;
static int g_user_task_destroy_calls = 0;
static bool g_user_task_destroy_result = true;
static int g_bootstrap_init_calls = 0;
static int g_bootstrap_reset_calls = 0;
static bool g_bootstrap_reset_result = true;
static int g_bootstrap_plan_calls = 0;
static uintptr_t g_last_plan_exec_symbol = 0;
static uintptr_t g_last_plan_ecall_symbol = 0;
static bool g_bootstrap_plan_result = true;
static int g_user_task_create_calls = 0;
static bool g_user_task_create_result = true;
static int g_bootstrap_configure_calls = 0;
static uintptr_t g_last_configure_alias_paddr = 0;
static uintptr_t g_last_configure_stack_paddr = 0;
static bool g_bootstrap_configure_result = true;
static int g_bootstrap_bind_calls = 0;
static bool g_bootstrap_bind_result = true;
static int g_map_region_calls = 0;
static vm_user_region_t* g_last_map_region = NULL;
static vm_object_t* g_last_map_object = NULL;
static size_t g_last_map_object_offset = 0;
static bool g_map_region_result = true;
static int g_fault_region_calls = 0;
static vm_user_region_t* g_last_fault_region = NULL;
static vm_object_t* g_last_fault_object = NULL;
static size_t g_last_fault_object_offset = 0;
static bool g_fault_region_result = true;
static int g_prepare_standard_calls = 0;
static user_task_bootstrap_t* g_last_prepare_bootstrap = NULL;
static trap_context_t* g_last_prepare_trap_context = NULL;
static uintptr_t g_last_prepare_arg0 = 0;
static bool g_prepare_standard_result = true;
static int g_activate_calls = 0;
static bool g_activate_result = true;
static int g_deactivate_calls = 0;
static bool g_deactivate_result = true;
static bool g_runnable_result = true;
static int g_enter_calls = 0;
static bool g_enter_result = true;
static int g_region_unmap_calls = 0;
static vm_user_region_t* g_last_unmap_region = NULL;
static uintptr_t g_last_unmap_vaddr = 0;
static bool g_region_unmap_result = true;
static int g_region_set_fault_calls = 0;
static vm_user_region_t* g_last_set_fault_region = NULL;
static vm_object_t* g_last_set_fault_object = NULL;
static bool g_region_set_fault_result = true;
static int g_region_clear_object_calls = 0;
static vm_user_region_t* g_last_clear_object_region = NULL;
static bool g_region_clear_object_result = true;
static int g_object_reset_calls = 0;
static vm_object_t* g_last_reset_object = NULL;
static bool g_object_reset_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_user_program_lifecycle_and_helpers(void);
static int test_user_program_create_failure_replans_bootstrap(void);
static int test_user_program_create_failure_still_fails_when_cleanup_breaks(void);
static void clear_bootstrap_state(user_task_bootstrap_t* bootstrap);
static void fill_standard_bootstrap_layout(user_task_bootstrap_t* bootstrap,
                                           uintptr_t exec_symbol,
                                           uintptr_t ecall_symbol);
static bool stub_runtime_validate(const trap_user_runtime_t* user_runtime,
                                  uint64_t epc,
                                  uint64_t tval,
                                  void* context);
static void stub_timer_post_handler(uint64_t cause, void* context);
static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context);

static void clear_bootstrap_state(user_task_bootstrap_t* bootstrap) {
    if (bootstrap == NULL) {
        return;
    }

    memset(bootstrap, 0, sizeof(*bootstrap));
}

static void fill_standard_bootstrap_layout(user_task_bootstrap_t* bootstrap,
                                           uintptr_t exec_symbol,
                                           uintptr_t ecall_symbol) {
    const uintptr_t exec_page_paddr = align_down_page(exec_symbol);
    const uintptr_t exec_vaddr = 0x10000U;

    if (bootstrap == NULL) {
        return;
    }

    bootstrap->planned = true;
    bootstrap->exec_page_paddr = exec_page_paddr;
    bootstrap->exec_vaddr = exec_vaddr;
    bootstrap->stack_vaddr = exec_vaddr + MEMORY_PAGE_SIZE;
    bootstrap->alias_vaddr = exec_vaddr + 2U * MEMORY_PAGE_SIZE;
    bootstrap->anon_vaddr = exec_vaddr + 3U * MEMORY_PAGE_SIZE;
    bootstrap->anon_tail_vaddr = exec_vaddr + 4U * MEMORY_PAGE_SIZE;
    bootstrap->entry_pc = exec_vaddr + (exec_symbol - exec_page_paddr);
    bootstrap->expected_ecall_pc = exec_vaddr + (ecall_symbol - exec_page_paddr);
    bootstrap->user_sp = bootstrap->stack_vaddr + MEMORY_PAGE_SIZE;
}

void user_task_init(user_task_t* user_task) {
    g_user_task_init_calls += 1;
    if (user_task != NULL) {
        memset(user_task, 0, sizeof(*user_task));
    }
    g_user_task_active = false;
}

bool user_task_destroy(user_task_t* user_task) {
    g_user_task_destroy_calls += 1;
    if (!g_user_task_destroy_result || user_task == NULL) {
        return false;
    }

    user_task->address_space = NULL;
    user_task->process.address_space = NULL;
    user_task->process.entry_pc = 0;
    user_task->process.user_sp = 0;
    g_user_task_active = false;
    return true;
}

void user_task_bootstrap_init(user_task_bootstrap_t* bootstrap) {
    g_bootstrap_init_calls += 1;
    clear_bootstrap_state(bootstrap);
}

bool user_task_bootstrap_reset(user_task_bootstrap_t* bootstrap) {
    g_bootstrap_reset_calls += 1;
    if (!g_bootstrap_reset_result || bootstrap == NULL) {
        return false;
    }

    clear_bootstrap_state(bootstrap);
    return true;
}

bool user_task_bootstrap_plan_layout(user_task_bootstrap_t* bootstrap,
                                     uintptr_t exec_symbol,
                                     uintptr_t ecall_symbol) {
    g_bootstrap_plan_calls += 1;
    g_last_plan_exec_symbol = exec_symbol;
    g_last_plan_ecall_symbol = ecall_symbol;
    if (!g_bootstrap_plan_result || bootstrap == NULL) {
        return false;
    }

    fill_standard_bootstrap_layout(bootstrap, exec_symbol, ecall_symbol);
    return true;
}

bool user_task_create(user_task_t* user_task) {
    g_user_task_create_calls += 1;
    if (!g_user_task_create_result || user_task == NULL) {
        return false;
    }

    memset(&g_address_space, 0, sizeof(g_address_space));
    g_address_space.allocated = true;
    g_address_space.root_table = (uint64_t*)MEM_BASE;
    g_address_space.root_table_pa = MEM_BASE;
    user_task->address_space = &g_address_space;
    user_task->process.address_space = &g_address_space;
    return true;
}

bool user_task_bootstrap_configure(user_task_bootstrap_t* bootstrap,
                                   user_task_t* user_task,
                                   uintptr_t alias_backing_paddr,
                                   uintptr_t user_stack_paddr) {
    g_bootstrap_configure_calls += 1;
    g_last_configure_alias_paddr = alias_backing_paddr;
    g_last_configure_stack_paddr = user_stack_paddr;
    if (!g_bootstrap_configure_result || bootstrap == NULL || user_task == NULL) {
        return false;
    }

    bootstrap->configured = true;
    bootstrap->user_task = user_task;
    bootstrap->exec_object.initialized = true;
    bootstrap->stack_object.initialized = true;
    bootstrap->alias_object.initialized = true;
    bootstrap->anon_object.initialized = true;
    return true;
}

bool user_task_bootstrap_bind(user_task_bootstrap_t* bootstrap) {
    if (bootstrap == NULL) {
        return false;
    }

    g_bootstrap_bind_calls += 1;
    if (!g_bootstrap_bind_result) {
        return false;
    }

    bootstrap->bound = true;
    bootstrap->exec_region.vaddr = bootstrap->exec_vaddr;
    bootstrap->exec_region.size = MEMORY_PAGE_SIZE;
    bootstrap->exec_region.object = &bootstrap->exec_object;
    bootstrap->exec_region.object_mode = VM_REGION_OBJECT_MAPPED;
    bootstrap->stack_region.vaddr = bootstrap->stack_vaddr;
    bootstrap->stack_region.size = MEMORY_PAGE_SIZE;
    bootstrap->stack_region.object = &bootstrap->stack_object;
    bootstrap->stack_region.object_mode = VM_REGION_OBJECT_MAPPED;
    bootstrap->alias_region.vaddr = bootstrap->alias_vaddr;
    bootstrap->alias_region.size = MEMORY_PAGE_SIZE;
    bootstrap->alias_region.object = &bootstrap->alias_object;
    bootstrap->alias_region.object_mode = VM_REGION_OBJECT_MAPPED;
    bootstrap->anon_region.vaddr = bootstrap->anon_vaddr;
    bootstrap->anon_region.size = MEMORY_PAGE_SIZE;
    bootstrap->anon_region.object = &bootstrap->anon_object;
    bootstrap->anon_region.object_mode = VM_REGION_OBJECT_FAULT;
    bootstrap->anon_tail_region.vaddr = bootstrap->anon_tail_vaddr;
    bootstrap->anon_tail_region.size = MEMORY_PAGE_SIZE;
    bootstrap->anon_tail_region.object = &bootstrap->anon_object;
    bootstrap->anon_tail_region.object_mode = VM_REGION_OBJECT_FAULT;
    return true;
}

vm_address_space_t* user_task_address_space(user_task_t* user_task) {
    return user_task != NULL && user_task->address_space != NULL &&
                   user_task->process.address_space == user_task->address_space
               ? user_task->address_space
               : NULL;
}

vm_process_t* user_task_process(user_task_t* user_task) {
    return user_task_address_space(user_task) != NULL ? &user_task->process : NULL;
}

trap_user_runtime_t* user_task_runtime(user_task_t* user_task) {
    return user_task_address_space(user_task) != NULL ? &user_task->runtime : NULL;
}

bool user_task_map_object_region_at(user_task_t* user_task,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object,
                                    size_t object_offset) {
    (void)user_task;
    (void)vaddr;
    (void)size;
    (void)flags;
    g_map_region_calls += 1;
    g_last_map_region = region;
    g_last_map_object = object;
    g_last_map_object_offset = object_offset;
    return region != NULL && object != NULL && g_map_region_result;
}

bool user_task_set_fault_object_region_at(user_task_t* user_task,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object,
                                          size_t object_offset) {
    (void)user_task;
    (void)vaddr;
    (void)size;
    (void)flags;
    g_fault_region_calls += 1;
    g_last_fault_region = region;
    g_last_fault_object = object;
    g_last_fault_object_offset = object_offset;
    return region != NULL && object != NULL && g_fault_region_result;
}

bool user_task_bootstrap_prepare_standard(
    user_task_bootstrap_t* bootstrap,
    trap_context_t* trap_context,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context) {
    (void)trap_stack_base;
    (void)trap_stack_size;
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_prepare_standard_calls += 1;
    g_last_prepare_bootstrap = bootstrap;
    g_last_prepare_trap_context = trap_context;
    g_last_prepare_arg0 = arg0;
    g_prepare_standard_result = g_prepare_standard_result &&
                                validate != NULL &&
                                validate_context != NULL;
    return bootstrap != NULL && trap_context != NULL && g_prepare_standard_result;
}

bool user_task_activate(user_task_t* user_task) {
    g_activate_calls += 1;
    if (!g_activate_result || user_task == NULL) {
        return false;
    }

    g_user_task_active = true;
    return true;
}

bool user_task_deactivate(user_task_t* user_task) {
    g_deactivate_calls += 1;
    if (!g_deactivate_result || user_task == NULL) {
        return false;
    }

    g_user_task_active = false;
    return true;
}

bool user_task_is_active(const user_task_t* user_task) {
    return user_task != NULL && user_task->address_space != NULL && g_user_task_active;
}

bool user_task_is_runnable(const user_task_t* user_task) {
    return user_task != NULL && user_task->address_space != NULL && g_runnable_result;
}

bool user_task_enter(const user_task_t* user_task) {
    g_enter_calls += 1;
    return user_task != NULL && user_task->address_space != NULL && g_enter_result;
}

bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size) {
    return region != NULL && size != 0 &&
           range_within_window(vaddr,
                               size,
                               region->vaddr,
                               region->vaddr + (uintptr_t)region->size);
}

bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr) {
    g_region_unmap_calls += 1;
    g_last_unmap_region = region;
    g_last_unmap_vaddr = vaddr;
    return region != NULL && g_region_unmap_result;
}

bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object) {
    g_region_set_fault_calls += 1;
    g_last_set_fault_region = region;
    g_last_set_fault_object = object;
    if (!g_region_set_fault_result || region == NULL || object == NULL) {
        return false;
    }

    region->object = object;
    region->object_mode = VM_REGION_OBJECT_FAULT;
    return true;
}

bool vm_user_region_clear_object(vm_user_region_t* region) {
    g_region_clear_object_calls += 1;
    g_last_clear_object_region = region;
    if (!g_region_clear_object_result || region == NULL) {
        return false;
    }

    region->object = NULL;
    region->object_mode = VM_REGION_OBJECT_NONE;
    return true;
}

bool vm_object_reset(vm_object_t* object) {
    g_object_reset_calls += 1;
    g_last_reset_object = object;
    if (!g_object_reset_result || object == NULL) {
        return false;
    }

    object->initialized = false;
    return true;
}

static void reset_stub_state(void) {
    memset(&g_address_space, 0, sizeof(g_address_space));
    g_user_task_active = false;
    g_user_task_init_calls = 0;
    g_user_task_destroy_calls = 0;
    g_user_task_destroy_result = true;
    g_bootstrap_init_calls = 0;
    g_bootstrap_reset_calls = 0;
    g_bootstrap_reset_result = true;
    g_bootstrap_plan_calls = 0;
    g_last_plan_exec_symbol = 0;
    g_last_plan_ecall_symbol = 0;
    g_bootstrap_plan_result = true;
    g_user_task_create_calls = 0;
    g_user_task_create_result = true;
    g_bootstrap_configure_calls = 0;
    g_last_configure_alias_paddr = 0;
    g_last_configure_stack_paddr = 0;
    g_bootstrap_configure_result = true;
    g_bootstrap_bind_calls = 0;
    g_bootstrap_bind_result = true;
    g_map_region_calls = 0;
    g_last_map_region = NULL;
    g_last_map_object = NULL;
    g_last_map_object_offset = 0;
    g_map_region_result = true;
    g_fault_region_calls = 0;
    g_last_fault_region = NULL;
    g_last_fault_object = NULL;
    g_last_fault_object_offset = 0;
    g_fault_region_result = true;
    g_prepare_standard_calls = 0;
    g_last_prepare_bootstrap = NULL;
    g_last_prepare_trap_context = NULL;
    g_last_prepare_arg0 = 0;
    g_prepare_standard_result = true;
    g_activate_calls = 0;
    g_activate_result = true;
    g_deactivate_calls = 0;
    g_deactivate_result = true;
    g_runnable_result = true;
    g_enter_calls = 0;
    g_enter_result = true;
    g_region_unmap_calls = 0;
    g_last_unmap_region = NULL;
    g_last_unmap_vaddr = 0;
    g_region_unmap_result = true;
    g_region_set_fault_calls = 0;
    g_last_set_fault_region = NULL;
    g_last_set_fault_object = NULL;
    g_region_set_fault_result = true;
    g_region_clear_object_calls = 0;
    g_last_clear_object_region = NULL;
    g_region_clear_object_result = true;
    g_object_reset_calls = 0;
    g_last_reset_object = NULL;
    g_object_reset_result = true;
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

static int test_user_program_lifecycle_and_helpers(void) {
    user_program_t program;
    trap_context_t trap_context = {0};
    vm_user_region_t extra_region = {0};
    vm_object_t extra_object = {.initialized = true};
    const uintptr_t exec_symbol = 0x2000U + 0x90U;
    const uintptr_t ecall_symbol = 0x2000U + 0xB0U;
    static uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));

    reset_stub_state();
    memset(&program, 0x5A, sizeof(program));
    user_program_init(&program);

    if (g_user_task_init_calls != 1 || g_bootstrap_init_calls != 1 ||
        user_program_address_space(&program) != NULL ||
        user_program_process(&program) != NULL ||
        user_program_runtime(&program) != NULL ||
        user_program_value(&program, USER_PROGRAM_VALUE_ENTRY_PC) != 0) {
        return fail("expected user program init to clear lifecycle state");
    }

    if (!user_program_plan_standard(&program, exec_symbol, ecall_symbol) ||
        g_bootstrap_plan_calls != 1 || g_last_plan_exec_symbol != exec_symbol ||
        g_last_plan_ecall_symbol != ecall_symbol) {
        return fail("expected user program plan wrapper to forward symbols");
    }

    if (!user_program_create(&program, 0x7000U, 0x8000U) ||
        g_user_task_create_calls != 1 || g_bootstrap_configure_calls != 1 ||
        g_last_configure_alias_paddr != 0x7000U ||
        g_last_configure_stack_paddr != 0x8000U ||
        g_bootstrap_bind_calls != 1 ||
        user_program_address_space(&program) != &g_address_space ||
        user_program_process(&program) != &program.user_task.process ||
        user_program_runtime(&program) != &program.user_task.runtime ||
        user_program_value(&program, USER_PROGRAM_VALUE_ENTRY_PC) !=
            program.bootstrap.entry_pc ||
        !user_program_region_contains(&program,
                                      USER_PROGRAM_REGION_ALIAS,
                                      program.bootstrap.alias_vaddr,
                                      MEMORY_PAGE_SIZE)) {
        return fail("expected user program create to expose created lifecycle state");
    }

    if (!user_program_map_object_region_at(&program,
                                           &extra_region,
                                           0x9000U,
                                           MEMORY_PAGE_SIZE,
                                           VM_PAGE_READ | VM_PAGE_USER,
                                           &extra_object,
                                           MEMORY_PAGE_SIZE) ||
        g_map_region_calls != 1 || g_last_map_region != &extra_region ||
        g_last_map_object != &extra_object ||
        g_last_map_object_offset != MEMORY_PAGE_SIZE) {
        return fail("expected user program map wrapper to forward object region args");
    }

    if (!user_program_set_fault_object_region_at(
            &program,
            &extra_region,
            0xA000U,
            MEMORY_PAGE_SIZE,
            VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
            &extra_object,
            2U * MEMORY_PAGE_SIZE) ||
        g_fault_region_calls != 1 || g_last_fault_region != &extra_region ||
        g_last_fault_object != &extra_object ||
        g_last_fault_object_offset != 2U * MEMORY_PAGE_SIZE) {
        return fail("expected user program fault wrapper to forward object region args");
    }

    if (!user_program_prepare_standard(&program,
                                       &trap_context,
                                       3U,
                                       trap_stack,
                                       sizeof(trap_stack),
                                       stub_runtime_validate,
                                       &program,
                                       stub_timer_post_handler,
                                       &program.bootstrap.exec_region,
                                       stub_external_post_handler,
                                       &program.bootstrap.exec_object) ||
        g_prepare_standard_calls != 1 ||
        g_last_prepare_bootstrap != &program.bootstrap ||
        g_last_prepare_trap_context != &trap_context ||
        g_last_prepare_arg0 != 3U) {
        return fail("expected user program prepare wrapper to forward bootstrap state");
    }

    if (!user_program_activate(&program) || !user_program_is_active(&program) ||
        !user_program_is_runnable(&program) || !user_program_enter(&program) ||
        !user_program_deactivate(&program) || g_activate_calls != 1 ||
        g_deactivate_calls != 1 || g_enter_calls != 1) {
        return fail("expected user program runtime helpers to forward task state");
    }

    if (!user_program_set_region_fault_object(&program,
                                              USER_PROGRAM_REGION_ALIAS,
                                              &extra_object) ||
        g_region_set_fault_calls != 1 ||
        g_last_set_fault_region != &program.bootstrap.alias_region ||
        g_last_set_fault_object != &extra_object) {
        return fail("expected region fault-object helper to target selected region");
    }

    if (!user_program_rebind_region_fault_object(&program,
                                                 USER_PROGRAM_REGION_ALIAS,
                                                 &program.bootstrap.alias_object) ||
        g_region_clear_object_calls != 1 ||
        g_last_clear_object_region != &program.bootstrap.alias_region ||
        g_region_set_fault_calls != 2 ||
        g_last_set_fault_object != &program.bootstrap.alias_object) {
        return fail("expected region rebind helper to clear then rebind object");
    }

    if (!user_program_unmap_region_base_page(&program, USER_PROGRAM_REGION_ALIAS) ||
        g_region_unmap_calls != 1 ||
        g_last_unmap_region != &program.bootstrap.alias_region ||
        g_last_unmap_vaddr != program.bootstrap.alias_region.vaddr) {
        return fail("expected region base-page unmap helper to target region base");
    }

    if (!user_program_reset_object(&program, USER_PROGRAM_OBJECT_ANON) ||
        g_object_reset_calls != 1 ||
        g_last_reset_object != &program.bootstrap.anon_object ||
        user_program_region(&program, USER_PROGRAM_REGION_STACK) !=
            &program.bootstrap.stack_region ||
        user_program_object(&program, USER_PROGRAM_OBJECT_EXEC) !=
            &program.bootstrap.exec_object) {
        return fail("expected object and region helpers to select bootstrap slots");
    }

    if (!user_program_destroy(&program) || g_user_task_destroy_calls != 1 ||
        g_bootstrap_reset_calls != 1 || g_user_task_init_calls != 2 ||
        g_bootstrap_init_calls != 2 || user_program_address_space(&program) != NULL ||
        user_program_process(&program) != NULL) {
        return fail("expected user program destroy to reset task and bootstrap state");
    }

    return 0;
}

static int test_user_program_create_failure_replans_bootstrap(void) {
    user_program_t program;
    const uintptr_t exec_symbol = 0x3000U + 0x88U;
    const uintptr_t ecall_symbol = 0x3000U + 0xA8U;

    reset_stub_state();
    memset(&program, 0, sizeof(program));
    user_program_init(&program);
    if (!user_program_plan_standard(&program, exec_symbol, ecall_symbol)) {
        return fail("expected initial user program plan to succeed");
    }

    g_bootstrap_bind_result = false;
    if (user_program_create(&program, 0x9000U, 0xA000U) ||
        g_user_task_create_calls != 1 || g_bootstrap_configure_calls != 1 ||
        g_bootstrap_bind_calls != 1 || g_user_task_destroy_calls != 1 ||
        g_bootstrap_reset_calls != 1 || g_bootstrap_plan_calls != 2 ||
        g_last_plan_exec_symbol != exec_symbol ||
        g_last_plan_ecall_symbol != ecall_symbol || !program.bootstrap.planned ||
        program.user_task.address_space != NULL ||
        program.user_task.process.address_space != NULL) {
        return fail("expected user program create failure to destroy and replan state");
    }

    return 0;
}

static int test_user_program_create_failure_still_fails_when_cleanup_breaks(void) {
    user_program_t program;

    reset_stub_state();
    memset(&program, 0, sizeof(program));
    user_program_init(&program);
    if (!user_program_plan_standard(&program, 0x3000U + 0x88U, 0x3000U + 0xA8U)) {
        return fail("expected initial user program plan to succeed");
    }

    g_bootstrap_bind_result = false;
    g_user_task_destroy_result = false;
    if (user_program_create(&program, 0x9000U, 0xA000U)) {
        return fail("expected create failure to stay failed when cleanup also fails");
    }

    if (g_user_task_create_calls != 1 || g_bootstrap_configure_calls != 1 ||
        g_bootstrap_bind_calls != 1 || g_user_task_destroy_calls != 1 ||
        g_bootstrap_plan_calls != 1) {
        return fail("expected create failure to stop after failed cleanup");
    }

    return 0;
}

int main(void) {
    if (test_user_program_lifecycle_and_helpers() != 0 ||
        test_user_program_create_failure_replans_bootstrap() != 0 ||
        test_user_program_create_failure_still_fails_when_cleanup_breaks() != 0) {
        return 1;
    }

    return 0;
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
#include "../../guest/include/user_task.h"

static vm_address_space_t g_created_address_space;
static trap_user_runtime_t* g_active_runtime = NULL;
static int g_runtime_init_calls = 0;
static int g_address_space_create_calls = 0;
static bool g_address_space_create_result = true;
static int g_address_space_destroy_calls = 0;
static vm_address_space_t* g_last_destroyed_address_space = NULL;
static bool g_address_space_destroy_result = true;
static int g_process_create_calls = 0;
static vm_address_space_t* g_last_process_create_address_space = NULL;
static bool g_process_create_result = true;
static int g_process_reset_calls = 0;
static bool g_process_reset_result = true;
static int g_process_bind_calls = 0;
static const vm_process_user_region_binding_t* g_last_bindings = NULL;
static size_t g_last_binding_count = 0;
static bool g_process_bind_result = true;
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
static uintptr_t g_last_prepare_entry_pc = 0;
static uintptr_t g_last_prepare_user_sp = 0;
static uintptr_t g_last_prepare_arg0 = 0;
static uintptr_t g_last_prepare_expected_ecall_pc = 0;
static void* g_last_prepare_trap_stack_base = NULL;
static size_t g_last_prepare_trap_stack_size = 0;
static trap_context_t* g_last_prepare_trap_context = NULL;
static trap_user_runtime_t* g_last_prepare_runtime = NULL;
static trap_user_runtime_validate_t g_last_prepare_validate = NULL;
static void* g_last_prepare_validate_context = NULL;
static bool g_prepare_standard_result = true;
static int g_runtime_activate_calls = 0;
static bool g_runtime_activate_result = true;
static int g_runtime_deactivate_calls = 0;
static bool g_runtime_deactivate_result = true;
static int g_runtime_enter_calls = 0;
static bool g_runtime_enter_result = true;
static bool g_process_runnable_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_user_task_lifecycle_and_wrappers(void);
static int test_user_task_create_rolls_back_failed_process_create(void);
static bool stub_runtime_validate(const trap_user_runtime_t* user_runtime,
                                  uint64_t epc,
                                  uint64_t tval,
                                  void* context);
static void stub_timer_post_handler(uint64_t cause, void* context);
static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context);

void trap_user_runtime_init(trap_user_runtime_t* user_runtime) {
    g_runtime_init_calls += 1;
    if (user_runtime != NULL) {
        memset(user_runtime, 0, sizeof(*user_runtime));
    }
}

bool vm_address_space_create(vm_address_space_t** out_space) {
    g_address_space_create_calls += 1;
    if (!g_address_space_create_result || out_space == NULL) {
        return false;
    }

    memset(&g_created_address_space, 0, sizeof(g_created_address_space));
    g_created_address_space.allocated = true;
    g_created_address_space.root_table = (uint64_t*)MEM_BASE;
    g_created_address_space.root_table_pa = MEM_BASE;
    *out_space = &g_created_address_space;
    return true;
}

bool vm_address_space_destroy(vm_address_space_t* address_space) {
    g_address_space_destroy_calls += 1;
    g_last_destroyed_address_space = address_space;
    return address_space != NULL && g_address_space_destroy_result;
}

bool vm_process_create(vm_process_t* process, vm_address_space_t* address_space) {
    size_t i = 0;

    g_process_create_calls += 1;
    g_last_process_create_address_space = address_space;
    if (!g_process_create_result || process == NULL || address_space == NULL) {
        return false;
    }

    process->address_space = address_space;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }
    return true;
}

bool vm_process_reset(vm_process_t* process) {
    size_t i = 0;

    g_process_reset_calls += 1;
    if (!g_process_reset_result || process == NULL) {
        return false;
    }

    process->address_space = NULL;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }
    return true;
}

bool vm_process_bind_user_regions(
    vm_process_t* process,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count) {
    g_process_bind_calls += 1;
    g_last_bindings = bindings;
    g_last_binding_count = binding_count;
    return process != NULL && bindings != NULL && g_process_bind_result;
}

bool vm_process_map_object_region_at(vm_process_t* process,
                                     vm_user_region_t* region,
                                     uintptr_t vaddr,
                                     size_t size,
                                     uint64_t flags,
                                     vm_object_t* object,
                                     size_t object_offset) {
    (void)process;
    (void)vaddr;
    (void)size;
    (void)flags;
    g_map_region_calls += 1;
    g_last_map_region = region;
    g_last_map_object = object;
    g_last_map_object_offset = object_offset;
    return region != NULL && object != NULL && g_map_region_result;
}

bool vm_process_set_fault_object_region_at(vm_process_t* process,
                                           vm_user_region_t* region,
                                           uintptr_t vaddr,
                                           size_t size,
                                           uint64_t flags,
                                           vm_object_t* object,
                                           size_t object_offset) {
    (void)process;
    (void)vaddr;
    (void)size;
    (void)flags;
    g_fault_region_calls += 1;
    g_last_fault_region = region;
    g_last_fault_object = object;
    g_last_fault_object_offset = object_offset;
    return region != NULL && object != NULL && g_fault_region_result;
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
    (void)process;
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_prepare_standard_calls += 1;
    g_last_prepare_runtime = user_runtime;
    g_last_prepare_trap_context = trap_context;
    g_last_prepare_entry_pc = entry_pc;
    g_last_prepare_user_sp = user_sp;
    g_last_prepare_arg0 = arg0;
    g_last_prepare_trap_stack_base = trap_stack_base;
    g_last_prepare_trap_stack_size = trap_stack_size;
    g_last_prepare_expected_ecall_pc = expected_ecall_pc;
    g_last_prepare_validate = validate;
    g_last_prepare_validate_context = validate_context;
    return user_runtime != NULL && trap_context != NULL && g_prepare_standard_result;
}

bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime) {
    g_runtime_activate_calls += 1;
    if (!g_runtime_activate_result || user_runtime == NULL) {
        return false;
    }

    g_active_runtime = user_runtime;
    return true;
}

bool trap_user_runtime_is_active(const trap_user_runtime_t* user_runtime) {
    return user_runtime != NULL && user_runtime == g_active_runtime;
}

bool trap_user_runtime_deactivate(trap_user_runtime_t* user_runtime) {
    g_runtime_deactivate_calls += 1;
    if (!g_runtime_deactivate_result || user_runtime == NULL ||
        user_runtime != g_active_runtime) {
        return false;
    }

    g_active_runtime = NULL;
    return true;
}

bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime) {
    g_runtime_enter_calls += 1;
    return user_runtime != NULL && g_runtime_enter_result;
}

bool vm_process_is_runnable(const vm_process_t* process) {
    return process != NULL && process->address_space != NULL &&
           g_process_runnable_result;
}

static void reset_stub_state(void) {
    memset(&g_created_address_space, 0, sizeof(g_created_address_space));
    g_active_runtime = NULL;
    g_runtime_init_calls = 0;
    g_address_space_create_calls = 0;
    g_address_space_create_result = true;
    g_address_space_destroy_calls = 0;
    g_last_destroyed_address_space = NULL;
    g_address_space_destroy_result = true;
    g_process_create_calls = 0;
    g_last_process_create_address_space = NULL;
    g_process_create_result = true;
    g_process_reset_calls = 0;
    g_process_reset_result = true;
    g_process_bind_calls = 0;
    g_last_bindings = NULL;
    g_last_binding_count = 0;
    g_process_bind_result = true;
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
    g_last_prepare_entry_pc = 0;
    g_last_prepare_user_sp = 0;
    g_last_prepare_arg0 = 0;
    g_last_prepare_expected_ecall_pc = 0;
    g_last_prepare_trap_stack_base = NULL;
    g_last_prepare_trap_stack_size = 0;
    g_last_prepare_trap_context = NULL;
    g_last_prepare_runtime = NULL;
    g_last_prepare_validate = NULL;
    g_last_prepare_validate_context = NULL;
    g_prepare_standard_result = true;
    g_runtime_activate_calls = 0;
    g_runtime_activate_result = true;
    g_runtime_deactivate_calls = 0;
    g_runtime_deactivate_result = true;
    g_runtime_enter_calls = 0;
    g_runtime_enter_result = true;
    g_process_runnable_result = true;
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

static int test_user_task_lifecycle_and_wrappers(void) {
    user_task_t user_task;
    trap_context_t trap_context = {0};
    vm_user_region_t region = {0};
    vm_object_t object = {.initialized = true};
    vm_process_user_region_binding_t binding = {
        .region = &region,
        .vaddr = MEMORY_PAGE_SIZE,
        .size = MEMORY_PAGE_SIZE,
        .flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
        .object = &object,
        .object_offset = 0,
        .object_mode = VM_REGION_OBJECT_MAPPED,
    };
    static uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));

    reset_stub_state();
    memset(&user_task, 0xA5, sizeof(user_task));
    user_task_init(&user_task);

    if (g_runtime_init_calls != 1 || user_task.address_space != NULL ||
        user_task.process.address_space != NULL ||
        user_task_address_space(&user_task) != NULL ||
        user_task_process(&user_task) != NULL ||
        user_task_bind_regions(&user_task, &binding, 1U) ||
        user_task_map_object_region(&user_task,
                                    &region,
                                    MEMORY_PAGE_SIZE,
                                    MEMORY_PAGE_SIZE,
                                    VM_PAGE_READ | VM_PAGE_USER,
                                    &object) ||
        user_task_set_fault_object_region(&user_task,
                                          &region,
                                          2U * MEMORY_PAGE_SIZE,
                                          MEMORY_PAGE_SIZE,
                                          VM_PAGE_READ | VM_PAGE_WRITE |
                                              VM_PAGE_USER,
                                          &object)) {
        return fail("expected uncreated user task helpers to reject requests");
    }

    if (!user_task_create(&user_task) || g_address_space_create_calls != 1 ||
        g_process_create_calls != 1 ||
        g_last_process_create_address_space != &g_created_address_space ||
        user_task.address_space != &g_created_address_space ||
        user_task.process.address_space != &g_created_address_space ||
        user_task_address_space(&user_task) != &g_created_address_space ||
        user_task_process(&user_task) != &user_task.process ||
        user_task_runtime(&user_task) != &user_task.runtime) {
        return fail("expected user task create to bind address space and process");
    }

    if (!user_task_bind_regions(&user_task, &binding, 1U) ||
        g_process_bind_calls != 1 || g_last_bindings != &binding ||
        g_last_binding_count != 1U) {
        return fail("expected user task bind wrapper to forward bindings");
    }

    if (!user_task_map_object_region_at(&user_task,
                                        &region,
                                        MEMORY_PAGE_SIZE,
                                        MEMORY_PAGE_SIZE,
                                        VM_PAGE_READ | VM_PAGE_USER,
                                        &object,
                                        MEMORY_PAGE_SIZE) ||
        g_map_region_calls != 1 || g_last_map_region != &region ||
        g_last_map_object != &object ||
        g_last_map_object_offset != MEMORY_PAGE_SIZE) {
        return fail("expected user task map wrapper to forward object region args");
    }

    if (!user_task_set_fault_object_region_at(
            &user_task,
            &region,
            2U * MEMORY_PAGE_SIZE,
            MEMORY_PAGE_SIZE,
            VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
            &object,
            2U * MEMORY_PAGE_SIZE) ||
        g_fault_region_calls != 1 || g_last_fault_region != &region ||
        g_last_fault_object != &object ||
        g_last_fault_object_offset != 2U * MEMORY_PAGE_SIZE) {
        return fail("expected user task fault wrapper to forward object region args");
    }

    if (!user_task_prepare_standard(&user_task,
                                    &trap_context,
                                    0x1000U,
                                    0x2000U,
                                    7U,
                                    trap_stack,
                                    sizeof(trap_stack),
                                    0x3000U,
                                    stub_runtime_validate,
                                    &binding,
                                    stub_timer_post_handler,
                                    &region,
                                    stub_external_post_handler,
                                    &object) ||
        g_prepare_standard_calls != 1 || g_last_prepare_runtime != &user_task.runtime ||
        g_last_prepare_trap_context != &trap_context ||
        g_last_prepare_entry_pc != 0x1000U ||
        g_last_prepare_user_sp != 0x2000U ||
        g_last_prepare_arg0 != 7U ||
        g_last_prepare_trap_stack_base != trap_stack ||
        g_last_prepare_trap_stack_size != sizeof(trap_stack) ||
        g_last_prepare_expected_ecall_pc != 0x3000U ||
        g_last_prepare_validate != stub_runtime_validate ||
        g_last_prepare_validate_context != &binding) {
        return fail("expected user task prepare wrapper to forward runtime args");
    }

    if (!user_task_activate(&user_task) || !user_task_is_active(&user_task) ||
        !user_task_is_runnable(&user_task) || !user_task_enter(&user_task) ||
        g_runtime_activate_calls != 1 || g_runtime_enter_calls != 1) {
        return fail("expected user task runtime helpers to forward active state");
    }

    if (!user_task_destroy(&user_task) || g_runtime_deactivate_calls != 1 ||
        g_process_reset_calls != 1 || g_address_space_destroy_calls != 1 ||
        g_last_destroyed_address_space != &g_created_address_space ||
        g_runtime_init_calls != 2 || user_task.address_space != NULL ||
        user_task.process.address_space != NULL ||
        user_task.process.entry_pc != 0 || user_task.process.user_sp != 0 ||
        user_task_process(&user_task) != NULL ||
        user_task_address_space(&user_task) != NULL) {
        return fail("expected user task destroy to deactivate and reinitialize");
    }

    return 0;
}

static int test_user_task_create_rolls_back_failed_process_create(void) {
    user_task_t user_task;

    reset_stub_state();
    memset(&user_task, 0, sizeof(user_task));
    user_task_init(&user_task);
    g_process_create_result = false;
    if (user_task_create(&user_task) || g_address_space_create_calls != 1 ||
        g_process_create_calls != 1 || g_address_space_destroy_calls != 1 ||
        g_last_destroyed_address_space != &g_created_address_space ||
        user_task.address_space != NULL || user_task.process.address_space != NULL) {
        return fail("expected user task create failure to destroy allocated address space");
    }

    return 0;
}

int main(void) {
    if (test_user_task_lifecycle_and_wrappers() != 0 ||
        test_user_task_create_rolls_back_failed_process_create() != 0) {
        return 1;
    }

    return 0;
}

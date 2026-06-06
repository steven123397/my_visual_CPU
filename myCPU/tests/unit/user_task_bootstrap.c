#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"
#include "../../guest/include/user_task_bootstrap.h"

static uintptr_t g_user_limit = VM_USER_VADDR_LIMIT;
static int g_physical_init_calls = 0;
static uintptr_t g_last_physical_paddr = 0;
static size_t g_last_physical_size = 0;
static bool g_physical_init_result = true;
static int g_anon_init_calls = 0;
static size_t g_last_anon_size = 0;
static bool g_anon_init_result = true;
static int g_object_reset_calls = 0;
static bool g_object_reset_result = true;
static int g_bind_regions_calls = 0;
static vm_process_user_region_binding_t g_last_bindings[5];
static size_t g_last_binding_count = 0;
static bool g_bind_regions_result = true;
static int g_prepare_standard_calls = 0;
static uintptr_t g_last_prepare_entry_pc = 0;
static uintptr_t g_last_prepare_user_sp = 0;
static uintptr_t g_last_prepare_arg0 = 0;
static uintptr_t g_last_prepare_expected_ecall_pc = 0;
static trap_context_t* g_last_prepare_trap_context = NULL;
static user_task_t* g_last_prepare_user_task = NULL;
static trap_user_runtime_validate_t g_last_prepare_validate = NULL;
static void* g_last_prepare_validate_context = NULL;
static bool g_prepare_standard_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_bootstrap_plan_configure_bind_and_prepare(void);
static int test_bootstrap_configure_failure_resets_state(void);
static int test_bootstrap_reset_clears_initialized_objects(void);
static bool stub_runtime_validate(const trap_user_runtime_t* user_runtime,
                                  uint64_t epc,
                                  uint64_t tval,
                                  void* context);
static void stub_timer_post_handler(uint64_t cause, void* context);
static void stub_external_post_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context);

uintptr_t vm_user_limit(void) {
    return g_user_limit;
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, g_user_limit);
}

vm_address_space_t* user_task_address_space(user_task_t* user_task) {
    return user_task != NULL ? user_task->address_space : NULL;
}

vm_process_t* user_task_process(user_task_t* user_task) {
    return user_task != NULL && user_task->address_space != NULL &&
                   user_task->process.address_space == user_task->address_space
               ? &user_task->process
               : NULL;
}

trap_user_runtime_t* user_task_runtime(user_task_t* user_task) {
    return user_task != NULL ? &user_task->runtime : NULL;
}

bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size) {
    g_physical_init_calls += 1;
    g_last_physical_paddr = paddr;
    g_last_physical_size = size;
    if (!g_physical_init_result || object == NULL) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_PHYSICAL;
    object->size = size;
    object->attachment_count = 0;
    object->backing.physical.base_paddr = paddr;
    return true;
}

bool vm_object_init_anon(vm_object_t* object, size_t size) {
    static uintptr_t page_slots[VM_OBJECT_ANON_PAGE_SLOTS];
    size_t i = 0;

    g_anon_init_calls += 1;
    g_last_anon_size = size;
    if (!g_anon_init_result || object == NULL) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_ANON;
    object->size = size;
    object->attachment_count = 0;
    object->backing.anon.page_slots = page_slots;
    object->backing.anon.page_count = object_page_count(size);
    for (i = 0; i < VM_OBJECT_ANON_SLOT_TABLE_COUNT - 1U; ++i) {
        object->backing.anon.extra_page_slots[i] = NULL;
    }
    return true;
}

bool vm_object_reset(vm_object_t* object) {
    size_t i = 0;

    g_object_reset_calls += 1;
    if (!g_object_reset_result || object == NULL) {
        return false;
    }

    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = NULL;
    object->backing.anon.page_count = 0;
    for (i = 0; i < VM_OBJECT_ANON_SLOT_TABLE_COUNT - 1U; ++i) {
        object->backing.anon.extra_page_slots[i] = NULL;
    }
    return true;
}

bool user_task_bind_regions(
    user_task_t* user_task,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count) {
    size_t i = 0;

    g_bind_regions_calls += 1;
    g_last_binding_count = binding_count;
    if (!g_bind_regions_result || user_task == NULL || bindings == NULL ||
        binding_count > 5U) {
        return false;
    }

    for (i = 0; i < binding_count; ++i) {
        g_last_bindings[i] = bindings[i];
    }
    return true;
}

bool user_task_prepare_standard(
    user_task_t* user_task,
    trap_context_t* trap_context,
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
    (void)trap_stack_base;
    (void)trap_stack_size;
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_prepare_standard_calls += 1;
    g_last_prepare_user_task = user_task;
    g_last_prepare_trap_context = trap_context;
    g_last_prepare_entry_pc = entry_pc;
    g_last_prepare_user_sp = user_sp;
    g_last_prepare_arg0 = arg0;
    g_last_prepare_expected_ecall_pc = expected_ecall_pc;
    g_last_prepare_validate = validate;
    g_last_prepare_validate_context = validate_context;
    return user_task != NULL && trap_context != NULL && g_prepare_standard_result;
}

static void reset_stub_state(void) {
    g_user_limit = VM_USER_VADDR_LIMIT;
    g_physical_init_calls = 0;
    g_last_physical_paddr = 0;
    g_last_physical_size = 0;
    g_physical_init_result = true;
    g_anon_init_calls = 0;
    g_last_anon_size = 0;
    g_anon_init_result = true;
    g_object_reset_calls = 0;
    g_object_reset_result = true;
    g_bind_regions_calls = 0;
    memset(g_last_bindings, 0, sizeof(g_last_bindings));
    g_last_binding_count = 0;
    g_bind_regions_result = true;
    g_prepare_standard_calls = 0;
    g_last_prepare_entry_pc = 0;
    g_last_prepare_user_sp = 0;
    g_last_prepare_arg0 = 0;
    g_last_prepare_expected_ecall_pc = 0;
    g_last_prepare_trap_context = NULL;
    g_last_prepare_user_task = NULL;
    g_last_prepare_validate = NULL;
    g_last_prepare_validate_context = NULL;
    g_prepare_standard_result = true;
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

static int test_bootstrap_plan_configure_bind_and_prepare(void) {
    user_task_bootstrap_t bootstrap;
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
        .root_table_pa = MEM_BASE,
    };
    user_task_t user_task = {
        .address_space = &address_space,
        .process = {.address_space = &address_space},
    };
    trap_context_t trap_context = {0};
    const uintptr_t exec_symbol = 0x2000U + 0x180U;
    const uintptr_t ecall_symbol = 0x2000U + 0x1A0U;
    const uintptr_t alias_backing_paddr = 0x9000U;
    const uintptr_t user_stack_paddr = 0xA000U;
    const uintptr_t expected_alias_vaddr = VM_USER_VADDR_LIMIT - MEMORY_PAGE_SIZE;
    const uintptr_t expected_anon_tail_vaddr =
        expected_alias_vaddr - 5U * MEMORY_PAGE_SIZE;
    const uintptr_t expected_anon_vaddr =
        expected_alias_vaddr - 4U * MEMORY_PAGE_SIZE;
    const uintptr_t expected_stack_vaddr =
        expected_alias_vaddr - 3U * MEMORY_PAGE_SIZE;
    const uintptr_t expected_exec_vaddr =
        expected_alias_vaddr - 2U * MEMORY_PAGE_SIZE;
    static uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));

    reset_stub_state();
    memset(&bootstrap, 0xCD, sizeof(bootstrap));
    user_task_bootstrap_init(&bootstrap);

    if (!user_task_bootstrap_plan_layout(&bootstrap, exec_symbol, ecall_symbol) ||
        !bootstrap.planned || bootstrap.exec_page_paddr != 0x2000U ||
        bootstrap.alias_vaddr != expected_alias_vaddr ||
        bootstrap.anon_tail_vaddr != expected_anon_tail_vaddr ||
        bootstrap.anon_vaddr != expected_anon_vaddr ||
        bootstrap.stack_vaddr != expected_stack_vaddr ||
        bootstrap.exec_vaddr != expected_exec_vaddr ||
        bootstrap.entry_pc != expected_exec_vaddr + 0x180U ||
        bootstrap.expected_ecall_pc != expected_exec_vaddr + 0x1A0U ||
        bootstrap.user_sp != expected_stack_vaddr + MEMORY_PAGE_SIZE) {
        return fail("expected bootstrap plan to compute canonical user layout");
    }

    if (!user_task_bootstrap_configure(&bootstrap,
                                       &user_task,
                                       alias_backing_paddr,
                                       user_stack_paddr) ||
        !bootstrap.configured || bootstrap.user_task != &user_task ||
        g_physical_init_calls != 3 || g_last_physical_paddr != alias_backing_paddr ||
        g_last_physical_size != MEMORY_PAGE_SIZE || g_anon_init_calls != 1 ||
        g_last_anon_size != 2U * MEMORY_PAGE_SIZE) {
        return fail("expected bootstrap configure to initialize backing objects");
    }

    if (!user_task_bootstrap_bind(&bootstrap) || !bootstrap.bound ||
        g_bind_regions_calls != 1 || g_last_binding_count != 5U ||
        g_last_bindings[0].region != &bootstrap.exec_region ||
        g_last_bindings[0].vaddr != bootstrap.exec_vaddr ||
        g_last_bindings[0].flags !=
            (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        g_last_bindings[0].object != &bootstrap.exec_object ||
        g_last_bindings[0].object_mode != VM_REGION_OBJECT_MAPPED ||
        g_last_bindings[4].region != &bootstrap.anon_tail_region ||
        g_last_bindings[4].vaddr != bootstrap.anon_tail_vaddr ||
        g_last_bindings[4].object != &bootstrap.anon_object ||
        g_last_bindings[4].object_offset != MEMORY_PAGE_SIZE ||
        g_last_bindings[4].object_mode != VM_REGION_OBJECT_FAULT) {
        return fail("expected bootstrap bind to assemble standard region bindings");
    }

    if (!user_task_bootstrap_prepare_standard(&bootstrap,
                                              &trap_context,
                                              9U,
                                              trap_stack,
                                              sizeof(trap_stack),
                                              stub_runtime_validate,
                                              &bootstrap,
                                              stub_timer_post_handler,
                                              &user_task,
                                              stub_external_post_handler,
                                              &address_space) ||
        g_prepare_standard_calls != 1 || g_last_prepare_user_task != &user_task ||
        g_last_prepare_trap_context != &trap_context ||
        g_last_prepare_entry_pc != bootstrap.entry_pc ||
        g_last_prepare_user_sp != bootstrap.user_sp ||
        g_last_prepare_arg0 != 9U ||
        g_last_prepare_expected_ecall_pc != bootstrap.expected_ecall_pc ||
        g_last_prepare_validate != stub_runtime_validate ||
        g_last_prepare_validate_context != &bootstrap) {
        return fail("expected bootstrap prepare wrapper to forward planned entry state");
    }

    return 0;
}

static int test_bootstrap_configure_failure_resets_state(void) {
    user_task_bootstrap_t bootstrap;
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
        .root_table_pa = MEM_BASE,
    };
    user_task_t user_task = {
        .address_space = &address_space,
        .process = {.address_space = &address_space},
    };

    reset_stub_state();
    memset(&bootstrap, 0, sizeof(bootstrap));
    if (!user_task_bootstrap_plan_layout(&bootstrap, 0x2000U, 0x2100U)) {
        return fail("expected bootstrap plan setup to succeed");
    }

    g_anon_init_result = false;
    if (user_task_bootstrap_configure(&bootstrap, &user_task, 0x3000U, 0x4000U) ||
        g_physical_init_calls != 3 || g_anon_init_calls != 1 ||
        g_object_reset_calls != 3 || bootstrap.planned || bootstrap.configured ||
        bootstrap.bound || bootstrap.user_task != NULL ||
        bootstrap.exec_page_paddr != 0 || bootstrap.entry_pc != 0 ||
        bootstrap.exec_object.initialized || bootstrap.stack_object.initialized ||
        bootstrap.alias_object.initialized || bootstrap.anon_object.initialized) {
        return fail("expected bootstrap configure failure to reset planned state");
    }

    return 0;
}

static int test_bootstrap_reset_clears_initialized_objects(void) {
    user_task_bootstrap_t bootstrap;
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
        .root_table_pa = MEM_BASE,
    };
    user_task_t user_task = {
        .address_space = &address_space,
        .process = {.address_space = &address_space},
    };

    reset_stub_state();
    memset(&bootstrap, 0, sizeof(bootstrap));
    if (!user_task_bootstrap_plan_layout(&bootstrap, 0x5000U, 0x5200U) ||
        !user_task_bootstrap_configure(&bootstrap, &user_task, 0x7000U, 0x8000U) ||
        !user_task_bootstrap_reset(&bootstrap) || g_object_reset_calls != 4 ||
        bootstrap.planned || bootstrap.configured || bootstrap.bound ||
        bootstrap.user_task != NULL || bootstrap.exec_page_paddr != 0 ||
        bootstrap.exec_object.initialized || bootstrap.anon_object.initialized) {
        return fail("expected bootstrap reset to clear initialized objects and state");
    }

    return 0;
}

int main(void) {
    if (test_bootstrap_plan_configure_bind_and_prepare() != 0 ||
        test_bootstrap_configure_failure_resets_state() != 0 ||
        test_bootstrap_reset_clears_initialized_objects() != 0) {
        return 1;
    }

    return 0;
}

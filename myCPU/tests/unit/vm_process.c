#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"

static vm_process_t* g_active_process = NULL;
static int g_address_space_enable_calls = 0;
static vm_address_space_t* g_last_enabled_address_space = NULL;
static bool g_address_space_enable_result = true;
static int g_user_region_init_calls = 0;
static bool g_user_region_init_result = true;
static int g_unregister_user_region_calls = 0;
static bool g_unregister_user_region_result = true;
static int g_clear_object_calls = 0;
static bool g_clear_object_result = true;
static int g_map_object_calls = 0;
static vm_user_region_t* g_last_map_region = NULL;
static vm_object_t* g_last_map_object = NULL;
static size_t g_last_map_object_offset = 0;
static bool g_map_object_result = true;
static int g_fault_object_calls = 0;
static vm_user_region_t* g_last_fault_region = NULL;
static vm_object_t* g_last_fault_object = NULL;
static size_t g_last_fault_object_offset = 0;
static bool g_fault_object_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_process_create_context_activate_and_reset(void);
static int test_process_map_and_fault_object_wrappers(void);
static int test_bind_user_regions_rolls_back_on_failure(void);
static int test_remove_region_restores_binding_when_unregister_fails(void);
static int test_process_reset_rejects_active_process(void);

uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space) {
    if (address_space == NULL || !address_space->allocated) {
        return 0;
    }

    return address_space->root_table_pa;
}

bool vm_address_space_enable(vm_address_space_t* address_space) {
    g_address_space_enable_calls += 1;
    g_last_enabled_address_space = address_space;
    return address_space != NULL && g_address_space_enable_result;
}

bool vm_address_space_user_region_init(vm_address_space_t* address_space,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags) {
    g_user_region_init_calls += 1;
    if (!g_user_region_init_result || address_space == NULL || region == NULL) {
        return false;
    }

    region->address_space = address_space;
    region->vaddr = vaddr;
    region->size = size;
    region->flags = flags;
    region->registered = true;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    return true;
}

bool vm_address_space_unregister_user_region_internal(
    vm_address_space_t* address_space,
    vm_user_region_t* region) {
    g_unregister_user_region_calls += 1;
    return g_unregister_user_region_result && address_space != NULL &&
           region != NULL;
}

bool vm_user_region_clear_object(vm_user_region_t* region) {
    g_clear_object_calls += 1;
    if (!g_clear_object_result || region == NULL) {
        return false;
    }

    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    return true;
}

bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size) {
    return region != NULL && region->registered &&
           range_within_window(vaddr,
                               size,
                               region->vaddr,
                               region->vaddr + (uintptr_t)region->size);
}

bool vm_user_region_map_object_at(vm_user_region_t* region,
                                  vm_object_t* object,
                                  size_t object_offset) {
    g_map_object_calls += 1;
    g_last_map_region = region;
    g_last_map_object = object;
    g_last_map_object_offset = object_offset;
    if (!g_map_object_result || region == NULL || object == NULL) {
        return false;
    }

    region->object = object;
    region->object_offset = object_offset;
    region->object_mode = VM_REGION_OBJECT_MAPPED;
    return true;
}

bool vm_user_region_set_fault_object_at(vm_user_region_t* region,
                                        vm_object_t* object,
                                        size_t object_offset) {
    g_fault_object_calls += 1;
    g_last_fault_region = region;
    g_last_fault_object = object;
    g_last_fault_object_offset = object_offset;
    if (!g_fault_object_result || region == NULL || object == NULL) {
        return false;
    }

    region->object = object;
    region->object_offset = object_offset;
    region->object_mode = VM_REGION_OBJECT_FAULT;
    return true;
}

void runtime_context_activate_process(vm_process_t* process) {
    g_active_process = process;
}

bool runtime_context_process_is_active(const vm_process_t* process) {
    return process != NULL && process == g_active_process;
}

void runtime_context_clear_process(const vm_process_t* process) {
    if (process == g_active_process) {
        g_active_process = NULL;
    }
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, VM_USER_VADDR_LIMIT);
}

uintptr_t vm_user_base(void) {
    return VM_USER_VADDR_BASE;
}

uintptr_t vm_user_limit(void) {
    return VM_USER_VADDR_LIMIT;
}

static void reset_stub_state(void) {
    g_active_process = NULL;
    g_address_space_enable_calls = 0;
    g_last_enabled_address_space = NULL;
    g_address_space_enable_result = true;
    g_user_region_init_calls = 0;
    g_user_region_init_result = true;
    g_unregister_user_region_calls = 0;
    g_unregister_user_region_result = true;
    g_clear_object_calls = 0;
    g_clear_object_result = true;
    g_map_object_calls = 0;
    g_last_map_region = NULL;
    g_last_map_object = NULL;
    g_last_map_object_offset = 0;
    g_map_object_result = true;
    g_fault_object_calls = 0;
    g_last_fault_region = NULL;
    g_last_fault_object = NULL;
    g_last_fault_object_offset = 0;
    g_fault_object_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_process_create_context_activate_and_reset(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table_pa = MEM_BASE,
    };
    vm_process_t process = {0};
    vm_user_region_t exec_region = {0};
    vm_user_region_t stack_region = {0};
    const uintptr_t entry_pc = MEMORY_PAGE_SIZE / 2U;
    const uintptr_t user_sp = 2U * MEMORY_PAGE_SIZE;

    reset_stub_state();
    if (!vm_process_create(&process, &address_space)) {
        return fail("expected process create to succeed");
    }

    if (process.address_space != &address_space || process.entry_pc != 0 ||
        process.user_sp != 0 || process.user_regions[0] != NULL) {
        return fail("expected process create to initialize process state");
    }

    if (!vm_process_user_region_init(&process,
                                     &exec_region,
                                     0,
                                     MEMORY_PAGE_SIZE,
                                     VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        !vm_process_user_region_init(&process,
                                     &stack_region,
                                     MEMORY_PAGE_SIZE,
                                     MEMORY_PAGE_SIZE,
                                     VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER)) {
        return fail("expected process to initialize user regions");
    }

    if (g_user_region_init_calls != 2 || process.user_regions[0] != &exec_region ||
        process.user_regions[1] != &stack_region) {
        return fail("expected process region init to occupy process slots");
    }

    if (!vm_process_set_user_context(&process, entry_pc, user_sp) ||
        !vm_process_is_runnable(&process)) {
        return fail("expected process user context to become runnable");
    }

    if (!vm_process_activate(&process) || !vm_process_is_active(&process) ||
        g_address_space_enable_calls != 1 ||
        g_last_enabled_address_space != &address_space) {
        return fail("expected process activate to enable address space and mark active");
    }

    runtime_context_clear_process(&process);
    if (!vm_process_reset(&process) || g_active_process != NULL ||
        process.address_space != NULL || process.entry_pc != 0 || process.user_sp != 0 ||
        process.user_regions[0] != NULL || process.user_regions[1] != NULL ||
        g_clear_object_calls != 2 || g_unregister_user_region_calls != 2 ||
        exec_region.registered || stack_region.registered) {
        return fail("expected process reset to clear regions and runtime state");
    }

    return 0;
}

static int test_process_map_and_fault_object_wrappers(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table_pa = MEM_BASE,
    };
    vm_process_t process = {0};
    vm_user_region_t mapped_region = {0};
    vm_user_region_t fault_region = {0};
    vm_object_t mapped_object = {.initialized = true};
    vm_object_t fault_object = {.initialized = true};

    reset_stub_state();
    if (!vm_process_create(&process, &address_space)) {
        return fail("expected process create before object binding");
    }

    if (!vm_process_map_object_region_at(&process,
                                         &mapped_region,
                                         0,
                                         MEMORY_PAGE_SIZE,
                                         VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER,
                                         &mapped_object,
                                         MEMORY_PAGE_SIZE)) {
        return fail("expected mapped object wrapper to succeed");
    }

    if (g_map_object_calls != 1 || g_last_map_region != &mapped_region ||
        g_last_map_object != &mapped_object ||
        g_last_map_object_offset != MEMORY_PAGE_SIZE ||
        mapped_region.object_mode != VM_REGION_OBJECT_MAPPED) {
        return fail("expected mapped object wrapper to forward mapping contract");
    }

    if (!vm_process_remove_user_region(&process, &mapped_region)) {
        return fail("expected mapped region removal to succeed");
    }

    if (!vm_process_set_fault_object_region_at(&process,
                                               &fault_region,
                                               MEMORY_PAGE_SIZE,
                                               MEMORY_PAGE_SIZE,
                                               VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
                                               &fault_object,
                                               2U * MEMORY_PAGE_SIZE)) {
        return fail("expected fault object wrapper to succeed");
    }

    if (g_fault_object_calls != 1 || g_last_fault_region != &fault_region ||
        g_last_fault_object != &fault_object ||
        g_last_fault_object_offset != 2U * MEMORY_PAGE_SIZE ||
        fault_region.object_mode != VM_REGION_OBJECT_FAULT) {
        return fail("expected fault object wrapper to forward fault-binding contract");
    }

    return 0;
}

static int test_bind_user_regions_rolls_back_on_failure(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table_pa = MEM_BASE,
    };
    vm_process_t process = {0};
    vm_user_region_t region_a = {0};
    vm_user_region_t region_b = {0};
    vm_object_t object_a = {.initialized = true};
    vm_object_t object_b = {.initialized = true};
    const vm_process_user_region_binding_t bindings[2] = {
        {
            .region = &region_a,
            .vaddr = 0,
            .size = MEMORY_PAGE_SIZE,
            .flags = VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER,
            .object = &object_a,
            .object_offset = 0,
            .object_mode = VM_REGION_OBJECT_MAPPED,
        },
        {
            .region = &region_b,
            .vaddr = MEMORY_PAGE_SIZE,
            .size = MEMORY_PAGE_SIZE,
            .flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
            .object = &object_b,
            .object_offset = 0,
            .object_mode = VM_REGION_OBJECT_FAULT,
        },
    };

    reset_stub_state();
    if (!vm_process_create(&process, &address_space)) {
        return fail("expected process create before bind_user_regions");
    }

    g_fault_object_result = false;
    if (vm_process_bind_user_regions(&process, bindings, 2U)) {
        return fail("expected bind_user_regions failure to propagate");
    }

    if (process.user_regions[0] != NULL || process.user_regions[1] != NULL ||
        region_a.registered || region_b.registered ||
        g_map_object_calls != 1 || g_fault_object_calls != 1 ||
        g_clear_object_calls != 2 || g_unregister_user_region_calls != 2) {
        return fail("expected bind_user_regions failure to roll back all bound regions");
    }

    return 0;
}

static int test_remove_region_restores_binding_when_unregister_fails(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table_pa = MEM_BASE,
    };
    vm_process_t process = {0};
    vm_user_region_t region = {
        .vaddr = 0,
        .size = MEMORY_PAGE_SIZE,
        .flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
        .registered = true,
    };
    vm_object_t object = {.initialized = true};

    reset_stub_state();
    if (!vm_process_create(&process, &address_space)) {
        return fail("expected process create before remove rollback to succeed");
    }

    region.address_space = &address_space;
    region.object = &object;
    region.object_mode = VM_REGION_OBJECT_FAULT;
    process.user_regions[0] = &region;

    g_unregister_user_region_result = false;
    if (vm_process_remove_user_region(&process, &region)) {
        return fail("expected remove to fail when unregister fails");
    }

    if (process.user_regions[0] != &region || region.address_space != &address_space ||
        region.object != &object || region.object_mode != VM_REGION_OBJECT_FAULT ||
        g_clear_object_calls != 1 || g_fault_object_calls != 1) {
        return fail("expected remove rollback to preserve region binding after unregister failure");
    }

    return 0;
}

static int test_process_reset_rejects_active_process(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table_pa = MEM_BASE,
    };
    vm_process_t process = {0};
    vm_user_region_t exec_region = {0};
    vm_user_region_t stack_region = {0};

    reset_stub_state();
    if (!vm_process_create(&process, &address_space) ||
        !vm_process_user_region_init(&process,
                                     &exec_region,
                                     0,
                                     MEMORY_PAGE_SIZE,
                                     VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        !vm_process_user_region_init(&process,
                                     &stack_region,
                                     MEMORY_PAGE_SIZE,
                                     MEMORY_PAGE_SIZE,
                                     VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER) ||
        !vm_process_set_user_context(&process, MEMORY_PAGE_SIZE / 2U, 2U * MEMORY_PAGE_SIZE) ||
        !vm_process_activate(&process)) {
        return fail("expected process create and activate before reset rejection");
    }

    if (vm_process_reset(&process)) {
        return fail("expected reset to reject active process");
    }

    if (g_active_process != &process || process.address_space != &address_space) {
        return fail("expected reset rejection to preserve active process state");
    }

    return 0;
}

int main(void) {
    if (test_process_create_context_activate_and_reset() != 0 ||
        test_process_map_and_fault_object_wrappers() != 0 ||
        test_bind_user_regions_rolls_back_on_failure() != 0 ||
        test_remove_region_restores_binding_when_unregister_fails() != 0 ||
        test_process_reset_rejects_active_process() != 0) {
        return 1;
    }

    return 0;
}

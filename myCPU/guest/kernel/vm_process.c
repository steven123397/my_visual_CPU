#include "vm.h"

#include <stddef.h>
#include <stdint.h>

#include "runtime_context.h"
#include "vm_private.h"

static vm_user_region_t** find_free_process_region_slot(vm_process_t* process) {
    size_t i = 0;

    if (process == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == NULL) {
            return &process->user_regions[i];
        }
    }

    return NULL;
}

static vm_user_region_t** find_process_region_slot(vm_process_t* process,
                                                   const vm_user_region_t* region) {
    size_t i = 0;

    if (process == NULL || region == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == region) {
            return &process->user_regions[i];
        }
    }

    return NULL;
}

static bool process_owns_region(const vm_process_t* process,
                                const vm_user_region_t* region) {
    size_t i = 0;

    if (process == NULL || region == NULL) {
        return false;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == region) {
            return true;
        }
    }

    return false;
}

static const vm_user_region_t* find_process_region_containing(
    const vm_process_t* process,
    uintptr_t vaddr,
    size_t size,
    uint64_t required_flags) {
    size_t i = 0;

    if (process == NULL || process->address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        const vm_user_region_t* region = process->user_regions[i];

        if (region == NULL || region->address_space != process->address_space ||
            (region->flags & required_flags) != required_flags ||
            !vm_user_region_contains(region, vaddr, size)) {
            continue;
        }

        return region;
    }

    return NULL;
}

static void clear_region_descriptor(vm_user_region_t* region) {
    if (region == NULL) {
        return;
    }

    region->address_space = NULL;
    region->vaddr = 0;
    region->size = 0;
    region->flags = 0;
    region->registered = false;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
}

static bool process_bind_object_region(vm_process_t* process,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object,
                                       size_t object_offset,
                                       bool map_now) {
    bool ok = false;

    if (process == NULL || region == NULL || object == NULL ||
        !vm_process_user_region_init(process, region, vaddr, size, flags)) {
        return false;
    }

    if (map_now) {
        ok = vm_user_region_map_object_at(region, object, object_offset);
    } else {
        ok = vm_user_region_set_fault_object_at(region, object, object_offset);
    }

    if (ok) {
        return true;
    }

    if (!vm_process_remove_user_region(process, region)) {
        return false;
    }

    return false;
}

bool vm_process_create(vm_process_t* process, vm_address_space_t* address_space) {
    size_t i = 0;

    if (process == NULL || address_space == NULL ||
        vm_address_space_root_table(address_space) == 0 ||
        process->address_space != NULL || process->entry_pc != 0 ||
        process->user_sp != 0) {
        return false;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] != NULL) {
            return false;
        }
    }

    process->address_space = address_space;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }

    return true;
}

bool vm_process_activate(vm_process_t* process) {
    if (!vm_process_is_runnable(process) ||
        !vm_address_space_enable(process->address_space)) {
        return false;
    }

    runtime_context_activate_process(process);
    return true;
}

bool vm_process_is_active(const vm_process_t* process) {
    return runtime_context_process_is_active(process);
}

bool vm_process_remove_user_region(vm_process_t* process,
                                   vm_user_region_t* region) {
    vm_user_region_t** process_slot = NULL;

    if (process == NULL || process->address_space == NULL || region == NULL ||
        region->address_space != process->address_space ||
        !vm_user_region_contains(region, region->vaddr, 1U)) {
        return false;
    }

    process_slot = find_process_region_slot(process, region);
    if (process_slot == NULL || !vm_user_region_clear_object(region) ||
        !vm_address_space_unregister_user_region_internal(process->address_space,
                                                          region)) {
        return false;
    }

    *process_slot = NULL;
    if (vm_user_region_contains(region, process->entry_pc, 1U)) {
        process->entry_pc = 0;
    }
    if (process->user_sp > 0 &&
        vm_user_region_contains(region, process->user_sp - 1U, 1U)) {
        process->user_sp = 0;
    }
    clear_region_descriptor(region);
    return true;
}

bool vm_process_reset(vm_process_t* process) {
    size_t i = 0;

    if (process == NULL) {
        return false;
    }

    while (i < VM_PROCESS_MAX_USER_REGIONS) {
        vm_user_region_t* region = process->user_regions[i];

        if (region != NULL) {
            if (!vm_process_remove_user_region(process, region)) {
                return false;
            }
        } else {
            ++i;
        }
    }

    runtime_context_clear_process(process);
    process->address_space = NULL;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }

    return true;
}

bool vm_process_user_region_init(vm_process_t* process,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags) {
    vm_user_region_t** slot = NULL;

    if (process == NULL || process->address_space == NULL || region == NULL ||
        process_owns_region(process, region)) {
        return false;
    }

    slot = find_free_process_region_slot(process);
    if (slot == NULL ||
        !vm_address_space_user_region_init(process->address_space,
                                           region,
                                           vaddr,
                                           size,
                                           flags)) {
        return false;
    }

    *slot = region;
    return true;
}

bool vm_process_bind_user_regions(
    vm_process_t* process,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count) {
    vm_user_region_t* bound_regions[VM_PROCESS_MAX_USER_REGIONS];
    size_t i = 0;
    size_t bound_count = 0;
    bool ok = false;

    if (process == NULL ||
        (binding_count != 0 && bindings == NULL) ||
        binding_count > VM_PROCESS_MAX_USER_REGIONS) {
        return false;
    }

    while (i < binding_count) {
        const vm_process_user_region_binding_t* binding = &bindings[i];

        if (binding->region == NULL || binding->object == NULL) {
            ok = false;
            break;
        }

        switch (binding->object_mode) {
        case VM_REGION_OBJECT_MAPPED:
            ok = process_bind_object_region(process,
                                            binding->region,
                                            binding->vaddr,
                                            binding->size,
                                            binding->flags,
                                            binding->object,
                                            binding->object_offset,
                                            true);
            break;
        case VM_REGION_OBJECT_FAULT:
            ok = process_bind_object_region(process,
                                            binding->region,
                                            binding->vaddr,
                                            binding->size,
                                            binding->flags,
                                            binding->object,
                                            binding->object_offset,
                                            false);
            break;
        default:
            ok = false;
            break;
        }

        if (!ok) {
            break;
        }

        bound_regions[bound_count++] = binding->region;
        ++i;
    }

    if (i == binding_count) {
        return true;
    }

    while (bound_count > 0) {
        bound_count -= 1;
        if (!vm_process_remove_user_region(process, bound_regions[bound_count])) {
            return false;
        }
    }

    return false;
}

bool vm_process_map_object_region_at(vm_process_t* process,
                                     vm_user_region_t* region,
                                     uintptr_t vaddr,
                                     size_t size,
                                     uint64_t flags,
                                     vm_object_t* object,
                                     size_t object_offset) {
    return process_bind_object_region(process,
                                      region,
                                      vaddr,
                                      size,
                                      flags,
                                      object,
                                      object_offset,
                                      true);
}

bool vm_process_map_object_region(vm_process_t* process,
                                  vm_user_region_t* region,
                                  uintptr_t vaddr,
                                  size_t size,
                                  uint64_t flags,
                                  vm_object_t* object) {
    return vm_process_map_object_region_at(process,
                                           region,
                                           vaddr,
                                           size,
                                           flags,
                                           object,
                                           0);
}

bool vm_process_set_fault_object_region_at(vm_process_t* process,
                                           vm_user_region_t* region,
                                           uintptr_t vaddr,
                                           size_t size,
                                           uint64_t flags,
                                           vm_object_t* object,
                                           size_t object_offset) {
    return process_bind_object_region(process,
                                      region,
                                      vaddr,
                                      size,
                                      flags,
                                      object,
                                      object_offset,
                                      false);
}

bool vm_process_set_fault_object_region(vm_process_t* process,
                                        vm_user_region_t* region,
                                        uintptr_t vaddr,
                                        size_t size,
                                        uint64_t flags,
                                        vm_object_t* object) {
    return vm_process_set_fault_object_region_at(process,
                                                 region,
                                                 vaddr,
                                                 size,
                                                 flags,
                                                 object,
                                                 0);
}

bool vm_process_set_user_context(vm_process_t* process,
                                 uintptr_t entry_pc,
                                 uintptr_t user_sp) {
    if (process == NULL || process->address_space == NULL ||
        vm_address_space_root_table(process->address_space) == 0 ||
        !vm_range_is_user(entry_pc, 1U) || user_sp <= vm_user_base() ||
        user_sp > vm_user_limit() ||
        find_process_region_containing(process,
                                       entry_pc,
                                       1U,
                                       VM_PAGE_EXEC | VM_PAGE_USER) == NULL ||
        find_process_region_containing(process,
                                       user_sp - 1U,
                                       1U,
                                       VM_PAGE_WRITE | VM_PAGE_USER) == NULL) {
        return false;
    }

    process->entry_pc = entry_pc;
    process->user_sp = user_sp;
    return true;
}

bool vm_process_is_runnable(const vm_process_t* process) {
    return process != NULL && process->address_space != NULL &&
           vm_address_space_root_table(process->address_space) != 0 &&
           find_process_region_containing(process,
                                          process->entry_pc,
                                          1U,
                                          VM_PAGE_EXEC | VM_PAGE_USER) != NULL &&
           process->user_sp > vm_user_base() &&
           process->user_sp <= vm_user_limit() &&
           find_process_region_containing(process,
                                          process->user_sp - 1U,
                                          1U,
                                          VM_PAGE_WRITE | VM_PAGE_USER) != NULL;
}

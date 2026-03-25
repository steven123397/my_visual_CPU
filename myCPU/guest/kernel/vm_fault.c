#include "vm.h"

#include <stddef.h>
#include <stdint.h>

#include "riscv.h"
#include "vm_private.h"

static const struct VmFaultRange* find_kernel_fault_range(
    const vm_address_space_t* address_space,
    uintptr_t fault_page) {
    size_t i = 0;

    if (address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_KERNEL_FAULT_RANGES; ++i) {
        const struct VmFaultRange* range = &address_space->kernel_fault_ranges[i];

        if (!range->valid) {
            continue;
        }

        if (fault_page >= range->vaddr &&
            fault_page < range->vaddr + (uintptr_t)range->size) {
            return range;
        }
    }

    return NULL;
}

static const struct VmFaultActionRule* find_fault_action(
    const vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t fault_addr) {
    size_t i = 0;

    if (address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        const struct VmFaultActionRule* rule = &address_space->fault_actions[i];

        if (!rule->valid || rule->cause != cause) {
            continue;
        }

        if (fault_addr >= rule->vaddr &&
            fault_addr < rule->vaddr + (uintptr_t)rule->size) {
            return rule;
        }
    }

    return NULL;
}

static bool register_fault_action(vm_address_space_t* address_space,
                                  uint64_t cause,
                                  uintptr_t vaddr,
                                  size_t size,
                                  vm_fault_action_t action,
                                  volatile uintptr_t* resume_pc_slot) {
    size_t i = 0;
    struct VmFaultActionRule* free_slot = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        !page_fault_cause_valid(cause) || !span_args_valid(vaddr, size)) {
        return false;
    }

    if (action == VM_FAULT_ACTION_RESUME_AT_SLOT && resume_pc_slot == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        struct VmFaultActionRule* rule = &address_space->fault_actions[i];

        if (!rule->valid) {
            if (free_slot == NULL) {
                free_slot = rule;
            }
            continue;
        }

        if (rule->cause == cause &&
            ranges_overlap(vaddr, size, rule->vaddr, rule->size)) {
            return false;
        }
    }

    if (free_slot == NULL) {
        return false;
    }

    free_slot->valid = true;
    free_slot->cause = cause;
    free_slot->vaddr = vaddr;
    free_slot->size = size;
    free_slot->action = action;
    free_slot->resume_pc_slot = resume_pc_slot;
    return true;
}

bool vm_address_space_register_fault_skip(vm_address_space_t* address_space,
                                          uint64_t cause,
                                          uintptr_t vaddr,
                                          size_t size) {
    return register_fault_action(address_space,
                                 cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_SKIP_INSTRUCTION,
                                 NULL);
}

bool vm_address_space_register_fault_resume_slot(
    vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t vaddr,
    size_t size,
    volatile uintptr_t* resume_pc_slot) {
    return register_fault_action(address_space,
                                 cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_RESUME_AT_SLOT,
                                 resume_pc_slot);
}

bool vm_handle_page_fault(vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval) {
    vm_user_region_t* user_region = NULL;
    const struct VmFaultRange* range = NULL;
    const struct VmFaultActionRule* action = NULL;
    const uintptr_t fault_page = align_down_page((uintptr_t)tval);
    const uintptr_t fault_addr = (uintptr_t)tval;
    uintptr_t offset = 0;
    uintptr_t paddr = 0;
    uintptr_t resume_pc = 0;

    if (address_space == NULL || address_space->root_table == NULL) {
        return false;
    }

    if (process != NULL && process->address_space == address_space) {
        size_t i = 0;

        for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
            vm_user_region_t* region = process->user_regions[i];

            if (region == NULL || region->address_space != address_space ||
                !vm_user_region_contains(region, fault_page, 1U)) {
                continue;
            }

            user_region = region;
            break;
        }
    }

    if (user_region != NULL &&
        user_region->object != NULL &&
        user_region->object_mode != VM_REGION_OBJECT_NONE &&
        object_descriptor_valid(user_region->object) &&
        fault_range_allows_access(user_region->flags, cause)) {
        offset = fault_page - user_region->vaddr;
        if (!object_resolve_page(user_region->object,
                                 user_region->object_offset + offset,
                                 true,
                                 &paddr) ||
            !map_page_internal(address_space,
                               fault_page,
                               paddr,
                               user_region->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    range = find_kernel_fault_range(address_space, fault_page);
    if (range != NULL && fault_range_allows_access(range->flags, cause)) {
        offset = fault_page - range->vaddr;
        if (!map_page_internal(address_space,
                               fault_page,
                               range->paddr + offset,
                               range->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    action = find_fault_action(address_space, cause, fault_addr);
    if (action == NULL) {
        return false;
    }

    switch (action->action) {
    case VM_FAULT_ACTION_SKIP_INSTRUCTION:
        riscv_write_sepc(epc + 4U);
        return true;
    case VM_FAULT_ACTION_RESUME_AT_SLOT:
        resume_pc = (uintptr_t)(*action->resume_pc_slot);
        if (resume_pc == 0) {
            return false;
        }
        riscv_write_sepc(resume_pc);
        *action->resume_pc_slot = 0;
        return true;
    }

    return false;
}

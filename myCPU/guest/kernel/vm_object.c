#include "vm.h"

#include <stddef.h>
#include <stdint.h>

#include "pmm.h"
#include "vm_private.h"

static void clear_object_descriptor(vm_object_t* object) {
    if (object == NULL) {
        return;
    }

    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = NULL;
    object->backing.anon.page_count = 0;
}

bool object_resolve_page(vm_object_t* object,
                         size_t offset,
                         bool create,
                         uintptr_t* out_paddr) {
    size_t page_index = 0;
    void* page = NULL;

    if (!object_descriptor_valid(object) || out_paddr == NULL ||
        offset >= object->size ||
        (offset & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        *out_paddr = object->backing.physical.base_paddr + (uintptr_t)offset;
        return true;
    case VM_OBJECT_BACKING_ANON:
        page_index = offset / MEMORY_PAGE_SIZE;
        if (page_index >= object->backing.anon.page_count) {
            return false;
        }

        if (object->backing.anon.page_slots[page_index] == 0) {
            if (!create) {
                return false;
            }

            page = alloc_zeroed_page();
            if (page == NULL) {
                return false;
            }

            object->backing.anon.page_slots[page_index] = (uintptr_t)page;
        }

        if ((object->backing.anon.page_slots[page_index] &
             (MEMORY_PAGE_SIZE - 1U)) != 0) {
            return false;
        }

        *out_paddr = object->backing.anon.page_slots[page_index];
        return true;
    default:
        return false;
    }
}

bool region_object_compatible(const vm_user_region_t* region,
                              const vm_object_t* object,
                              size_t object_offset) {
    if (!user_region_descriptor_valid(region) ||
        !object_range_compatible(object, object_offset, region->size)) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        return mapped_range_args_valid(region->vaddr,
                                       object->backing.physical.base_paddr +
                                           (uintptr_t)object_offset,
                                       region->size);
    case VM_OBJECT_BACKING_ANON:
        return true;
    default:
        return false;
    }
}

bool clear_region_page_mappings(vm_user_region_t* region, bool* changed) {
    uintptr_t offset = 0;

    if (changed != NULL) {
        *changed = false;
    }

    if (!user_region_descriptor_valid(region)) {
        return false;
    }

    while (offset < region->size) {
        const uintptr_t vaddr = region->vaddr + offset;

        if (!can_map_page(region->address_space, vaddr)) {
            if (!unmap_page_internal(region->address_space, vaddr)) {
                return false;
            }

            if (changed != NULL) {
                *changed = true;
            }
        }

        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

bool map_region_object_pages(vm_user_region_t* region,
                             vm_object_t* object,
                             size_t object_offset) {
    size_t offset = 0;
    size_t mapped = 0;
    uintptr_t paddr = 0;

    if (!region_object_compatible(region, object, object_offset)) {
        return false;
    }

    while (offset < region->size) {
        if (!can_map_page(region->address_space, region->vaddr + offset)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }

    offset = 0;
    while (offset < region->size) {
        if (!object_resolve_page(object, object_offset + offset, true, &paddr) ||
            !map_page_internal(region->address_space,
                               region->vaddr + offset,
                               paddr,
                               region->flags)) {
            size_t rollback = 0;

            while (rollback < mapped) {
                unmap_page_internal(region->address_space, region->vaddr + rollback);
                rollback += MEMORY_PAGE_SIZE;
            }
            return false;
        }

        mapped += MEMORY_PAGE_SIZE;
        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

bool vm_object_reset(vm_object_t* object) {
    size_t i = 0;

    if (object == NULL) {
        return false;
    }

    if (!object->initialized) {
        clear_object_descriptor(object);
        return true;
    }

    if (!object_descriptor_valid(object) || object->attachment_count != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        clear_object_descriptor(object);
        return true;
    case VM_OBJECT_BACKING_ANON:
        for (i = 0; i < object->backing.anon.page_count; ++i) {
            const uintptr_t page = object->backing.anon.page_slots[i];

            if (page == 0) {
                continue;
            }

            if (!pmm_free_page((void*)page)) {
                return false;
            }

            object->backing.anon.page_slots[i] = 0;
        }

        if (!pmm_free_page(object->backing.anon.page_slots)) {
            return false;
        }

        clear_object_descriptor(object);
        return true;
    default:
        return false;
    }
}

bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size) {
    if (object == NULL || object->initialized || object->attachment_count != 0 ||
        size == 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        !span_args_valid(paddr, size)) {
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
    uintptr_t* page_slots = NULL;
    const size_t page_count = object_page_count(size);

    if (object == NULL || object->initialized || object->attachment_count != 0 ||
        size == 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        page_count == 0 ||
        page_count > VM_OBJECT_ANON_PAGE_SLOTS) {
        return false;
    }

    page_slots = (uintptr_t*)alloc_zeroed_page();
    if (page_slots == NULL) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_ANON;
    object->size = size;
    object->attachment_count = 0;
    object->backing.anon.page_slots = page_slots;
    object->backing.anon.page_count = page_count;
    return true;
}

bool vm_user_region_clear_object(vm_user_region_t* region) {
    bool changed = false;
    vm_object_t* object = NULL;

    if (!clear_region_page_mappings(region, &changed)) {
        return false;
    }

    object = region->object;
    if (object != NULL) {
        if (!object_descriptor_valid(object) || object->attachment_count == 0) {
            return false;
        }
        object->attachment_count -= 1;
    }

    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    if (changed) {
        flush_tlb_if_enabled();
    }

    return true;
}

static bool bind_region_object(vm_user_region_t* region,
                               vm_object_t* object,
                               size_t object_offset,
                               vm_region_object_mode_t object_mode) {
    const bool map_now = object_mode == VM_REGION_OBJECT_MAPPED;

    if (!region_object_compatible(region, object, object_offset) ||
        region->object != NULL ||
        region->object_mode != VM_REGION_OBJECT_NONE ||
        object->attachment_count == (size_t)-1) {
        return false;
    }

    if (map_now && !map_region_object_pages(region, object, object_offset)) {
        return false;
    }

    object->attachment_count += 1;
    region->object = object;
    region->object_offset = object_offset;
    region->object_mode = object_mode;
    if (map_now) {
        flush_tlb_if_enabled();
    }
    return true;
}

bool vm_user_region_map_object_at(vm_user_region_t* region,
                                  vm_object_t* object,
                                  size_t object_offset) {
    return bind_region_object(region,
                              object,
                              object_offset,
                              VM_REGION_OBJECT_MAPPED);
}

bool vm_user_region_map_object(vm_user_region_t* region, vm_object_t* object) {
    return vm_user_region_map_object_at(region, object, 0);
}

bool vm_user_region_set_fault_object_at(vm_user_region_t* region,
                                        vm_object_t* object,
                                        size_t object_offset) {
    return bind_region_object(region,
                              object,
                              object_offset,
                              VM_REGION_OBJECT_FAULT);
}

bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object) {
    return vm_user_region_set_fault_object_at(region, object, 0);
}

bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr) {
    if (!user_region_descriptor_valid(region) ||
        !vm_user_region_contains(region, vaddr, MEMORY_PAGE_SIZE) ||
        !unmap_page_internal(region->address_space, vaddr)) {
        return false;
    }

    flush_tlb_if_enabled();
    return true;
}

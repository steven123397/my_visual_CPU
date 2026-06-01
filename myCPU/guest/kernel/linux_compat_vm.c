#include "linux_compat_vm.h"

#include "memory.h"

static uintptr_t align_up_page_uintptr(uintptr_t value) {
    const uintptr_t mask = (uintptr_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
}

static size_t align_up_page_size(size_t value) {
    const size_t mask = (size_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
}

static bool vm_ready(const linux_compat_vm_t* vm) {
    return vm != 0 && vm->address_space != 0 && vm->process != 0;
}

static uint64_t prot_to_vm_flags(uint32_t prot) {
    uint64_t flags = VM_PAGE_USER;

    if ((prot & LINUX_COMPAT_PROT_READ) != 0U) {
        flags |= VM_PAGE_READ;
    }
    if ((prot & LINUX_COMPAT_PROT_WRITE) != 0U) {
        flags |= VM_PAGE_READ | VM_PAGE_WRITE;
    }
    if ((prot & LINUX_COMPAT_PROT_EXEC) != 0U) {
        flags |= VM_PAGE_EXEC;
    }
    return flags;
}

static void clear_region_slot(linux_compat_vm_region_t* slot) {
    if (slot == 0) {
        return;
    }
    slot->used = false;
    slot->heap = false;
    slot->region.address_space = 0;
    slot->region.vaddr = 0;
    slot->region.size = 0;
    slot->region.flags = 0;
    slot->region.registered = false;
    slot->region.object = 0;
    slot->region.object_offset = 0;
    slot->region.object_mode = VM_REGION_OBJECT_NONE;
    slot->object.initialized = false;
    slot->object.backing_kind = VM_OBJECT_BACKING_NONE;
    slot->object.size = 0;
    slot->object.attachment_count = 0;
    slot->object.backing.anon.page_slots = 0;
    slot->object.backing.anon.page_count = 0;
    slot->vaddr = 0;
    slot->length = 0;
    slot->prot = 0;
    slot->flags = 0;
}

static linux_compat_vm_region_t* find_heap_region(linux_compat_vm_t* vm) {
    size_t i = 0;

    if (vm == 0) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (vm->regions[i].used && vm->regions[i].heap) {
            return &vm->regions[i];
        }
    }
    return 0;
}

static linux_compat_vm_region_t* find_free_region(linux_compat_vm_t* vm) {
    size_t i = 0;

    if (vm == 0) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (!vm->regions[i].used) {
            return &vm->regions[i];
        }
    }
    return 0;
}

static linux_compat_vm_region_t* find_exact_region(linux_compat_vm_t* vm,
                                                   uintptr_t addr,
                                                   size_t length,
                                                   bool heap) {
    size_t i = 0;

    if (vm == 0) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        linux_compat_vm_region_t* slot = &vm->regions[i];

        if (slot->used && slot->heap == heap && slot->vaddr == addr &&
            slot->length == length) {
            return slot;
        }
    }
    return 0;
}

static linux_compat_vm_region_t* find_region_containing(linux_compat_vm_t* vm,
                                                        uintptr_t addr,
                                                        size_t length,
                                                        uint32_t required_prot) {
    size_t i = 0;

    if (vm == 0 || length == 0U || addr > UINTPTR_MAX - (uintptr_t)length) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        linux_compat_vm_region_t* slot = &vm->regions[i];

        if (!slot->used ||
            (slot->prot & required_prot) != required_prot ||
            addr < slot->vaddr ||
            addr + (uintptr_t)length > slot->vaddr + (uintptr_t)slot->length) {
            continue;
        }
        return slot;
    }
    return 0;
}

static bool release_region(linux_compat_vm_t* vm,
                           linux_compat_vm_region_t* slot) {
    bool ok = true;

    if (!vm_ready(vm) || slot == 0 || !slot->used) {
        return false;
    }

    if (slot->region.registered) {
        ok = vm_process_remove_user_region(vm->process, &slot->region) && ok;
    }
    if (slot->object.initialized) {
        ok = vm_object_reset(&slot->object) && ok;
    }
    clear_region_slot(slot);
    return ok;
}

static bool map_region(linux_compat_vm_t* vm,
                       linux_compat_vm_region_t* slot,
                       uintptr_t vaddr,
                       size_t length,
                       uint32_t prot,
                       uint32_t flags,
                       bool heap) {
    const uint64_t vm_flags = prot_to_vm_flags(prot);

    if (!vm_ready(vm) || slot == 0 || slot->used ||
        (vaddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        length == 0 || (length & ((size_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        (vm_flags & (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC)) == 0U ||
        !vm_object_init_anon(&slot->object, length)) {
        return false;
    }

    if (!vm_process_map_object_region_at(vm->process,
                                         &slot->region,
                                         vaddr,
                                         length,
                                         vm_flags,
                                         &slot->object,
                                         0U)) {
        (void)vm_object_reset(&slot->object);
        clear_region_slot(slot);
        return false;
    }

    slot->used = true;
    slot->heap = heap;
    slot->vaddr = vaddr;
    slot->length = length;
    slot->prot = prot;
    slot->flags = flags;
    return true;
}

void linux_compat_vm_init(linux_compat_vm_t* vm,
                          vm_address_space_t* address_space,
                          vm_process_t* process) {
    size_t i = 0;

    if (vm == 0) {
        return;
    }
    vm->address_space = address_space;
    vm->process = process;
    vm->brk_base = LINUX_COMPAT_BRK_BASE;
    vm->program_break = LINUX_COMPAT_BRK_BASE;
    vm->next_mmap = LINUX_COMPAT_MMAP_BASE;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        clear_region_slot(&vm->regions[i]);
    }
}

uintptr_t linux_compat_vm_brk(linux_compat_vm_t* vm, uintptr_t new_break) {
    linux_compat_vm_region_t* heap = 0;
    linux_compat_vm_region_t* slot = 0;
    const uintptr_t old_break = vm != 0 ? vm->program_break : 0U;
    size_t new_length = 0;

    if (!vm_ready(vm)) {
        return 0;
    }
    if (new_break == 0U) {
        return vm->program_break;
    }
    if (new_break < vm->brk_base || new_break >= vm_user_limit()) {
        return vm->program_break;
    }

    new_length = (size_t)align_up_page_uintptr(new_break - vm->brk_base);
    heap = find_heap_region(vm);
    if (heap != 0 && heap->length == new_length) {
        vm->program_break = new_break;
        return vm->program_break;
    }
    if (heap != 0 && !release_region(vm, heap)) {
        return old_break;
    }
    if (new_length == 0U) {
        vm->program_break = new_break;
        return vm->program_break;
    }

    slot = find_free_region(vm);
    if (!map_region(vm,
                    slot,
                    vm->brk_base,
                    new_length,
                    LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE,
                    0U,
                    true)) {
        vm->program_break = old_break;
        return old_break;
    }

    vm->program_break = new_break;
    return vm->program_break;
}

uintptr_t linux_compat_vm_mmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length,
                               uint32_t prot,
                               uint32_t flags) {
    linux_compat_vm_region_t* slot = 0;
    uintptr_t vaddr = addr;
    size_t mapped_length = 0;

    if (!vm_ready(vm) || length == 0U) {
        return (uintptr_t)-22;
    }

    mapped_length = align_up_page_size(length);
    if (vaddr == 0U) {
        vaddr = vm->next_mmap;
    }
    if ((vaddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        vaddr >= vm_user_limit() ||
        mapped_length > (size_t)(vm_user_limit() - vaddr)) {
        return (uintptr_t)-22;
    }

    slot = find_free_region(vm);
    if (!map_region(vm, slot, vaddr, mapped_length, prot, flags, false)) {
        return (uintptr_t)-12;
    }
    if (addr == 0U) {
        vm->next_mmap = vaddr + (uintptr_t)mapped_length;
    }
    return vaddr;
}

linux_compat_vm_region_t* linux_compat_vm_map_fixed(linux_compat_vm_t* vm,
                                                    uintptr_t addr,
                                                    size_t length,
                                                    uint32_t prot,
                                                    uint32_t flags) {
    linux_compat_vm_region_t* slot = 0;
    const size_t mapped_length = align_up_page_size(length);

    if (!vm_ready(vm) || addr == 0U || length == 0U ||
        (addr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        addr >= vm_user_limit() ||
        mapped_length > (size_t)(vm_user_limit() - addr)) {
        return 0;
    }

    slot = find_free_region(vm);
    if (!map_region(vm, slot, addr, mapped_length, prot, flags, false)) {
        return 0;
    }
    return slot;
}

int32_t linux_compat_vm_munmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length) {
    linux_compat_vm_region_t* slot = 0;
    size_t mapped_length = 0;

    if (!vm_ready(vm) || length == 0U ||
        (addr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U) {
        return -22;
    }
    mapped_length = align_up_page_size(length);
    slot = find_exact_region(vm, addr, mapped_length, false);
    if (slot == 0) {
        return -22;
    }
    return release_region(vm, slot) ? 0 : -22;
}

bool linux_compat_vm_read_user(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               void* out,
                               size_t length) {
    uint8_t* bytes = (uint8_t*)out;
    size_t copied = 0;

    if (!vm_ready(vm) || (out == 0 && length != 0U)) {
        return false;
    }
    while (copied < length) {
        const uintptr_t current = addr + (uintptr_t)copied;
        const uintptr_t page_offset = current & ((uintptr_t)MEMORY_PAGE_SIZE - 1U);
        const size_t chunk =
            (length - copied) < (MEMORY_PAGE_SIZE - page_offset)
                ? (length - copied)
                : (MEMORY_PAGE_SIZE - page_offset);
        linux_compat_vm_region_t* slot =
            find_region_containing(vm, current, chunk, LINUX_COMPAT_PROT_READ);
        uintptr_t page = 0;
        size_t object_offset = 0;
        size_t i = 0;

        if (slot == 0) {
            return false;
        }
        object_offset = (size_t)(current - slot->vaddr);
        if (!vm_object_resolve_page_for_write(&slot->object,
                                              object_offset - (size_t)page_offset,
                                              &page)) {
            return false;
        }
        for (i = 0; i < chunk; ++i) {
            bytes[copied + i] = ((const uint8_t*)page)[page_offset + i];
        }
        copied += chunk;
    }
    return true;
}

bool linux_compat_vm_write_user(linux_compat_vm_t* vm,
                                uintptr_t addr,
                                const void* data,
                                size_t length) {
    const uint8_t* bytes = (const uint8_t*)data;
    size_t copied = 0;

    if (!vm_ready(vm) || (data == 0 && length != 0U)) {
        return false;
    }
    while (copied < length) {
        const uintptr_t current = addr + (uintptr_t)copied;
        const uintptr_t page_offset = current & ((uintptr_t)MEMORY_PAGE_SIZE - 1U);
        const size_t chunk =
            (length - copied) < (MEMORY_PAGE_SIZE - page_offset)
                ? (length - copied)
                : (MEMORY_PAGE_SIZE - page_offset);
        linux_compat_vm_region_t* slot =
            find_region_containing(vm, current, chunk, LINUX_COMPAT_PROT_WRITE);
        uintptr_t page = 0;
        size_t object_offset = 0;
        size_t i = 0;

        if (slot == 0) {
            return false;
        }
        object_offset = (size_t)(current - slot->vaddr);
        if (!vm_object_resolve_page_for_write(&slot->object,
                                              object_offset - (size_t)page_offset,
                                              &page)) {
            return false;
        }
        for (i = 0; i < chunk; ++i) {
            ((uint8_t*)page)[page_offset + i] = bytes[copied + i];
        }
        copied += chunk;
    }
    return true;
}

void linux_compat_vm_destroy(linux_compat_vm_t* vm) {
    size_t i = 0;

    if (vm == 0) {
        return;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (vm->regions[i].used) {
            (void)release_region(vm, &vm->regions[i]);
        }
    }
    vm->address_space = 0;
    vm->process = 0;
    vm->program_break = vm->brk_base;
    vm->next_mmap = LINUX_COMPAT_MMAP_BASE;
}

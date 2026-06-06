#include "linux_compat_vm.h"

#include "memory.h"
#include "pmm.h"
#include "vm_private.h"

__attribute__((weak)) void console_putc(char ch) {
    (void)ch;
}

static void dbg_puts(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return;
    }
    while (value[i] != '\0') {
        console_putc(value[i]);
        i += 1U;
    }
}

static void dbg_i64(int64_t value) {
    char digits[24];
    size_t used = 0;

    if (value < 0) {
        console_putc('-');
        value = -value;
    }
    if (value == 0) {
        console_putc('0');
        return;
    }
    while (value != 0 && used < sizeof(digits)) {
        digits[used] = (char)('0' + (value % 10));
        used += 1U;
        value /= 10;
    }
    while (used > 0) {
        used -= 1U;
        console_putc(digits[used]);
    }
}

static uintptr_t align_up_page_uintptr(uintptr_t value) {
    const uintptr_t mask = (uintptr_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
}

static size_t align_up_page_size(size_t value) {
    const size_t mask = (size_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
}

static size_t min_size(size_t a, size_t b) {
    return a < b ? a : b;
}

static size_t debug_process_region_count(const vm_process_t* process) {
    size_t i = 0;
    size_t used = 0;

    if (process == 0) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] != 0) {
            used += 1U;
        }
    }
    return used;
}

static size_t debug_address_space_region_count(
    const vm_address_space_t* address_space) {
    size_t i = 0;
    size_t used = 0;

    if (address_space == 0) {
        return 0;
    }
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] != 0) {
            used += 1U;
        }
    }
    return used;
}

static void debug_first_address_space_overlap(
    const vm_address_space_t* address_space,
    uintptr_t vaddr,
    size_t length) {
    size_t i = 0;

    if (address_space == 0) {
        return;
    }
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        const vm_user_region_t* region = address_space->user_regions[i];

        if (region != 0 && region->registered &&
            ranges_overlap(vaddr, length, region->vaddr, region->size)) {
            dbg_puts(" overlap=");
            dbg_i64((int64_t)region->vaddr);
            dbg_puts("+");
            dbg_i64((int64_t)region->size);
            return;
        }
    }
}

static bool debug_fault_ranges_overlap(const struct VmFaultRange* ranges,
                                       size_t count,
                                       uintptr_t vaddr,
                                       size_t length) {
    size_t i = 0;

    if (ranges == 0) {
        return false;
    }
    for (i = 0; i < count; ++i) {
        if (ranges[i].valid &&
            ranges_overlap(vaddr, length, ranges[i].vaddr, ranges[i].size)) {
            return true;
        }
    }
    return false;
}

static bool debug_has_free_process_slot(const vm_process_t* process) {
    size_t i = 0;

    if (process == 0) {
        return false;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == 0) {
            return true;
        }
    }
    return false;
}

static bool debug_process_owns_region(const vm_process_t* process,
                                      const vm_user_region_t* region) {
    size_t i = 0;

    if (process == 0 || region == 0) {
        return false;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == region) {
            return true;
        }
    }
    return false;
}

static bool debug_has_free_address_space_slot(
    const vm_address_space_t* address_space) {
    size_t i = 0;

    if (address_space == 0) {
        return false;
    }
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] == 0) {
            return true;
        }
    }
    return false;
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
    size_t i = 0;

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
    for (i = 0; i < VM_OBJECT_ANON_SLOT_TABLE_COUNT - 1U; ++i) {
        slot->object.backing.anon.extra_page_slots[i] = 0;
    }
    slot->vaddr = 0;
    slot->length = 0;
    slot->prot = 0;
    slot->flags = 0;
}

static void clear_object_value(vm_object_t* object) {
    size_t i = 0;

    if (object == 0) {
        return;
    }
    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = 0;
    object->backing.anon.page_count = 0;
    for (i = 0; i < VM_OBJECT_ANON_SLOT_TABLE_COUNT - 1U; ++i) {
        object->backing.anon.extra_page_slots[i] = 0;
    }
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

static linux_compat_vm_region_t* find_region_covering(linux_compat_vm_t* vm,
                                                      uintptr_t addr,
                                                      size_t length) {
    size_t i = 0;

    if (vm == 0 || length == 0U || addr > UINTPTR_MAX - (uintptr_t)length) {
        return 0;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        linux_compat_vm_region_t* slot = &vm->regions[i];

        if (!slot->used ||
            addr < slot->vaddr ||
            addr + (uintptr_t)length >
                slot->vaddr + (uintptr_t)slot->length) {
            continue;
        }
        return slot;
    }
    return 0;
}

static bool mmap_range_available(linux_compat_vm_t* vm,
                                 uintptr_t addr,
                                 size_t length) {
    size_t i = 0;

    if (vm == 0 || length == 0U ||
        addr >= vm_user_limit() ||
        length > (size_t)(vm_user_limit() - addr)) {
        return false;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        linux_compat_vm_region_t* slot = &vm->regions[i];

        if (slot->used &&
            ranges_overlap(addr, length, slot->vaddr, slot->length)) {
            return false;
        }
    }
    return true;
}

static bool find_available_mmap_addr(linux_compat_vm_t* vm,
                                     uintptr_t start,
                                     size_t length,
                                     uintptr_t* out_addr) {
    uintptr_t vaddr = align_up_page_uintptr(start);

    if (out_addr == 0 || vm == 0 || length == 0U) {
        return false;
    }
    while (vaddr < vm_user_limit() &&
           length <= (size_t)(vm_user_limit() - vaddr)) {
        if (mmap_range_available(vm, vaddr, length)) {
            *out_addr = vaddr;
            return true;
        }
        vaddr += MEMORY_PAGE_SIZE;
    }
    return false;
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
                       bool heap,
                       bool map_now) {
    const uint64_t vm_flags = prot_to_vm_flags(prot);

    if (!vm_ready(vm) || slot == 0 || slot->used ||
        (vaddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        length == 0 || (length & ((size_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        (vm_flags & (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC)) == 0U ||
        !vm_object_init_anon(&slot->object, length)) {
        return false;
    }

    if (!(map_now ? vm_process_map_object_region_at(vm->process,
                                                    &slot->region,
                                                    vaddr,
                                                    length,
                                                    vm_flags,
                                                    &slot->object,
                                                    0U)
                  : vm_process_set_fault_object_region_at(vm->process,
                                                          &slot->region,
                                                          vaddr,
                                                          length,
                                                          vm_flags,
                                                          &slot->object,
                                                          0U))) {
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

static bool write_object_bytes(vm_object_t* object,
                               size_t object_offset,
                               const uint8_t* bytes,
                               size_t length) {
    size_t copied = 0;

    if (object == 0 || (bytes == 0 && length != 0U) ||
        object_offset > object->size ||
        length > object->size - object_offset) {
        return false;
    }

    while (copied < length) {
        const size_t current = object_offset + copied;
        const size_t page_offset =
            current & ((size_t)MEMORY_PAGE_SIZE - 1U);
        const size_t page_base = current - page_offset;
        const size_t chunk =
            min_size(length - copied, MEMORY_PAGE_SIZE - page_offset);
        uintptr_t page = 0;
        size_t i = 0;

        if (!vm_object_resolve_page_for_write(object, page_base, &page)) {
            return false;
        }
        for (i = 0; i < chunk; ++i) {
            ((uint8_t*)page)[page_offset + i] = bytes[copied + i];
        }
        copied += chunk;
    }
    return true;
}

static bool zero_object_bytes(vm_object_t* object,
                              size_t object_offset,
                              size_t length) {
    static const uint8_t zeros[64] = {0};
    size_t cleared = 0;

    while (cleared < length) {
        const size_t chunk = min_size(length - cleared, sizeof(zeros));

        if (!write_object_bytes(object,
                                object_offset + cleared,
                                zeros,
                                chunk)) {
            return false;
        }
        cleared += chunk;
    }
    return true;
}

static bool remap_existing_region_pages(linux_compat_vm_t* vm,
                                        linux_compat_vm_region_t* region,
                                        uintptr_t addr,
                                        size_t length,
                                        uint32_t prot) {
    uintptr_t offset = 0;
    const uint64_t vm_flags = prot_to_vm_flags(prot);

    if (!vm_ready(vm) || region == 0 ||
        (vm_flags & (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC)) == 0U) {
        return false;
    }

    while (offset < (uintptr_t)length) {
        const uintptr_t vaddr = addr + offset;
        const size_t object_offset = (size_t)(vaddr - region->vaddr);
        uintptr_t page = 0;

        if (!can_map_page(vm->address_space, vaddr) &&
            !vm_user_region_unmap_page(&region->region, vaddr)) {
            return false;
        }
        if (!vm_object_resolve_page_for_write(&region->object,
                                              object_offset,
                                              &page) ||
            !map_page_internal(vm->address_space, vaddr, page, vm_flags)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }
    flush_tlb_if_enabled();
    region->prot |= prot;
    region->region.flags = prot_to_vm_flags(region->prot);
    return true;
}

static bool convert_region_to_private_anon(linux_compat_vm_t* vm,
                                           linux_compat_vm_region_t* region,
                                           uint32_t prot) {
    vm_object_t replacement;
    const uintptr_t vaddr = region != 0 ? region->vaddr : 0U;
    const size_t length = region != 0 ? region->length : 0U;
    const uint32_t flags = region != 0 ? region->flags : 0U;
    const uint32_t merged_prot = region != 0 ? (region->prot | prot) : prot;
    const uint8_t* source =
        region != 0 &&
                region->object.backing_kind == VM_OBJECT_BACKING_PHYSICAL
            ? (const uint8_t*)region->object.backing.physical.base_paddr
            : 0;

    clear_object_value(&replacement);
    if (!vm_ready(vm) || region == 0 || !region->used || source == 0 ||
        !vm_object_init_anon(&replacement, length) ||
        !write_object_bytes(&replacement, 0U, source, length)) {
        (void)vm_object_reset(&replacement);
        return false;
    }

    if (!vm_process_remove_user_region(vm->process, &region->region)) {
        (void)vm_object_reset(&replacement);
        return false;
    }
    if (!vm_object_reset(&region->object)) {
        (void)vm_object_reset(&replacement);
        clear_region_slot(region);
        return false;
    }
    region->object = replacement;
    if (!vm_process_user_region_init(vm->process,
                                     &region->region,
                                     vaddr,
                                     length,
                                     prot_to_vm_flags(merged_prot)) ||
        !vm_user_region_set_fault_object_at(&region->region,
                                            &region->object,
                                            0U)) {
        (void)vm_object_reset(&region->object);
        clear_region_slot(region);
        return false;
    }

    region->used = true;
    region->heap = false;
    region->vaddr = vaddr;
    region->length = length;
    region->prot = merged_prot;
    region->flags = flags;
    return true;
}

static bool unmap_fixed_pages(linux_compat_vm_t* vm,
                              uintptr_t addr,
                              size_t length) {
    uintptr_t offset = 0;
    bool changed = false;

    if (!vm_ready(vm)) {
        return false;
    }
    while (offset < (uintptr_t)length) {
        const uintptr_t vaddr = addr + offset;

        if (!can_map_page(vm->address_space, vaddr)) {
            if (!unmap_page_internal(vm->address_space, vaddr)) {
                return false;
            }
            changed = true;
        }
        offset += MEMORY_PAGE_SIZE;
    }
    if (changed) {
        flush_tlb_if_enabled();
    }
    return true;
}

static uintptr_t mmap_fixed_prot_none(linux_compat_vm_t* vm,
                                      uintptr_t addr,
                                      size_t length) {
    const size_t mapped_length = align_up_page_size(length);

    if (!vm_ready(vm) || addr == 0U || length == 0U ||
        (addr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        addr >= vm_user_limit() ||
        mapped_length > (size_t)(vm_user_limit() - addr)) {
        return (uintptr_t)-22;
    }
    return unmap_fixed_pages(vm, addr, mapped_length) ? addr : (uintptr_t)-12;
}

static uintptr_t mmap_file_fixed_existing(linux_compat_vm_t* vm,
                                          uintptr_t addr,
                                          size_t length,
                                          uint32_t prot,
                                          const uint8_t* data,
                                          size_t data_size,
                                          size_t file_offset) {
    const size_t mapped_length = align_up_page_size(length);
    linux_compat_vm_region_t* region =
        find_region_covering(vm, addr, mapped_length);
    const size_t object_offset =
        region != 0 ? (size_t)(addr - region->vaddr) : 0U;
    size_t copy_length = 0;

    if (region == 0) {
        return (uintptr_t)-22;
    }
    if (region->object.backing_kind == VM_OBJECT_BACKING_PHYSICAL &&
        !convert_region_to_private_anon(vm, region, prot)) {
        return (uintptr_t)-12;
    }
    if (file_offset < data_size) {
        copy_length = min_size(length, data_size - file_offset);
    }
    if (!zero_object_bytes(&region->object, object_offset, mapped_length) ||
        (copy_length != 0U &&
         !write_object_bytes(&region->object,
                             object_offset,
                             data + file_offset,
                             copy_length)) ||
        !remap_existing_region_pages(vm, region, addr, mapped_length, prot)) {
        return (uintptr_t)-12;
    }
    return addr;
}

static uintptr_t map_mmap_region(linux_compat_vm_t* vm,
                                 uintptr_t addr,
                                 size_t length,
                                 uint32_t prot,
                                 uint32_t flags,
                                 linux_compat_vm_region_t** out_region) {
    linux_compat_vm_region_t* slot = 0;
    uintptr_t vaddr = addr;
    size_t mapped_length = 0;
    const bool fixed = (flags & LINUX_COMPAT_MAP_FIXED) != 0U;

    if (out_region != 0) {
        *out_region = 0;
    }
    if (!vm_ready(vm) || length == 0U) {
        return (uintptr_t)-22;
    }

    mapped_length = align_up_page_size(length);
    if (vaddr == 0U) {
        vaddr = vm->next_mmap;
    } else if (!fixed) {
        vaddr = align_up_page_uintptr(vaddr);
    }
    if ((vaddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        vaddr >= vm_user_limit() ||
        mapped_length > (size_t)(vm_user_limit() - vaddr)) {
        return (uintptr_t)-22;
    }
    if (!fixed && !mmap_range_available(vm, vaddr, mapped_length) &&
        !find_available_mmap_addr(vm,
                                  vm->next_mmap,
                                  mapped_length,
                                  &vaddr)) {
        return (uintptr_t)-12;
    }

    slot = find_free_region(vm);
    if (!map_region(vm, slot, vaddr, mapped_length, prot, flags, false, false)) {
        return (uintptr_t)-12;
    }
    if (!fixed && vaddr + (uintptr_t)mapped_length > vm->next_mmap) {
        vm->next_mmap = vaddr + (uintptr_t)mapped_length;
    }
    if (out_region != 0) {
        *out_region = slot;
    }
    return vaddr;
}

static bool move_anon_pages_to_region(linux_compat_vm_t* vm,
                                      linux_compat_vm_region_t* old_region,
                                      linux_compat_vm_region_t* new_region,
                                      size_t old_length) {
    const size_t page_count = old_length / MEMORY_PAGE_SIZE;
    size_t i = 0;

    if (!vm_ready(vm) || old_region == 0 || new_region == 0 ||
        old_region->object.backing_kind != VM_OBJECT_BACKING_ANON ||
        new_region->object.backing_kind != VM_OBJECT_BACKING_ANON ||
        old_region->object.backing.anon.page_count < page_count ||
        new_region->object.backing.anon.page_count < page_count) {
        return false;
    }

    for (i = 0; i < page_count; ++i) {
        uintptr_t* old_slot = anon_page_slot(&old_region->object, i);
        uintptr_t* new_slot = anon_page_slot(&new_region->object, i);
        const uintptr_t old_page = old_slot != 0 ? *old_slot : 0U;
        const uintptr_t new_page = new_slot != 0 ? *new_slot : 0U;
        const uintptr_t new_vaddr =
            new_region->vaddr + (uintptr_t)(i * MEMORY_PAGE_SIZE);

        if (old_slot == 0 || new_slot == 0) {
            return false;
        }
        if (old_page == 0U) {
            continue;
        }
        if (!unmap_page_internal(vm->address_space, new_vaddr)) {
            return false;
        }
        if (new_page != 0U && !pmm_free_page((void*)new_page)) {
            return false;
        }
        *new_slot = old_page;
        *old_slot = 0U;
        if (!map_page_internal(vm->address_space,
                               new_vaddr,
                               old_page,
                               new_region->region.flags)) {
            return false;
        }
    }
    flush_tlb_if_enabled();
    return true;
}

static uintptr_t map_physical_mmap_region(linux_compat_vm_t* vm,
                                          uintptr_t addr,
                                          size_t length,
                                          uint32_t prot,
                                          uint32_t flags,
                                          uintptr_t paddr) {
    linux_compat_vm_region_t* slot = 0;
    uintptr_t vaddr = addr;
    size_t mapped_length = 0;
    const bool fixed = (flags & LINUX_COMPAT_MAP_FIXED) != 0U;
    const uint64_t vm_flags = prot_to_vm_flags(prot);

    if (!vm_ready(vm) || length == 0U ||
        (paddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        (vm_flags & (VM_PAGE_READ | VM_PAGE_EXEC)) == 0U ||
        (vm_flags & VM_PAGE_WRITE) != 0U) {
        dbg_puts("\nDBG mmap physical reject pre paddr=");
        dbg_i64((int64_t)paddr);
        dbg_puts(" len=");
        dbg_i64((int64_t)length);
        dbg_puts("\n");
        return (uintptr_t)-22;
    }

    mapped_length = align_up_page_size(length);
    if (vaddr == 0U) {
        vaddr = vm->next_mmap;
    } else if (!fixed) {
        vaddr = align_up_page_uintptr(vaddr);
    }
    if ((vaddr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) != 0U ||
        vaddr >= vm_user_limit() ||
        mapped_length > (size_t)(vm_user_limit() - vaddr)) {
        dbg_puts("\nDBG mmap physical reject addr vaddr=");
        dbg_i64((int64_t)vaddr);
        dbg_puts(" len=");
        dbg_i64((int64_t)mapped_length);
        dbg_puts("\n");
        return (uintptr_t)-22;
    }
    if (!fixed && !mmap_range_available(vm, vaddr, mapped_length) &&
        !find_available_mmap_addr(vm,
                                  vm->next_mmap,
                                  mapped_length,
                                  &vaddr)) {
        dbg_puts("\nDBG mmap physical no range start=");
        dbg_i64((int64_t)vm->next_mmap);
        dbg_puts(" len=");
        dbg_i64((int64_t)mapped_length);
        dbg_puts("\n");
        return (uintptr_t)-12;
    }

    slot = find_free_region(vm);
    if (slot == 0) {
        dbg_puts("\nDBG mmap physical no slot vaddr=");
        dbg_i64((int64_t)vaddr);
        dbg_puts("\n");
        return (uintptr_t)-12;
    }
    if (!vm_object_init_physical(&slot->object, paddr, mapped_length)) {
        dbg_puts("\nDBG mmap physical object fail paddr=");
        dbg_i64((int64_t)paddr);
        dbg_puts(" len=");
        dbg_i64((int64_t)mapped_length);
        dbg_puts(" vaddr=");
        dbg_i64((int64_t)vaddr);
        dbg_puts("\n");
        clear_region_slot(slot);
        return (uintptr_t)-12;
    }
    if (!vm_process_user_region_init(vm->process,
                                     &slot->region,
                                     vaddr,
                                     mapped_length,
                                     vm_flags)) {
        dbg_puts("\nDBG mmap physical region init fail vaddr=");
        dbg_i64((int64_t)vaddr);
        dbg_puts(" paddr=");
        dbg_i64((int64_t)paddr);
        dbg_puts(" len=");
        dbg_i64((int64_t)mapped_length);
        dbg_puts(" proc_regions=");
        dbg_i64((int64_t)debug_process_region_count(vm->process));
        dbg_puts(" as_regions=");
        dbg_i64((int64_t)debug_address_space_region_count(vm->address_space));
        dbg_puts(" same_as=");
        dbg_i64(vm->process->address_space == vm->address_space ? 1 : 0);
        dbg_puts(" root=");
        dbg_i64(vm->address_space != 0 && vm->address_space->root_table != 0
                    ? 1
                    : 0);
        dbg_puts(" page_span=");
        dbg_i64(page_span_args_valid(vaddr, mapped_length) ? 1 : 0);
        dbg_puts(" user_range=");
        dbg_i64(vm_range_is_user(vaddr, mapped_length) ? 1 : 0);
        dbg_puts(" flags=");
        dbg_i64(user_flags_valid(vm_flags) ? 1 : 0);
        dbg_puts(" free_proc=");
        dbg_i64(debug_has_free_process_slot(vm->process) ? 1 : 0);
        dbg_puts(" owns=");
        dbg_i64(debug_process_owns_region(vm->process, &slot->region) ? 1 : 0);
        dbg_puts(" free_as=");
        dbg_i64(debug_has_free_address_space_slot(vm->address_space) ? 1 : 0);
        dbg_puts(" kmap_ov=");
        dbg_i64(debug_fault_ranges_overlap(vm->address_space->kernel_mappings,
                                           VM_MAX_KERNEL_MAPPINGS,
                                           vaddr,
                                           mapped_length)
                    ? 1
                    : 0);
        dbg_puts(" kfault_ov=");
        dbg_i64(debug_fault_ranges_overlap(vm->address_space->kernel_fault_ranges,
                                           VM_MAX_KERNEL_FAULT_RANGES,
                                           vaddr,
                                           mapped_length)
                    ? 1
                    : 0);
        debug_first_address_space_overlap(vm->address_space,
                                          vaddr,
                                          mapped_length);
        dbg_puts("\n");
        (void)vm_object_reset(&slot->object);
        clear_region_slot(slot);
        return (uintptr_t)-12;
    }
    if (!vm_user_region_set_fault_object_at(&slot->region,
                                            &slot->object,
                                            0U)) {
        dbg_puts("\nDBG mmap physical fault bind fail vaddr=");
        dbg_i64((int64_t)vaddr);
        dbg_puts(" paddr=");
        dbg_i64((int64_t)paddr);
        dbg_puts(" len=");
        dbg_i64((int64_t)mapped_length);
        dbg_puts("\n");
        (void)vm_process_remove_user_region(vm->process, &slot->region);
        (void)vm_object_reset(&slot->object);
        clear_region_slot(slot);
        return (uintptr_t)-12;
    }

    slot->used = true;
    slot->heap = false;
    slot->vaddr = vaddr;
    slot->length = mapped_length;
    slot->prot = prot;
    slot->flags = flags;
    if (!fixed && vaddr + (uintptr_t)mapped_length > vm->next_mmap) {
        vm->next_mmap = vaddr + (uintptr_t)mapped_length;
    }
    return vaddr;
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
                    true,
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
    if ((flags & LINUX_COMPAT_MAP_FIXED) != 0U && prot == 0U) {
        return mmap_fixed_prot_none(vm, addr, length);
    }
    return map_mmap_region(vm, addr, length, prot, flags, 0);
}

uintptr_t linux_compat_vm_mmap_file(linux_compat_vm_t* vm,
                                    uintptr_t addr,
                                    size_t length,
                                    uint32_t prot,
                                    uint32_t flags,
                                    const uint8_t* data,
                                    size_t data_size,
                                    size_t file_offset) {
    linux_compat_vm_region_t* region = 0;
    uintptr_t vaddr = 0;
    size_t copy_length = 0;
    const uintptr_t data_addr = (uintptr_t)data + (uintptr_t)file_offset;

    if ((file_offset & ((size_t)MEMORY_PAGE_SIZE - 1U)) != 0U) {
        return (uintptr_t)-22;
    }
    if ((flags & LINUX_COMPAT_MAP_FIXED) != 0U && addr != 0U) {
        const uintptr_t fixed =
            mmap_file_fixed_existing(vm,
                                     addr,
                                     length,
                                     prot,
                                     data,
                                     data_size,
                                     file_offset);
        if ((intptr_t)fixed != -22) {
            return fixed;
        }
    }

    if (data != 0 &&
        (prot & LINUX_COMPAT_PROT_WRITE) == 0U &&
        (data_addr & ((uintptr_t)MEMORY_PAGE_SIZE - 1U)) == 0U) {
        return map_physical_mmap_region(vm,
                                        addr,
                                        length,
                                        prot,
                                        flags,
                                        data_addr);
    }

    vaddr = map_mmap_region(vm, addr, length, prot, flags, &region);
    if ((intptr_t)vaddr < 0) {
        return vaddr;
    }
    if (file_offset < data_size) {
        copy_length = min_size(length, data_size - file_offset);
    }
    if (copy_length != 0U &&
        !write_object_bytes(&region->object,
                            0U,
                            data + file_offset,
                            copy_length)) {
        (void)release_region(vm, region);
        return (uintptr_t)-12;
    }
    return vaddr;
}

uintptr_t linux_compat_vm_mremap(linux_compat_vm_t* vm,
                                 uintptr_t old_addr,
                                 size_t old_length,
                                 size_t new_length,
                                 uint32_t flags) {
    static const uint32_t kMremapMaymove = 1U;
    linux_compat_vm_region_t* old_region = 0;
    linux_compat_vm_region_t* new_region = 0;
    uintptr_t new_addr = 0;
    size_t old_mapped_length = 0;
    size_t new_mapped_length = 0;

    if (!vm_ready(vm) || old_addr == 0U || old_length == 0U ||
        new_length == 0U || (flags & ~kMremapMaymove) != 0U) {
        return (uintptr_t)-22;
    }
    old_mapped_length = align_up_page_size(old_length);
    new_mapped_length = align_up_page_size(new_length);
    if (new_mapped_length <= old_mapped_length) {
        return old_addr;
    }
    if ((flags & kMremapMaymove) == 0U) {
        return (uintptr_t)-12;
    }

    old_region = find_exact_region(vm, old_addr, old_mapped_length, false);
    if (old_region == 0) {
        return (uintptr_t)-22;
    }

    new_addr = map_mmap_region(vm,
                               0U,
                               new_mapped_length,
                               old_region->prot,
                               old_region->flags,
                               &new_region);
    if ((intptr_t)new_addr < 0) {
        return new_addr;
    }
    if (!move_anon_pages_to_region(vm,
                                   old_region,
                                   new_region,
                                   old_mapped_length)) {
        (void)release_region(vm, new_region);
        return (uintptr_t)-12;
    }
    if (!release_region(vm, old_region)) {
        (void)release_region(vm, new_region);
        return (uintptr_t)-22;
    }
    return new_addr;
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
    if (!map_region(vm, slot, addr, mapped_length, prot, flags, false, true)) {
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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "platform_mmio.h"
#include "riscv.h"
#include "vm.h"

#define SV39_PAGE_SHIFT 12U
#define SV39_LEVEL_BITS 9U
#define SV39_LEVEL_ENTRIES 512U
#define SV39_SUPERPAGE_SIZE_1G (1UL << 30)
#define SV39_PTE_VALID (1ULL << 0)
#define SV39_PTE_LEAF_MASK (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC)
#define SV39_PTE_USER VM_PAGE_USER
#define SV39_PTE_ACCESSED (1ULL << 6)
#define SV39_PTE_DIRTY (1ULL << 7)
#define SV39_PTE_NON_LEAF_RESERVED_MASK \
    (SV39_PTE_USER | SV39_PTE_ACCESSED | SV39_PTE_DIRTY)
#define SV39_PTE_FLAG_MASK \
    (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC | VM_PAGE_USER)
#define SV39_PPN_MASK ((1ULL << 44) - 1ULL)
#define VM_MAX_KERNEL_MAPPINGS 16U
#define VM_MAX_KERNEL_FAULT_RANGES 16U
#define VM_MAX_USER_REGIONS VM_PROCESS_MAX_USER_REGIONS
#define VM_MAX_ADDRESS_SPACES 2U
#define VM_MAX_FAULT_ACTIONS 16U
#define VM_USER_VADDR_BASE ((uintptr_t)0)
#define VM_USER_VADDR_LIMIT ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_BASE ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_LIMIT ((uintptr_t)MEM_BASE + (uintptr_t)MEM_SIZE)
#define VM_OBJECT_ANON_SLOT_TABLE_ENTRIES \
    (MEMORY_PAGE_SIZE / sizeof(uintptr_t))
#define VM_OBJECT_ANON_PAGE_SLOTS \
    (VM_OBJECT_ANON_SLOT_TABLE_ENTRIES * VM_OBJECT_ANON_SLOT_TABLE_COUNT)

struct VmFaultRange {
    bool valid;
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size;
    uint64_t flags;
};

typedef enum VmFaultAction {
    VM_FAULT_ACTION_SKIP_INSTRUCTION = 0,
    VM_FAULT_ACTION_RESUME_AT_SLOT,
} vm_fault_action_t;

struct VmFaultActionRule {
    bool valid;
    uint64_t cause;
    uintptr_t vaddr;
    size_t size;
    vm_fault_action_t action;
    volatile uintptr_t* resume_pc_slot;
};

struct VmAddressSpace {
    bool allocated;
    uint64_t* root_table;
    uintptr_t root_table_pa;
    uint64_t satp_value;
    bool enabled;
    struct VmFaultRange kernel_mappings[VM_MAX_KERNEL_MAPPINGS];
    struct VmFaultRange kernel_fault_ranges[VM_MAX_KERNEL_FAULT_RANGES];
    struct VmFaultActionRule fault_actions[VM_MAX_FAULT_ACTIONS];
    vm_user_region_t* user_regions[VM_MAX_USER_REGIONS];
};

static inline size_t vpn_index(uintptr_t vaddr, unsigned level) {
    return (size_t)((vaddr >> (SV39_PAGE_SHIFT + level * SV39_LEVEL_BITS)) &
                    (SV39_LEVEL_ENTRIES - 1U));
}

static inline uintptr_t align_down_page(uintptr_t value) {
    return value & ~((uintptr_t)MEMORY_PAGE_SIZE - 1U);
}

static inline uint64_t pte_from_pa(uintptr_t paddr, uint64_t flags) {
    return (((uint64_t)(paddr >> SV39_PAGE_SHIFT) & SV39_PPN_MASK) << 10) |
           flags;
}

static inline uint64_t* table_from_pte(uint64_t pte) {
    const uintptr_t table_pa =
        (uintptr_t)(((pte >> 10) & SV39_PPN_MASK) << SV39_PAGE_SHIFT);
    return (uint64_t*)table_pa;
}

static inline bool range_overflows(uintptr_t base, size_t size) {
    if (size == 0) {
        return false;
    }

    return base > UINTPTR_MAX - ((uintptr_t)size - 1U);
}

static inline bool span_args_valid(uintptr_t base, size_t size) {
    return size != 0 && !range_overflows(base, size);
}

static inline bool range_within_window(uintptr_t base,
                                       size_t size,
                                       uintptr_t window_base,
                                       uintptr_t window_limit) {
    const uintptr_t end = base + (uintptr_t)size;

    if (!span_args_valid(base, size)) {
        return false;
    }

    return base >= window_base && end <= window_limit;
}

static inline bool page_span_args_valid(uintptr_t base, size_t size) {
    if (size == 0 ||
        (base & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    return !range_overflows(base, size);
}

static inline bool mapped_range_args_valid(uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size) {
    if (!page_span_args_valid(vaddr, size) ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    return !range_overflows(paddr, size);
}

static inline bool flags_valid(uint64_t flags) {
    if ((flags & ~SV39_PTE_FLAG_MASK) != 0) {
        return false;
    }

    if ((flags & SV39_PTE_LEAF_MASK) == 0) {
        return false;
    }

    if ((flags & VM_PAGE_WRITE) != 0 && (flags & VM_PAGE_READ) == 0) {
        return false;
    }

    return true;
}

static inline bool user_flags_valid(uint64_t flags) {
    return flags_valid(flags) && (flags & VM_PAGE_USER) != 0;
}

static inline bool kernel_flags_valid(uint64_t flags) {
    return flags_valid(flags) && (flags & VM_PAGE_USER) == 0;
}

static inline bool page_fault_cause_valid(uint64_t cause) {
    return cause == RISCV_EXC_INSN_PAGE_FAULT ||
           cause == RISCV_EXC_LOAD_PAGE_FAULT ||
           cause == RISCV_EXC_STORE_PAGE_FAULT;
}

static inline bool ranges_overlap(uintptr_t start_a,
                                  size_t size_a,
                                  uintptr_t start_b,
                                  size_t size_b) {
    const uintptr_t end_a = start_a + (uintptr_t)size_a;
    const uintptr_t end_b = start_b + (uintptr_t)size_b;

    return start_a < end_b && start_b < end_a;
}

static inline bool user_region_descriptor_valid(const vm_user_region_t* region) {
    return region != NULL && region->address_space != NULL && region->registered &&
           page_span_args_valid(region->vaddr, region->size) &&
           user_flags_valid(region->flags) &&
           vm_range_is_user(region->vaddr, region->size);
}

static inline size_t object_page_count(size_t size) {
    return size / MEMORY_PAGE_SIZE;
}

static inline size_t anon_slot_table_count_for_pages(size_t page_count) {
    return (page_count + VM_OBJECT_ANON_SLOT_TABLE_ENTRIES - 1U) /
           VM_OBJECT_ANON_SLOT_TABLE_ENTRIES;
}

static inline uintptr_t* anon_slot_table(vm_object_t* object,
                                         size_t table_index) {
    if (object == NULL || table_index >= VM_OBJECT_ANON_SLOT_TABLE_COUNT) {
        return NULL;
    }
    return table_index == 0U
               ? object->backing.anon.page_slots
               : object->backing.anon.extra_page_slots[table_index - 1U];
}

static inline const uintptr_t* anon_slot_table_const(
    const vm_object_t* object,
    size_t table_index) {
    if (object == NULL || table_index >= VM_OBJECT_ANON_SLOT_TABLE_COUNT) {
        return NULL;
    }
    return table_index == 0U
               ? object->backing.anon.page_slots
               : object->backing.anon.extra_page_slots[table_index - 1U];
}

static inline uintptr_t* anon_page_slot(vm_object_t* object,
                                        size_t page_index) {
    uintptr_t* table =
        anon_slot_table(object,
                        page_index / VM_OBJECT_ANON_SLOT_TABLE_ENTRIES);

    return table == NULL
               ? NULL
               : &table[page_index % VM_OBJECT_ANON_SLOT_TABLE_ENTRIES];
}

static inline bool physical_object_descriptor_valid(const vm_object_t* object) {
    return (object->backing.physical.base_paddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           !range_overflows(object->backing.physical.base_paddr, object->size);
}

static inline bool anon_object_descriptor_valid(const vm_object_t* object) {
    const size_t expected_page_count = object_page_count(object->size);
    const size_t slot_table_count =
        anon_slot_table_count_for_pages(expected_page_count);
    size_t i = 0;

    if (expected_page_count == 0 ||
        expected_page_count > VM_OBJECT_ANON_PAGE_SLOTS ||
        object->backing.anon.page_count != expected_page_count ||
        slot_table_count == 0 ||
        slot_table_count > VM_OBJECT_ANON_SLOT_TABLE_COUNT) {
        return false;
    }
    for (i = 0; i < slot_table_count; ++i) {
        const uintptr_t* table = anon_slot_table_const(object, i);

        if (table == NULL ||
            (((uintptr_t)table) & (MEMORY_PAGE_SIZE - 1U)) != 0) {
            return false;
        }
    }
    return true;
}

static inline bool object_descriptor_valid(const vm_object_t* object) {
    if (object == NULL || !object->initialized || object->size == 0 ||
        (object->size & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        return physical_object_descriptor_valid(object);
    case VM_OBJECT_BACKING_ANON:
        return anon_object_descriptor_valid(object);
    default:
        return false;
    }
}

static inline bool object_range_compatible(const vm_object_t* object,
                                           size_t object_offset,
                                           size_t size) {
    return object_descriptor_valid(object) &&
           page_span_args_valid(object_offset, size) &&
           range_within_window(object_offset, size, 0, object->size);
}

static inline bool fault_range_allows_access(uint64_t flags, uint64_t cause) {
    switch (cause) {
    case RISCV_EXC_INSN_PAGE_FAULT:
        return (flags & VM_PAGE_EXEC) != 0;
    case RISCV_EXC_LOAD_PAGE_FAULT:
        return (flags & VM_PAGE_READ) != 0;
    case RISCV_EXC_STORE_PAGE_FAULT:
        return (flags & VM_PAGE_WRITE) != 0;
    default:
        return false;
    }
}

bool vm_address_space_unregister_user_region_internal(
    vm_address_space_t* address_space,
    vm_user_region_t* region);
void* alloc_zeroed_page(void);
bool map_page_internal(vm_address_space_t* address_space,
                       uintptr_t vaddr,
                       uintptr_t paddr,
                       uint64_t flags);
bool unmap_page_internal(vm_address_space_t* address_space, uintptr_t vaddr);
bool can_map_page(vm_address_space_t* address_space, uintptr_t vaddr);
void flush_tlb_if_enabled(void);
bool object_resolve_page(vm_object_t* object,
                         size_t offset,
                         bool create,
                         uintptr_t* out_paddr);
bool region_object_compatible(const vm_user_region_t* region,
                              const vm_object_t* object,
                              size_t object_offset);
bool clear_region_page_mappings(vm_user_region_t* region, bool* changed);
bool map_region_object_pages(vm_user_region_t* region,
                             vm_object_t* object,
                             size_t object_offset);

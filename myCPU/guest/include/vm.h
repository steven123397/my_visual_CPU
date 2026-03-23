#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VM_PAGE_READ (1ULL << 1)
#define VM_PAGE_WRITE (1ULL << 2)
#define VM_PAGE_EXEC (1ULL << 3)
#define VM_PAGE_USER (1ULL << 4)
#define VM_PROCESS_MAX_USER_REGIONS 8U

typedef struct VmAddressSpace vm_address_space_t;

typedef enum VmRegionObjectMode {
    VM_REGION_OBJECT_NONE = 0,
    VM_REGION_OBJECT_MAPPED,
    VM_REGION_OBJECT_FAULT,
} vm_region_object_mode_t;

typedef struct VmObject {
    bool initialized;
    uintptr_t paddr;
    size_t size;
} vm_object_t;

typedef struct VmUserRegion {
    vm_address_space_t* address_space;
    uintptr_t vaddr;
    size_t size;
    uint64_t flags;
    bool registered;
    vm_object_t* object;
    vm_region_object_mode_t object_mode;
} vm_user_region_t;

typedef struct VmProcess {
    vm_address_space_t* address_space;
    uintptr_t entry_pc;
    uintptr_t user_sp;
    vm_user_region_t* user_regions[VM_PROCESS_MAX_USER_REGIONS];
} vm_process_t;

bool vm_address_space_create(vm_address_space_t** out_space);
bool vm_address_space_activate(vm_address_space_t* address_space);
bool vm_address_space_is_active(const vm_address_space_t* address_space);
bool vm_address_space_is_enabled(const vm_address_space_t* address_space);
bool vm_address_space_disable(vm_address_space_t* address_space);
bool vm_address_space_destroy(vm_address_space_t* address_space);
bool vm_address_space_map_identity_1g(vm_address_space_t* address_space,
                                      uintptr_t base,
                                      uint64_t flags);
bool vm_address_space_map_kernel_range(vm_address_space_t* address_space,
                                       uintptr_t vaddr,
                                       uintptr_t paddr,
                                       size_t size,
                                       uint64_t flags);
bool vm_address_space_user_region_init(vm_address_space_t* address_space,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags);
bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags);
bool vm_address_space_register_fault_skip(vm_address_space_t* address_space,
                                          uint64_t cause,
                                          uintptr_t vaddr,
                                          size_t size);
bool vm_address_space_register_fault_resume_slot(
    vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t vaddr,
    size_t size,
    volatile uintptr_t* resume_pc_slot);
bool vm_address_space_enable(vm_address_space_t* address_space);
uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space);
uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space);
bool vm_process_create(vm_process_t* process, vm_address_space_t* address_space);
bool vm_process_activate(vm_process_t* process);
bool vm_process_is_active(const vm_process_t* process);
bool vm_process_remove_user_region(vm_process_t* process,
                                   vm_user_region_t* region);
bool vm_process_reset(vm_process_t* process);
bool vm_process_user_region_init(vm_process_t* process,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags);
bool vm_process_set_user_context(vm_process_t* process,
                                 uintptr_t entry_pc,
                                 uintptr_t user_sp);
bool vm_process_is_runnable(const vm_process_t* process);

void vm_object_reset(vm_object_t* object);
bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size);
bool vm_user_region_clear_object(vm_user_region_t* region);
bool vm_user_region_map_object(vm_user_region_t* region, vm_object_t* object);
bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object);
bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr);
bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size);
bool vm_range_is_kernel(uintptr_t vaddr, size_t size);
bool vm_range_is_user(uintptr_t vaddr, size_t size);
uintptr_t vm_kernel_base(void);
uintptr_t vm_kernel_limit(void);
uintptr_t vm_user_base(void);
uintptr_t vm_user_limit(void);
bool vm_handle_page_fault(const vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval);
void vm_flush_tlb(void);

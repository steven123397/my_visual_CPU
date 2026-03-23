#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VM_PAGE_READ (1ULL << 1)
#define VM_PAGE_WRITE (1ULL << 2)
#define VM_PAGE_EXEC (1ULL << 3)
#define VM_PAGE_USER (1ULL << 4)

bool vm_init(void);
bool vm_map_identity_1g(uintptr_t base, uint64_t flags);
bool vm_map_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags);
bool vm_map_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags);
bool vm_map_kernel_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags);
bool vm_map_user_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags);
bool vm_unmap_page(uintptr_t vaddr);
bool vm_range_is_kernel(uintptr_t vaddr, size_t size);
bool vm_range_is_user(uintptr_t vaddr, size_t size);
uintptr_t vm_kernel_base(void);
uintptr_t vm_kernel_limit(void);
uintptr_t vm_user_base(void);
uintptr_t vm_user_limit(void);
bool vm_register_fault_range(uintptr_t vaddr,
                             uintptr_t paddr,
                             size_t size,
                             uint64_t flags);
bool vm_register_user_fault_range(uintptr_t vaddr,
                                  uintptr_t paddr,
                                  size_t size,
                                  uint64_t flags);
bool vm_register_fault_skip(uint64_t cause, uintptr_t vaddr, size_t size);
bool vm_register_fault_resume_slot(uint64_t cause,
                                   uintptr_t vaddr,
                                   size_t size,
                                   volatile uintptr_t* resume_pc_slot);
bool vm_handle_page_fault(uint64_t cause, uint64_t epc, uint64_t tval);
void vm_flush_tlb(void);
bool vm_enable(void);

uintptr_t vm_root_table(void);
uint64_t vm_satp_value(void);
bool vm_is_enabled(void);

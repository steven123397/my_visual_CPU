#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vm.h"

#define LINUX_COMPAT_BRK_BASE ((uintptr_t)0x08000000U)
#define LINUX_COMPAT_MMAP_BASE ((uintptr_t)0x20000000U)

#ifndef LINUX_COMPAT_PROT_READ
#define LINUX_COMPAT_PROT_READ 0x1U
#define LINUX_COMPAT_PROT_WRITE 0x2U
#define LINUX_COMPAT_PROT_EXEC 0x4U
#endif
#ifndef LINUX_COMPAT_MAP_FIXED
#define LINUX_COMPAT_MAP_FIXED 0x10U
#endif

typedef struct LinuxCompatVmRegion {
    bool used;
    bool heap;
    vm_user_region_t region;
    vm_object_t object;
    uintptr_t vaddr;
    size_t length;
    uint32_t prot;
    uint32_t flags;
} linux_compat_vm_region_t;

typedef struct LinuxCompatVm {
    vm_address_space_t* address_space;
    vm_process_t* process;
    uintptr_t brk_base;
    uintptr_t program_break;
    uintptr_t next_mmap;
    linux_compat_vm_region_t regions[VM_PROCESS_MAX_USER_REGIONS];
} linux_compat_vm_t;

void linux_compat_vm_init(linux_compat_vm_t* vm,
                          vm_address_space_t* address_space,
                          vm_process_t* process);
uintptr_t linux_compat_vm_brk(linux_compat_vm_t* vm, uintptr_t new_break);
uintptr_t linux_compat_vm_mmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length,
                               uint32_t prot,
                               uint32_t flags);
uintptr_t linux_compat_vm_mmap_file(linux_compat_vm_t* vm,
                                    uintptr_t addr,
                                    size_t length,
                                    uint32_t prot,
                                    uint32_t flags,
                                    const uint8_t* data,
                                    size_t data_size,
                                    size_t file_offset);
uintptr_t linux_compat_vm_mremap(linux_compat_vm_t* vm,
                                 uintptr_t old_addr,
                                 size_t old_length,
                                 size_t new_length,
                                 uint32_t flags);
linux_compat_vm_region_t* linux_compat_vm_map_fixed(linux_compat_vm_t* vm,
                                                    uintptr_t addr,
                                                    size_t length,
                                                    uint32_t prot,
                                                    uint32_t flags);
int32_t linux_compat_vm_munmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length);
bool linux_compat_vm_read_user(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               void* out,
                               size_t length);
bool linux_compat_vm_write_user(linux_compat_vm_t* vm,
                                uintptr_t addr,
                                const void* data,
                                size_t length);
void linux_compat_vm_destroy(linux_compat_vm_t* vm);

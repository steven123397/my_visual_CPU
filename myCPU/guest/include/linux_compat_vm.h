#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vm.h"

#define LINUX_COMPAT_BRK_BASE ((uintptr_t)0x08000000U)
#define LINUX_COMPAT_MMAP_BASE ((uintptr_t)0x20000000U)

/* Linux compat VM 区间表：为 brk/mmap/mprotect 维护旁路用户区映射。 */
#ifndef LINUX_COMPAT_PROT_READ
#define LINUX_COMPAT_PROT_READ 0x1U
#define LINUX_COMPAT_PROT_WRITE 0x2U
#define LINUX_COMPAT_PROT_EXEC 0x4U
#endif
#ifndef LINUX_COMPAT_MAP_FIXED
#define LINUX_COMPAT_MAP_SHARED 0x01U
#define LINUX_COMPAT_MAP_PRIVATE 0x02U
#define LINUX_COMPAT_MAP_FIXED 0x10U
#define LINUX_COMPAT_MAP_ANONYMOUS 0x20U
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

/* 初始化 VM 旁路状态，绑定地址空间与进程。 */
void linux_compat_vm_init(linux_compat_vm_t* vm,
                          vm_address_space_t* address_space,
                          vm_process_t* process);
/* brk：调整 program_break，返回新/旧 break。 */
uintptr_t linux_compat_vm_brk(linux_compat_vm_t* vm, uintptr_t new_break);
/* 匿名 mmap：在用户区分配 length 字节。 */
uintptr_t linux_compat_vm_mmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length,
                               uint32_t prot,
                               uint32_t flags);
/* 文件 mmap：把 data 映射到用户区。 */
uintptr_t linux_compat_vm_mmap_file(linux_compat_vm_t* vm,
                                    uintptr_t addr,
                                    size_t length,
                                    uint32_t prot,
                                    uint32_t flags,
                                    const uint8_t* data,
                                    size_t data_size,
                                    size_t file_offset);
/* mremap：扩展/移动已有映射。 */
uintptr_t linux_compat_vm_mremap(linux_compat_vm_t* vm,
                                 uintptr_t old_addr,
                                 size_t old_length,
                                 size_t new_length,
                                 uint32_t flags);
/* 在固定地址映射一段区间（MAP_FIXED）。 */
linux_compat_vm_region_t* linux_compat_vm_map_fixed(linux_compat_vm_t* vm,
                                                    uintptr_t addr,
                                                    size_t length,
                                                    uint32_t prot,
                                                    uint32_t flags);
/* munmap：解除一段映射。 */
int32_t linux_compat_vm_munmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length);
/* mprotect：修改一段映射的权限。 */
int32_t linux_compat_vm_mprotect(linux_compat_vm_t* vm,
                                 uintptr_t addr,
                                 size_t length,
                                 uint32_t prot);
/* 从用户区读 length 字节到 out。 */
bool linux_compat_vm_read_user(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               void* out,
                               size_t length);
/* 向用户区写 length 字节。 */
bool linux_compat_vm_write_user(linux_compat_vm_t* vm,
                                uintptr_t addr,
                                const void* data,
                                size_t length);
/* 销毁 VM 旁路状态：释放所有映射与对象。 */
void linux_compat_vm_destroy(linux_compat_vm_t* vm);

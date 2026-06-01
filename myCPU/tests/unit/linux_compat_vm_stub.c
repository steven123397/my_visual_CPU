#include "../../guest/include/linux_compat_vm.h"

#include <stdbool.h>

void linux_compat_vm_init(linux_compat_vm_t* vm,
                          vm_address_space_t* address_space,
                          vm_process_t* process) {
    (void)vm;
    (void)address_space;
    (void)process;
}

uintptr_t linux_compat_vm_brk(linux_compat_vm_t* vm, uintptr_t new_break) {
    (void)vm;
    (void)new_break;
    return 0;
}

uintptr_t linux_compat_vm_mmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length,
                               uint32_t prot,
                               uint32_t flags) {
    (void)vm;
    (void)addr;
    (void)length;
    (void)prot;
    (void)flags;
    return (uintptr_t)-22;
}

int32_t linux_compat_vm_munmap(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               size_t length) {
    (void)vm;
    (void)addr;
    (void)length;
    return -22;
}

bool linux_compat_vm_read_user(linux_compat_vm_t* vm,
                               uintptr_t addr,
                               void* out,
                               size_t length) {
    (void)vm;
    (void)addr;
    (void)out;
    (void)length;
    return false;
}

bool linux_compat_vm_write_user(linux_compat_vm_t* vm,
                                uintptr_t addr,
                                const void* data,
                                size_t length) {
    (void)vm;
    (void)addr;
    (void)data;
    (void)length;
    return false;
}

void linux_compat_vm_destroy(linux_compat_vm_t* vm) {
    (void)vm;
}

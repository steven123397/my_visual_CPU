#include "../../guest/include/linux_compat_vm.h"
#include "../../guest/include/kernel_bringup.h"
#include "../../guest/include/trap.h"

#include <stdbool.h>
#include <string.h>

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

void trap_user_runtime_init(trap_user_runtime_t* user_runtime) {
    if (user_runtime != 0) {
        memset(user_runtime, 0, sizeof(*user_runtime));
    }
}

trap_context_t* trap_active_context(void) {
    static trap_context_t trap_context;

    return &trap_context;
}

bool kernel_bringup_create_linux_compat_address_space(
    vm_address_space_t** out_space,
    uint32_t mmio_mask) {
    (void)mmio_mask;
    if (out_space != 0) {
        *out_space = (vm_address_space_t*)1;
    }
    return out_space != 0;
}

bool vm_address_space_destroy(vm_address_space_t* address_space) {
    (void)address_space;
    return true;
}

bool vm_process_create(vm_process_t* process,
                       vm_address_space_t* address_space) {
    if (process == 0 || address_space == 0) {
        return false;
    }
    memset(process, 0, sizeof(*process));
    process->address_space = address_space;
    return true;
}

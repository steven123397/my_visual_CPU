#include "console.h"
#include "kernel_runtime.h"
#include "memory.h"
#include "monitor.h"
#include "panic.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "trap.h"
#include "vm.h"

static bool interactive_os_activate_trap_context(trap_context_t* trap_context) {
    return trap_context != NULL &&
           trap_context_activate(trap_context) &&
           trap_context_is_active(trap_context) &&
           trap_active_context() == trap_context;
}

static bool interactive_os_run_bringup(kernel_runtime_t* runtime) {
    vm_address_space_t* address_space = NULL;

    if (runtime == NULL) {
        return false;
    }

    memory_init();
    runtime_context_reset();
    trap_context_init(&runtime->trap_context);
    if (!interactive_os_activate_trap_context(&runtime->trap_context)) {
        return false;
    }
    console_putc('K');

    pmm_init();
    if (pmm_total_pages() == 0 || pmm_free_pages() == 0) {
        return false;
    }
    console_putc('M');

    if (!vm_address_space_create(&address_space) ||
        !vm_address_space_map_identity_1g(address_space,
                                          vm_kernel_base(),
                                          VM_PAGE_READ | VM_PAGE_WRITE |
                                              VM_PAGE_EXEC) ||
        !vm_address_space_map_identity_1g(address_space,
                                          0,
                                          VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_address_space_enable(address_space) ||
        !vm_address_space_is_enabled(address_space) ||
        !vm_address_space_is_active(address_space) ||
        riscv_read_satp() != vm_address_space_satp_value(address_space)) {
        return false;
    }

    runtime->address_space = address_space;
    console_putc('V');
    return true;
}

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!interactive_os_run_bringup(&runtime)) {
        panic_shutdown();
    }

    monitor_run(&runtime);
}

#include "kernel_runtime.h"

#include <stddef.h>

#include "console.h"
#include "memory.h"
#include "platform.h"
#include "pmm.h"
#include "runtime_context.h"
#include "storage.h"
#include "supervisor_runtime.h"

#if defined(__riscv)
#include "riscv.h"
#else
uint64_t riscv_read_satp(void);
#endif

static bool kernel_runtime_destroy_owned_address_space(kernel_runtime_t* runtime);
static bool demo_storage_signature_guardrail(const uint8_t* block,
                                             size_t block_size,
                                             void* context);

void kernel_runtime_init(kernel_runtime_t* runtime) {
    if (runtime == NULL) {
        return;
    }

    kernel_runtime_set_address_space(runtime, NULL);
    supervisor_runtime_interrupt_state_init(&runtime->interrupts);
}

trap_context_t* kernel_runtime_trap_context(kernel_runtime_t* runtime) {
    return runtime != NULL ? &runtime->trap_context : NULL;
}

void kernel_runtime_set_address_space(kernel_runtime_t* runtime,
                                      vm_address_space_t* address_space) {
    if (runtime == NULL) {
        return;
    }

    runtime->address_space = address_space;
}

vm_address_space_t* kernel_runtime_address_space(const kernel_runtime_t* runtime) {
    return runtime != NULL ? runtime->address_space : NULL;
}

supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state(
    kernel_runtime_t* runtime) {
    return runtime != NULL ? &runtime->interrupts : NULL;
}

const supervisor_runtime_interrupt_state_t* kernel_runtime_interrupt_state_const(
    const kernel_runtime_t* runtime) {
    return runtime != NULL ? &runtime->interrupts : NULL;
}

bool kernel_runtime_run_entry_bringup(kernel_runtime_t* runtime) {
    trap_context_t* trap_context = kernel_runtime_trap_context(runtime);

    if (trap_context == NULL) {
        return false;
    }

    if (!kernel_runtime_destroy_owned_address_space(runtime)) {
        return false;
    }

    supervisor_runtime_interrupt_state_init(&runtime->interrupts);
    memory_init();
    runtime_context_reset();
    trap_context_init(trap_context);
    return trap_context_activate(trap_context) &&
           trap_context_is_active(trap_context) &&
           trap_active_context() == trap_context;
}

static bool kernel_runtime_destroy_owned_address_space(kernel_runtime_t* runtime) {
    vm_address_space_t* address_space = kernel_runtime_address_space(runtime);

    if (address_space == NULL) {
        return true;
    }

    if (!vm_address_space_destroy(address_space)) {
        return false;
    }

    kernel_runtime_set_address_space(runtime, NULL);
    return true;
}

bool kernel_runtime_run_identity_superpage_bringup(kernel_runtime_t* runtime) {
    vm_address_space_t* address_space = NULL;

    if (runtime == NULL) {
        return false;
    }

    if (!kernel_runtime_destroy_owned_address_space(runtime) ||
        !kernel_runtime_run_entry_bringup(runtime)) {
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
        if (address_space != NULL && !vm_address_space_destroy(address_space)) {
            kernel_runtime_set_address_space(runtime, address_space);
        }
        return false;
    }

    kernel_runtime_set_address_space(runtime, address_space);
    console_putc('V');
    return true;
}

void kernel_runtime_begin_plic_supervisor_phase(char marker) {
    platform_plic_supervisor_init();
    if (marker != '\0') {
        console_putc(marker);
    }
}

bool kernel_runtime_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                     uint64_t timeout_delta) {
    return supervisor_runtime_wait_for_first_external_delivery(
        kernel_runtime_interrupt_state(runtime),
        timeout_delta);
}

bool kernel_runtime_wait_for_next_external_delivery(kernel_runtime_t* runtime,
                                                    uint64_t timeout_delta) {
    supervisor_runtime_interrupt_state_t* interrupts =
        kernel_runtime_interrupt_state(runtime);

    return interrupts != NULL &&
           supervisor_runtime_wait_for_next_counter(&interrupts->external_interrupts,
                                                   timeout_delta);
}

bool kernel_runtime_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                  uint64_t timer_delta,
                                                  uint64_t timeout_delta) {
    return supervisor_runtime_wait_for_first_timer_delivery(
        kernel_runtime_interrupt_state(runtime),
        timer_delta,
        timeout_delta);
}

bool kernel_runtime_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    return interrupts != NULL &&
           supervisor_runtime_schedule_platform_interrupts_and_wait(
               interrupts,
               timer_delta,
               timeout_delta);
}

bool kernel_runtime_complete_storage_probe(char marker) {
    storage_info_t storage_info = {0};

    if (!storage_probe(&storage_info) || storage_info.capacity_blocks == 0) {
        return false;
    }

    if (marker != '\0') {
        console_putc(marker);
    }
    return true;
}

bool kernel_runtime_complete_storage_lba0_check(
    char marker,
    kernel_runtime_storage_lba0_predicate_t predicate,
    void* context) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();
    const bool accepted =
        storage_page != NULL && predicate != NULL &&
        storage_read_block(0, storage_page) == 0 &&
        predicate(storage_page, STORAGE_BLOCK_SIZE, context);
    bool freed = false;

    if (storage_page != NULL) {
        freed = pmm_free_page(storage_page);
    }
    if (storage_page == NULL || !accepted || !freed) {
        return false;
    }

    if (marker != '\0') {
        console_putc(marker);
    }
    return true;
}

bool kernel_runtime_complete_demo_storage_signature_guardrail(char marker) {
    return kernel_runtime_complete_storage_lba0_check(
        marker,
        demo_storage_signature_guardrail,
        NULL);
}

bool kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
    supervisor_runtime_interrupt_state_t* interrupts,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    return kernel_runtime_complete_demo_storage_signature_guardrail('\0') &&
           kernel_runtime_wait_platform_interrupts(interrupts,
                                                   timer_delta,
                                                   timeout_delta);
}

static bool demo_storage_signature_guardrail(const uint8_t* block,
                                             size_t block_size,
                                             void* context) {
    (void)context;
    return block != NULL && block_size >= 4U &&
           block[0] == 'S' && block[1] == 't' &&
           block[2] == 'o' && block[3] == 'r';
}

bool kernel_runtime_bind_self_interrupt_handlers(
    kernel_runtime_t* runtime,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler) {
    supervisor_runtime_interrupt_state_t* interrupts =
        kernel_runtime_interrupt_state(runtime);

    if (interrupts == NULL) {
        return false;
    }

    supervisor_runtime_interrupt_state_bind_self_handlers(
        interrupts,
        expected_external_source_id,
        timer_post_handler,
        external_post_handler);
    return true;
}

bool kernel_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context) {
    kernel_runtime_t* runtime = (kernel_runtime_t*)context;

    return trap_context != NULL && runtime != NULL &&
           supervisor_runtime_install_external_counter_policy(
               trap_context,
               kernel_runtime_interrupt_state(runtime));
}

bool kernel_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context) {
    kernel_runtime_t* runtime = (kernel_runtime_t*)context;

    return trap_context != NULL && runtime != NULL &&
           supervisor_runtime_install_interrupt_counter_policies(
               trap_context,
               kernel_runtime_interrupt_state(runtime));
}

bool kernel_runtime_run_common_bringup(
    kernel_runtime_t* runtime,
    const kernel_bringup_options_t* options) {
    kernel_bringup_options_t bound_options;

    if (runtime == NULL) {
        return false;
    }

    if (options == NULL) {
        return false;
    }

    if (!kernel_runtime_destroy_owned_address_space(runtime)) {
        return false;
    }

    supervisor_runtime_interrupt_state_reset_counters(&runtime->interrupts);

    bound_options = *options;
    if (bound_options.pre_vm_setup != NULL &&
        bound_options.pre_vm_context == NULL) {
        bound_options.pre_vm_context = runtime;
    }

    return kernel_bringup_run_common(kernel_runtime_trap_context(runtime),
                                     &runtime->address_space,
                                     &bound_options);
}

bool kernel_runtime_run_bringup(
    kernel_runtime_t* runtime,
    uint32_t mmio_mask,
    uint64_t pmm_probe_marker,
    kernel_bringup_pre_vm_setup_t pre_vm_setup) {
    const kernel_bringup_options_t options = {
        .mmio_mask = mmio_mask,
        .pmm_probe_marker = pmm_probe_marker,
        .pre_vm_setup = pre_vm_setup,
        .pre_vm_context = NULL,
        .map_managed_memory = true,
    };

    return kernel_runtime_run_common_bringup(runtime, &options);
}

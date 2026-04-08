#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/memory.h"
#include "../../guest/include/platform.h"
#include "riscv.h"
#include "../../guest/include/user_program_smoke.h"

struct VmAddressSpace {
    bool enabled;
    bool active;
    uintptr_t root_table;
    uint64_t satp_value;
};

static uintptr_t g_text_start = MEM_BASE;
static uintptr_t g_text_end = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
static uintptr_t g_rodata_start = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
static uintptr_t g_rodata_end = MEM_BASE + 3U * MEMORY_PAGE_SIZE;
static uintptr_t g_data_start = MEM_BASE + 3U * MEMORY_PAGE_SIZE;
static uintptr_t g_heap_limit = MEM_BASE + 6U * MEMORY_PAGE_SIZE;
static uintptr_t g_managed_start = MEM_BASE + 4U * MEMORY_PAGE_SIZE;
static uintptr_t g_managed_end = MEM_BASE + 6U * MEMORY_PAGE_SIZE;

static uintptr_t g_memory_kernel_start_override = MEM_BASE;
static uint32_t g_active_memory_backing_page[MEMORY_PAGE_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static uint32_t g_active_memory_remap_page[MEMORY_PAGE_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static uint32_t g_active_memory_anon_page[MEMORY_PAGE_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static uint32_t g_active_memory_anon_tail_page[MEMORY_PAGE_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static volatile uint32_t g_active_memory_rodata_marker = 0;
static volatile uintptr_t g_active_memory_fault_resume_slot = 0;
static bool g_user_program_is_active = false;
static bool g_user_program_is_runnable = false;
static int g_user_program_activate_calls = 0;
static int g_user_program_deactivate_calls = 0;
static int g_user_program_unmap_region_base_page_calls = 0;
static user_program_region_id_t g_last_user_program_unmap_region_base_page_region =
    USER_PROGRAM_REGION_EXEC;
static bool g_user_program_unmap_region_base_page_result = false;
static int g_user_program_reset_object_calls = 0;
static user_program_object_id_t g_last_user_program_reset_object_id =
    USER_PROGRAM_OBJECT_EXEC;
static bool g_user_program_reset_object_result = false;
static int g_user_program_enter_calls = 0;
static bool g_user_program_enter_result = false;
static uint64_t g_sstatus = 0;
static int g_timer_schedule_delta_calls = 0;
static uint64_t g_last_timer_schedule_delta = 0;
static int g_platform_uart_enable_thre_irq_calls = 0;
static int g_trap_user_runtime_arm_timer_signal_calls = 0;
static bool g_trap_user_runtime_arm_timer_signal_result = false;
static bool g_trap_user_runtime_timer_signal_delivered_result = false;
static uint32_t* g_last_timer_signal_page = NULL;
static size_t g_last_timer_signal_index = 0;
static uint32_t g_last_timer_signal_value = 0;
static int g_trap_user_runtime_arm_external_signal_calls = 0;
static bool g_trap_user_runtime_arm_external_signal_result = false;
static bool g_trap_user_runtime_external_signal_delivered_result = false;
static uint32_t* g_last_external_signal_page = NULL;
static size_t g_last_external_signal_index = 0;
static uint32_t g_last_external_signal_value = 0;

static struct VmAddressSpace g_address_space = {0};
static int g_plan_standard_calls = 0;
static uintptr_t g_last_exec_symbol = 0;
static uintptr_t g_last_ecall_symbol = 0;
static bool g_plan_standard_result = true;
static int g_user_program_create_calls = 0;
static int g_user_program_destroy_calls = 0;
static int g_user_program_prepare_standard_calls = 0;
static int g_vm_object_init_physical_calls = 0;
static int g_vm_user_region_unmap_page_calls = 0;
static bool g_user_program_create_result = true;
static bool g_user_program_destroy_result = true;
static bool g_user_program_prepare_standard_result = true;
static bool g_vm_object_init_physical_result = true;
static bool g_vm_user_region_unmap_page_result = false;

static void reset_stub_state(void);
static void fill_created_program_layout(user_program_t* program);
static bool program_created(const user_program_t* program);
static int fail(const char* message);
static int test_smoke_init_and_plan_wrapper(void);
static int test_validate_standard_plan(void);
static int test_prepare_standard_rolls_back_failed_address_space_stage(void);
static int test_prepare_standard_rolls_back_failed_runtime_stage(void);
static int test_activate_supervisor_access_rolls_back_on_failure(void);
static int test_active_memory_requires_anon_unmap(void);
static int test_enter_round_arms_interrupt_signals(void);
static int test_enter_round_rolls_back_active_state_on_failure(void);

static bool program_created(const user_program_t* program) {
    return program != NULL &&
           program->user_task.address_space == &g_address_space &&
           program->user_task.process.address_space == &g_address_space;
}

static void fill_created_program_layout(user_program_t* program) {
    if (program == NULL) {
        return;
    }

    program->bootstrap.planned = true;
    program->bootstrap.exec_page_paddr = g_text_start;
    program->bootstrap.exec_vaddr = 4U * MEMORY_PAGE_SIZE;
    program->bootstrap.stack_vaddr = 5U * MEMORY_PAGE_SIZE;
    program->bootstrap.alias_vaddr = 6U * MEMORY_PAGE_SIZE;
    program->bootstrap.anon_vaddr = 7U * MEMORY_PAGE_SIZE;
    program->bootstrap.anon_tail_vaddr = 8U * MEMORY_PAGE_SIZE;
    program->bootstrap.entry_pc = program->bootstrap.exec_vaddr + 0x40U;
    program->bootstrap.expected_ecall_pc = program->bootstrap.exec_vaddr + 0x80U;
    program->bootstrap.user_sp = program->bootstrap.stack_vaddr + MEMORY_PAGE_SIZE;

    program->bootstrap.stack_region.address_space = &g_address_space;
    program->bootstrap.stack_region.vaddr = program->bootstrap.stack_vaddr;
    program->bootstrap.stack_region.size = MEMORY_PAGE_SIZE;
    program->bootstrap.stack_region.flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    program->bootstrap.stack_region.registered = true;

    program->bootstrap.alias_region.address_space = &g_address_space;
    program->bootstrap.alias_region.vaddr = program->bootstrap.alias_vaddr;
    program->bootstrap.alias_region.size = MEMORY_PAGE_SIZE;
    program->bootstrap.alias_region.flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    program->bootstrap.alias_region.registered = true;

    program->bootstrap.anon_region.address_space = &g_address_space;
    program->bootstrap.anon_region.vaddr = program->bootstrap.anon_vaddr;
    program->bootstrap.anon_region.size = MEMORY_PAGE_SIZE;
    program->bootstrap.anon_region.flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    program->bootstrap.anon_region.registered = true;

    program->bootstrap.anon_tail_region.address_space = &g_address_space;
    program->bootstrap.anon_tail_region.vaddr = program->bootstrap.anon_tail_vaddr;
    program->bootstrap.anon_tail_region.size = MEMORY_PAGE_SIZE;
    program->bootstrap.anon_tail_region.flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    program->bootstrap.anon_tail_region.registered = true;

    program->bootstrap.anon_object.initialized = true;
    program->bootstrap.anon_object.backing_kind = VM_OBJECT_BACKING_ANON;
    program->bootstrap.anon_object.size = 2U * MEMORY_PAGE_SIZE;
}

bool user_program_plan_standard(user_program_t* program,
                                uintptr_t exec_symbol,
                                uintptr_t ecall_symbol) {
    g_plan_standard_calls += 1;
    g_last_exec_symbol = exec_symbol;
    g_last_ecall_symbol = ecall_symbol;
    if (!g_plan_standard_result || program == NULL) {
        return false;
    }

    program->bootstrap.planned = true;
    return true;
}

bool user_program_create(user_program_t* program,
                         uintptr_t alias_backing_paddr,
                         uintptr_t user_stack_paddr) {
    (void)alias_backing_paddr;
    (void)user_stack_paddr;
    g_user_program_create_calls += 1;
    if (!g_user_program_create_result || program == NULL) {
        return false;
    }

    memset(program, 0, sizeof(*program));
    g_address_space.enabled = false;
    g_address_space.active = false;
    g_address_space.root_table = MEM_BASE;
    g_address_space.satp_value = RISCV_SATP_MODE_SV39 | (MEM_BASE >> 12);
    program->user_task.address_space = &g_address_space;
    program->user_task.process.address_space = &g_address_space;
    fill_created_program_layout(program);
    return true;
}

bool user_program_destroy(user_program_t* program) {
    g_user_program_destroy_calls += 1;
    if (!g_user_program_destroy_result || program == NULL) {
        return false;
    }

    memset(program, 0, sizeof(*program));
    g_user_program_is_active = false;
    g_user_program_is_runnable = false;
    return true;
}

bool user_program_prepare_standard(
    user_program_t* program,
    trap_context_t* trap_context,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context) {
    (void)program;
    (void)trap_context;
    (void)arg0;
    (void)trap_stack_base;
    (void)trap_stack_size;
    (void)validate;
    (void)validate_context;
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_user_program_prepare_standard_calls += 1;
    g_user_program_is_runnable =
        g_user_program_prepare_standard_result && program != NULL;
    return g_user_program_is_runnable;
}

vm_address_space_t* user_program_address_space(user_program_t* program) {
    return program_created(program) ? program->user_task.address_space : NULL;
}

vm_process_t* user_program_process(user_program_t* program) {
    return program_created(program) ? &program->user_task.process : NULL;
}

trap_user_runtime_t* user_program_runtime(user_program_t* program) {
    return program_created(program) ? &program->user_task.runtime : NULL;
}

vm_user_region_t* user_program_region(user_program_t* program,
                                      user_program_region_id_t region_id) {
    if (!program_created(program)) {
        return NULL;
    }

    switch (region_id) {
    case USER_PROGRAM_REGION_STACK:
        return &program->bootstrap.stack_region;
    case USER_PROGRAM_REGION_ALIAS:
        return &program->bootstrap.alias_region;
    case USER_PROGRAM_REGION_ANON:
        return &program->bootstrap.anon_region;
    case USER_PROGRAM_REGION_ANON_TAIL:
        return &program->bootstrap.anon_tail_region;
    default:
        return &program->bootstrap.exec_region;
    }
}

vm_object_t* user_program_object(user_program_t* program,
                                 user_program_object_id_t object_id) {
    if (!program_created(program)) {
        return NULL;
    }

    switch (object_id) {
    case USER_PROGRAM_OBJECT_ANON:
        return &program->bootstrap.anon_object;
    case USER_PROGRAM_OBJECT_EXEC:
        return &program->bootstrap.exec_object;
    case USER_PROGRAM_OBJECT_STACK:
        return &program->bootstrap.stack_object;
    default:
        return &program->bootstrap.alias_object;
    }
}

uintptr_t user_program_value(const user_program_t* program,
                             user_program_value_id_t value_id) {
    if (program == NULL || !program->bootstrap.planned) {
        return 0;
    }

    switch (value_id) {
    case USER_PROGRAM_VALUE_EXEC_PAGE_PADDR:
        return program->bootstrap.exec_page_paddr;
    case USER_PROGRAM_VALUE_EXEC_VADDR:
        return program->bootstrap.exec_vaddr;
    case USER_PROGRAM_VALUE_STACK_VADDR:
        return program->bootstrap.stack_vaddr;
    case USER_PROGRAM_VALUE_ALIAS_VADDR:
        return program->bootstrap.alias_vaddr;
    case USER_PROGRAM_VALUE_ANON_VADDR:
        return program->bootstrap.anon_vaddr;
    case USER_PROGRAM_VALUE_ANON_TAIL_VADDR:
        return program->bootstrap.anon_tail_vaddr;
    case USER_PROGRAM_VALUE_ENTRY_PC:
        return program->bootstrap.entry_pc;
    case USER_PROGRAM_VALUE_EXPECTED_ECALL_PC:
        return program->bootstrap.expected_ecall_pc;
    case USER_PROGRAM_VALUE_USER_SP:
        return program->bootstrap.user_sp;
    }

    return 0;
}

bool user_program_region_contains(const user_program_t* program,
                                  user_program_region_id_t region_id,
                                  uintptr_t vaddr,
                                  size_t size) {
    vm_user_region_t* region = user_program_region((user_program_t*)program, region_id);

    return region != NULL &&
           vaddr >= region->vaddr &&
           size <= region->size &&
           vaddr + size <= region->vaddr + region->size;
}

bool user_program_is_active(const user_program_t* program) {
    (void)program;
    return g_user_program_is_active;
}

bool user_program_is_runnable(const user_program_t* program) {
    (void)program;
    return g_user_program_is_runnable;
}

bool user_program_activate(user_program_t* program) {
    (void)program;
    g_user_program_activate_calls += 1;
    g_user_program_is_active = true;
    g_user_program_is_runnable = true;
    return true;
}

bool user_program_deactivate(user_program_t* program) {
    (void)program;
    g_user_program_deactivate_calls += 1;
    g_user_program_is_active = false;
    g_user_program_is_runnable = false;
    return true;
}

bool user_program_enter(const user_program_t* program) {
    (void)program;
    g_user_program_enter_calls += 1;
    return g_user_program_enter_result;
}

bool user_program_map_object_region(user_program_t* program,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object) {
    if (program == NULL || region == NULL || object == NULL) {
        return false;
    }

    region->address_space = user_program_address_space(program);
    region->vaddr = vaddr;
    region->size = size;
    region->flags = flags;
    region->registered = true;
    region->object = object;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_FAULT;
    return true;
}

bool user_program_unmap_region_page(user_program_t* program,
                                    user_program_region_id_t region_id,
                                    uintptr_t vaddr) {
    (void)program;
    (void)region_id;
    (void)vaddr;
    return false;
}

bool user_program_unmap_region_base_page(user_program_t* program,
                                         user_program_region_id_t region_id) {
    g_user_program_unmap_region_base_page_calls += 1;
    g_last_user_program_unmap_region_base_page_region = region_id;
    return program != NULL && g_user_program_unmap_region_base_page_result;
}

bool user_program_set_region_fault_object(user_program_t* program,
                                          user_program_region_id_t region_id,
                                          vm_object_t* object) {
    (void)program;
    (void)region_id;
    (void)object;
    return false;
}

bool user_program_set_fault_object_region_at(user_program_t* program,
                                             vm_user_region_t* region,
                                             uintptr_t vaddr,
                                             size_t size,
                                             uint64_t flags,
                                             vm_object_t* object,
                                             size_t object_offset) {
    (void)program;
    (void)region;
    (void)vaddr;
    (void)size;
    (void)flags;
    (void)object;
    (void)object_offset;
    return false;
}

bool user_program_rebind_region_fault_object(
    user_program_t* program,
    user_program_region_id_t region_id,
    vm_object_t* object) {
    (void)program;
    (void)region_id;
    return object != NULL;
}

bool user_program_reset_object(user_program_t* program,
                               user_program_object_id_t object_id) {
    g_user_program_reset_object_calls += 1;
    g_last_user_program_reset_object_id = object_id;
    return program != NULL && g_user_program_reset_object_result;
}

uintptr_t memory_kernel_start(void) {
    return g_memory_kernel_start_override;
}

uintptr_t memory_text_start(void) {
    return g_text_start;
}

uintptr_t memory_text_end(void) {
    return g_text_end;
}

uintptr_t memory_rodata_start(void) {
    return g_rodata_start;
}

uintptr_t memory_rodata_end(void) {
    return g_rodata_end;
}

uintptr_t memory_data_start(void) {
    return g_data_start;
}

uintptr_t memory_heap_limit(void) {
    return g_heap_limit;
}

uintptr_t pmm_managed_start(void) {
    return g_managed_start;
}

uintptr_t pmm_managed_end(void) {
    return g_managed_end;
}

size_t pmm_free_pages(void) {
    return 0;
}

bool vm_address_space_map_identity_1g(vm_address_space_t* address_space,
                                      uintptr_t base,
                                      uint64_t flags) {
    (void)base;
    (void)flags;
    return address_space != NULL;
}

bool vm_address_space_map_kernel_range(vm_address_space_t* address_space,
                                       uintptr_t vaddr,
                                       uintptr_t paddr,
                                       size_t size,
                                       uint64_t flags) {
    (void)address_space;
    (void)vaddr;
    (void)paddr;
    (void)size;
    return flags != VM_PAGE_WRITE && (flags & VM_PAGE_USER) == 0;
}

bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags) {
    (void)address_space;
    (void)vaddr;
    (void)paddr;
    (void)size;
    (void)flags;
    return false;
}

bool vm_address_space_register_fault_skip(vm_address_space_t* address_space,
                                          uint64_t cause,
                                          uintptr_t vaddr,
                                          size_t size) {
    (void)address_space;
    (void)cause;
    (void)vaddr;
    (void)size;
    return true;
}

bool vm_address_space_register_fault_resume_slot(
    vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t vaddr,
    size_t size,
    volatile uintptr_t* resume_pc_slot) {
    (void)address_space;
    (void)cause;
    (void)vaddr;
    (void)size;
    (void)resume_pc_slot;
    return true;
}

uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space) {
    return address_space != NULL ? address_space->root_table : 0;
}

uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space) {
    return address_space != NULL ? address_space->satp_value : 0;
}

bool vm_address_space_is_active(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->active;
}

bool vm_address_space_is_enabled(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->enabled;
}

uintptr_t vm_kernel_base(void) {
    return memory_kernel_start();
}

uintptr_t vm_kernel_limit(void) {
    return memory_heap_limit();
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return vaddr < memory_kernel_start() &&
           size != 0 &&
           vaddr + size > vaddr &&
           vaddr + size <= memory_kernel_start();
}

bool vm_range_is_kernel(uintptr_t vaddr, size_t size) {
    return size != 0 &&
           vaddr >= memory_kernel_start() &&
           vaddr + size > vaddr &&
           vaddr + size <= memory_heap_limit();
}

bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size) {
    (void)paddr;
    g_vm_object_init_physical_calls += 1;
    if (!g_vm_object_init_physical_result || object == NULL) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_PHYSICAL;
    object->size = size;
    return true;
}

bool vm_process_user_region_init(vm_process_t* process,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags) {
    (void)process;
    (void)region;
    (void)vaddr;
    (void)size;
    (void)flags;
    return false;
}

bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object) {
    (void)region;
    (void)object;
    return false;
}

bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr) {
    g_vm_user_region_unmap_page_calls += 1;
    return region != NULL && region->registered &&
           vaddr == region->vaddr && g_vm_user_region_unmap_page_result;
}

uint64_t riscv_read_satp(void) {
    return g_address_space.satp_value;
}

void riscv_write_satp(uint64_t value) {
    g_address_space.satp_value = value;
}

uint64_t riscv_read_sstatus(void) {
    return g_sstatus;
}

void riscv_clear_sstatus_bits(uint64_t value) {
    g_sstatus &= ~value;
}

void riscv_set_sstatus_bits(uint64_t value) {
    g_sstatus |= value;
}

bool trap_user_runtime_arm_timer_signal(trap_user_runtime_t* user_runtime,
                                        uint32_t* page,
                                        size_t word_index,
                                        uint32_t value) {
    g_trap_user_runtime_arm_timer_signal_calls += 1;
    g_last_timer_signal_page = page;
    g_last_timer_signal_index = word_index;
    g_last_timer_signal_value = value;
    if (user_runtime != NULL) {
        user_runtime->timer_signal.page = page;
        user_runtime->timer_signal.word_index = word_index;
        user_runtime->timer_signal.value = value;
        user_runtime->timer_signal.armed = g_trap_user_runtime_arm_timer_signal_result;
        user_runtime->timer_signal.delivered =
            g_trap_user_runtime_timer_signal_delivered_result;
    }
    return g_trap_user_runtime_arm_timer_signal_result;
}

bool trap_user_runtime_timer_signal_delivered(
    const trap_user_runtime_t* user_runtime) {
    (void)user_runtime;
    return g_trap_user_runtime_timer_signal_delivered_result;
}

bool trap_user_runtime_arm_external_signal(trap_user_runtime_t* user_runtime,
                                           uint32_t* page,
                                           size_t word_index,
                                           uint32_t value) {
    g_trap_user_runtime_arm_external_signal_calls += 1;
    g_last_external_signal_page = page;
    g_last_external_signal_index = word_index;
    g_last_external_signal_value = value;
    if (user_runtime != NULL) {
        user_runtime->external_signal.page = page;
        user_runtime->external_signal.word_index = word_index;
        user_runtime->external_signal.value = value;
        user_runtime->external_signal.armed =
            g_trap_user_runtime_arm_external_signal_result;
        user_runtime->external_signal.delivered =
            g_trap_user_runtime_external_signal_delivered_result;
    }
    return g_trap_user_runtime_arm_external_signal_result;
}

bool trap_user_runtime_external_signal_delivered(
    const trap_user_runtime_t* user_runtime) {
    (void)user_runtime;
    return g_trap_user_runtime_external_signal_delivered_result;
}

void platform_uart_enable_thre_irq(void) {
    g_platform_uart_enable_thre_irq_calls += 1;
}

void timer_schedule_delta(uint64_t delta) {
    g_timer_schedule_delta_calls += 1;
    g_last_timer_schedule_delta = delta;
}

vm_address_space_t* runtime_context_active_address_space(void) {
    return NULL;
}

vm_process_t* runtime_context_active_process(void) {
    return NULL;
}

trap_context_t* trap_active_context(void) {
    return NULL;
}

static void reset_stub_state(void) {
    g_text_start = MEM_BASE;
    g_text_end = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
    g_rodata_start = MEM_BASE + 2U * MEMORY_PAGE_SIZE;
    g_rodata_end = MEM_BASE + 3U * MEMORY_PAGE_SIZE;
    g_data_start = MEM_BASE + 3U * MEMORY_PAGE_SIZE;
    g_heap_limit = MEM_BASE + 6U * MEMORY_PAGE_SIZE;
    g_managed_start = MEM_BASE + 4U * MEMORY_PAGE_SIZE;
    g_managed_end = MEM_BASE + 6U * MEMORY_PAGE_SIZE;
    memset(&g_address_space, 0, sizeof(g_address_space));
    g_plan_standard_calls = 0;
    g_last_exec_symbol = 0;
    g_last_ecall_symbol = 0;
    g_plan_standard_result = true;
    g_user_program_create_calls = 0;
    g_user_program_destroy_calls = 0;
    g_user_program_prepare_standard_calls = 0;
    g_vm_object_init_physical_calls = 0;
    g_vm_user_region_unmap_page_calls = 0;
    g_user_program_create_result = true;
    g_user_program_destroy_result = true;
    g_user_program_prepare_standard_result = true;
    g_vm_object_init_physical_result = true;
    g_vm_user_region_unmap_page_result = false;
    g_memory_kernel_start_override = MEM_BASE;
    memset(g_active_memory_backing_page, 0, sizeof(g_active_memory_backing_page));
    memset(g_active_memory_remap_page, 0, sizeof(g_active_memory_remap_page));
    memset(g_active_memory_anon_page, 0, sizeof(g_active_memory_anon_page));
    memset(g_active_memory_anon_tail_page, 0, sizeof(g_active_memory_anon_tail_page));
    g_active_memory_rodata_marker = 0;
    g_active_memory_fault_resume_slot = 0;
    g_user_program_is_active = false;
    g_user_program_is_runnable = false;
    g_user_program_activate_calls = 0;
    g_user_program_deactivate_calls = 0;
    g_user_program_unmap_region_base_page_calls = 0;
    g_last_user_program_unmap_region_base_page_region = USER_PROGRAM_REGION_EXEC;
    g_user_program_unmap_region_base_page_result = false;
    g_user_program_reset_object_calls = 0;
    g_last_user_program_reset_object_id = USER_PROGRAM_OBJECT_EXEC;
    g_user_program_reset_object_result = false;
    g_user_program_enter_calls = 0;
    g_user_program_enter_result = false;
    g_sstatus = 0;
    g_timer_schedule_delta_calls = 0;
    g_last_timer_schedule_delta = 0;
    g_platform_uart_enable_thre_irq_calls = 0;
    g_trap_user_runtime_arm_timer_signal_calls = 0;
    g_trap_user_runtime_arm_timer_signal_result = false;
    g_trap_user_runtime_timer_signal_delivered_result = false;
    g_last_timer_signal_page = NULL;
    g_last_timer_signal_index = 0;
    g_last_timer_signal_value = 0;
    g_trap_user_runtime_arm_external_signal_calls = 0;
    g_trap_user_runtime_arm_external_signal_result = false;
    g_trap_user_runtime_external_signal_delivered_result = false;
    g_last_external_signal_page = NULL;
    g_last_external_signal_index = 0;
    g_last_external_signal_value = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_smoke_init_and_plan_wrapper(void) {
    user_program_smoke_t smoke;
    user_program_t program;

    reset_stub_state();
    memset(&smoke, 0xA5, sizeof(smoke));
    memset(&program, 0, sizeof(program));
    user_program_smoke_init(&smoke);

    if (!user_program_smoke_is_reset(&smoke)) {
        return fail("expected smoke init to clear scratch regions and object state");
    }

    if (!user_program_smoke_plan_standard(&program, 0x1234U, 0x5678U) ||
        g_plan_standard_calls != 1 || g_last_exec_symbol != 0x1234U ||
        g_last_ecall_symbol != 0x5678U) {
        return fail("expected smoke plan wrapper to forward program symbols");
    }

    return 0;
}

static int test_validate_standard_plan(void) {
    user_program_t program = {0};

    reset_stub_state();
    program.bootstrap.planned = true;
    program.bootstrap.exec_page_paddr = g_text_start;
    program.bootstrap.exec_vaddr = 4U * MEMORY_PAGE_SIZE;
    program.bootstrap.stack_vaddr = 5U * MEMORY_PAGE_SIZE;
    program.bootstrap.alias_vaddr = 6U * MEMORY_PAGE_SIZE;
    program.bootstrap.anon_vaddr = 7U * MEMORY_PAGE_SIZE;
    program.bootstrap.anon_tail_vaddr = 8U * MEMORY_PAGE_SIZE;
    program.bootstrap.entry_pc = program.bootstrap.exec_vaddr + 0x40U;
    program.bootstrap.expected_ecall_pc = program.bootstrap.exec_vaddr + 0x80U;
    program.bootstrap.user_sp = program.bootstrap.stack_vaddr + MEMORY_PAGE_SIZE;

    if (!user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to accept canonical layout");
    }

    program.bootstrap.user_sp = program.bootstrap.stack_vaddr;
    if (user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to reject bad user stack top");
    }

    program.bootstrap.user_sp = program.bootstrap.stack_vaddr + MEMORY_PAGE_SIZE;
    program.bootstrap.exec_page_paddr = g_text_end;
    if (user_program_smoke_validate_standard_plan(&program, 0, MEM_BASE)) {
        return fail("expected smoke plan validator to reject exec page outside text");
    }

    return 0;
}

static int test_prepare_standard_rolls_back_failed_address_space_stage(void) {
    user_program_smoke_t smoke;
    user_program_t program = {0};
    trap_context_t trap_context = {0};
    volatile uintptr_t fault_resume_pc_slot = 0;
    uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE] = {0};
    const user_program_smoke_prepare_t prepare = {
        .trap_context = &trap_context,
        .backing_page_paddr = MEM_BASE,
        .user_stack_paddr = MEM_BASE + MEMORY_PAGE_SIZE,
        .remap_page_paddr = MEM_BASE + 2U * MEMORY_PAGE_SIZE,
        .fault_skip_vaddr = 0x2000U,
        .fault_skip_size = 4U,
        .fault_resume_vaddr = 0x3000U,
        .fault_resume_size = 4U,
        .fault_resume_pc_slot = &fault_resume_pc_slot,
        .arg0 = 1U,
        .trap_stack_base = trap_stack,
        .trap_stack_size = sizeof(trap_stack),
        .validate = NULL,
        .validate_context = NULL,
        .supervisor_timer_post_handler = NULL,
        .supervisor_timer_post_context = NULL,
        .supervisor_external_post_handler = NULL,
        .supervisor_external_post_context = NULL,
    };

    reset_stub_state();
    user_program_smoke_init(&smoke);
    g_vm_object_init_physical_result = false;

    if (user_program_smoke_prepare_standard(&smoke, &program, &prepare)) {
        return fail("expected prepare_standard to fail when remap object init fails");
    }

    if (g_user_program_create_calls != 1 || g_vm_object_init_physical_calls != 1) {
        return fail("expected prepare_standard to reach the address-space orchestration failure point");
    }

    if (!user_program_smoke_is_reset(&smoke) ||
        g_user_program_destroy_calls != 1 ||
        program_created(&program) || g_user_program_prepare_standard_calls != 0) {
        return fail("expected prepare_standard failure to rollback smoke/program state before runtime prepare");
    }

    return 0;
}

static int test_prepare_standard_rolls_back_failed_runtime_stage(void) {
    user_program_smoke_t smoke;
    user_program_t program = {0};
    trap_context_t trap_context = {0};
    volatile uintptr_t fault_resume_pc_slot = 0;
    uint8_t trap_stack[TRAP_USER_RUNTIME_MIN_STACK_SIZE] = {0};
    const user_program_smoke_prepare_t prepare = {
        .trap_context = &trap_context,
        .backing_page_paddr = MEM_BASE,
        .user_stack_paddr = MEM_BASE + MEMORY_PAGE_SIZE,
        .remap_page_paddr = MEM_BASE + 2U * MEMORY_PAGE_SIZE,
        .fault_skip_vaddr = 0x2000U,
        .fault_skip_size = 4U,
        .fault_resume_vaddr = 0x3000U,
        .fault_resume_size = 4U,
        .fault_resume_pc_slot = &fault_resume_pc_slot,
        .arg0 = 1U,
        .trap_stack_base = trap_stack,
        .trap_stack_size = sizeof(trap_stack),
        .validate = NULL,
        .validate_context = NULL,
        .supervisor_timer_post_handler = NULL,
        .supervisor_timer_post_context = NULL,
        .supervisor_external_post_handler = NULL,
        .supervisor_external_post_context = NULL,
    };

    reset_stub_state();
    user_program_smoke_init(&smoke);
    g_vm_user_region_unmap_page_result = true;
    g_user_program_prepare_standard_result = false;

    if (user_program_smoke_prepare_standard(&smoke, &program, &prepare)) {
        return fail("expected prepare_standard to fail when runtime prepare fails");
    }

    if (g_user_program_create_calls != 1 ||
        g_vm_object_init_physical_calls != 1 ||
        g_vm_user_region_unmap_page_calls != 1 ||
        g_user_program_prepare_standard_calls != 1) {
        return fail("expected prepare_standard to reach the runtime orchestration failure point");
    }

    if (!user_program_smoke_is_reset(&smoke) ||
        g_user_program_destroy_calls != 1 ||
        program_created(&program)) {
        return fail("expected runtime-stage failure to rollback smoke/program state");
    }

    return 0;
}

static int test_activate_supervisor_access_rolls_back_on_failure(void) {
    user_program_smoke_t smoke = {0};
    user_program_t program = {0};
    trap_context_t trap_context = {0};

    reset_stub_state();
    user_program_smoke_init(&smoke);
    if (!user_program_create(&program, MEM_BASE, MEM_BASE + MEMORY_PAGE_SIZE)) {
        return fail("failed to bootstrap user program for activate rollback test");
    }

    smoke.program = &program;
    g_user_program_is_runnable = true;
    if (user_program_smoke_activate_supervisor_access(&smoke, &trap_context)) {
        return fail("expected activate helper to fail under mismatched active-context stubs");
    }

    if (g_user_program_activate_calls != 1 || g_user_program_deactivate_calls != 1 ||
        g_user_program_is_active || g_user_program_is_runnable) {
        return fail("expected failed activate helper to best-effort deactivate the program");
    }

    return 0;
}

static int test_active_memory_requires_anon_unmap(void) {
    user_program_smoke_t smoke = {0};
    user_program_t program = {0};
    user_program_smoke_active_phase_t phase = {0};

    reset_stub_state();
    user_program_smoke_init(&smoke);
    if (!user_program_plan_standard(&program, 0x1000U, 0x2000U) ||
        !user_program_create(&program, MEM_BASE, MEM_BASE + MEMORY_PAGE_SIZE)) {
        return fail("failed to bootstrap user program for active memory test");
    }

    program.bootstrap.alias_vaddr = (uintptr_t)g_active_memory_backing_page;
    program.bootstrap.alias_region.vaddr = program.bootstrap.alias_vaddr;
    program.bootstrap.anon_vaddr = (uintptr_t)g_active_memory_anon_page;
    program.bootstrap.anon_region.vaddr = program.bootstrap.anon_vaddr;
    program.bootstrap.anon_tail_vaddr =
        (uintptr_t)g_active_memory_anon_tail_page;
    program.bootstrap.anon_tail_region.vaddr =
        program.bootstrap.anon_tail_vaddr;

    smoke.program = &program;
    g_user_program_is_active = true;
    g_user_program_is_runnable = true;
    g_user_program_reset_object_result = false;
    g_user_program_unmap_region_base_page_result = false;
    g_active_memory_backing_page[0] = 0xBEEFBEEFU;
    g_active_memory_remap_page[0] = g_active_memory_backing_page[0];
    g_active_memory_rodata_marker = 0xDEADBEEFU;
    g_active_memory_fault_resume_slot = 1U;

    phase.alias_store_value = 0x11111111U;
    phase.backing_store_value = 0x22222222U;
    phase.anon_value0 = 0x33333333U;
    phase.anon_value1 = 0x44444444U;
    phase.anon_tail_value0 = 0x55555555U;
    phase.anon_tail_value1 = 0x66666666U;
    phase.remap_store_value = 0x77777777U;
    phase.rodata_marker = &g_active_memory_rodata_marker;
    phase.rodata_expected = 0xDEADBEEFU;
    phase.instruction_fault_target = 0x1234U;
    phase.fault_resume_pc_slot = &g_active_memory_fault_resume_slot;

    g_sstatus = RISCV_SSTATUS_SUM;
    g_memory_kernel_start_override = UINTPTR_MAX;
    if (user_program_smoke_exercise_active_memory(
            &smoke,
            g_active_memory_backing_page,
            g_active_memory_remap_page,
            &phase)) {
        return fail("expected active memory exercise to fail when anon unmap fails");
    }

    if (g_user_program_reset_object_calls != 1 ||
        g_last_user_program_reset_object_id != USER_PROGRAM_OBJECT_ANON) {
        return fail("expected active memory exercise to reach anon object reset before anon unmap");
    }

    if (g_user_program_unmap_region_base_page_calls < 1) {
        return fail("expected active memory exercise to attempt anon unmap");
    }

    return 0;
}

static int test_enter_round_arms_interrupt_signals(void) {
    user_program_smoke_t smoke = {0};
    user_program_t program = {0};
    trap_context_t trap_context = {0};
    uint32_t timer_signal_page[4] = {0};
    uint32_t external_signal_page[4] = {0};
    const user_program_smoke_round_t round = {
        .expected_trap_context = &trap_context,
        .timer_signal_page = timer_signal_page,
        .timer_signal_index = 1U,
        .timer_signal_value = 0x13572468U,
        .external_signal_page = external_signal_page,
        .external_signal_index = 2U,
        .external_signal_value = 0x24681357U,
        .timer_delta = 77U,
    };

    reset_stub_state();
    user_program_smoke_init(&smoke);
    if (!user_program_create(&program, MEM_BASE, MEM_BASE + MEMORY_PAGE_SIZE)) {
        return fail("failed to bootstrap user program for interrupt round test");
    }

    smoke.program = &program;
    g_user_program_is_active = true;
    g_user_program_is_runnable = true;
    g_user_program_unmap_region_base_page_result = true;
    g_user_program_enter_result = true;
    g_trap_user_runtime_arm_timer_signal_result = true;
    g_trap_user_runtime_arm_external_signal_result = true;
    g_sstatus = RISCV_SSTATUS_SUM | RISCV_SSTATUS_SIE;

    if (!user_program_smoke_enter_round(&smoke, &round)) {
        return fail("expected interrupt round helper to arm signals and enter the guest");
    }

    if ((g_sstatus & RISCV_SSTATUS_SUM) != 0 ||
        (g_sstatus & RISCV_SSTATUS_SIE) != 0) {
        return fail("expected interrupt round helper to clear SUM/SIE before entering the guest");
    }

    if (g_user_program_unmap_region_base_page_calls != 1 ||
        g_last_user_program_unmap_region_base_page_region != USER_PROGRAM_REGION_ALIAS) {
        return fail("expected interrupt round helper to unmap the alias base page exactly once");
    }

    if (g_trap_user_runtime_arm_timer_signal_calls != 1 ||
        g_last_timer_signal_page != timer_signal_page ||
        g_last_timer_signal_index != 1U ||
        g_last_timer_signal_value != 0x13572468U) {
        return fail("expected interrupt round helper to arm the timer signal with the provided payload");
    }

    if (g_trap_user_runtime_arm_external_signal_calls != 1 ||
        g_last_external_signal_page != external_signal_page ||
        g_last_external_signal_index != 2U ||
        g_last_external_signal_value != 0x24681357U) {
        return fail("expected interrupt round helper to arm the external signal with the provided payload");
    }

    if (g_platform_uart_enable_thre_irq_calls != 1 ||
        g_timer_schedule_delta_calls != 1 ||
        g_last_timer_schedule_delta != 77U ||
        g_user_program_enter_calls != 1) {
        return fail("expected interrupt round helper to enable UART, schedule the timer, and enter once");
    }

    return 0;
}

static int test_enter_round_rolls_back_active_state_on_failure(void) {
    user_program_smoke_t smoke = {0};
    user_program_t program = {0};
    trap_context_t trap_context = {0};
    uint32_t timer_signal_page[4] = {0};
    uint32_t external_signal_page[4] = {0};
    const user_program_smoke_round_t round = {
        .expected_trap_context = &trap_context,
        .timer_signal_page = timer_signal_page,
        .timer_signal_index = 1U,
        .timer_signal_value = 0x13572468U,
        .external_signal_page = external_signal_page,
        .external_signal_index = 2U,
        .external_signal_value = 0x24681357U,
        .timer_delta = 77U,
    };

    reset_stub_state();
    user_program_smoke_init(&smoke);
    if (!user_program_create(&program, MEM_BASE, MEM_BASE + MEMORY_PAGE_SIZE)) {
        return fail("failed to bootstrap user program for enter rollback test");
    }

    smoke.program = &program;
    g_user_program_is_active = true;
    g_user_program_is_runnable = true;
    g_user_program_unmap_region_base_page_result = true;
    g_trap_user_runtime_arm_timer_signal_result = true;
    g_trap_user_runtime_arm_external_signal_result = false;
    g_sstatus = RISCV_SSTATUS_SUM | RISCV_SSTATUS_SIE;

    if (user_program_smoke_enter_round(&smoke, &round)) {
        return fail("expected interrupt round helper to fail when external signal arm fails");
    }

    if (g_user_program_deactivate_calls != 1 || g_user_program_is_active ||
        g_user_program_is_runnable) {
        return fail("expected failed interrupt round to best-effort deactivate the program");
    }

    return 0;
}

int main(void) {
    if (test_smoke_init_and_plan_wrapper() != 0 ||
        test_validate_standard_plan() != 0 ||
        test_prepare_standard_rolls_back_failed_address_space_stage() != 0 ||
        test_prepare_standard_rolls_back_failed_runtime_stage() != 0 ||
        test_activate_supervisor_access_rolls_back_on_failure() != 0 ||
        test_active_memory_requires_anon_unmap() != 0 ||
        test_enter_round_arms_interrupt_signals() != 0 ||
        test_enter_round_rolls_back_active_state_on_failure() != 0) {
        return 1;
    }

    return 0;
}

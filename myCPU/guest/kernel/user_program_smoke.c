#include "user_program_smoke.h"

#include "memory.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"

static void clear_region(vm_user_region_t* region) {
    if (region == NULL) {
        return;
    }

    region->address_space = NULL;
    region->vaddr = 0;
    region->size = 0;
    region->flags = 0;
    region->registered = false;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
}

static void clear_object(vm_object_t* object) {
    if (object == NULL) {
        return;
    }

    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = NULL;
    object->backing.anon.page_count = 0;
}

static bool invalid_region_state_ok(const vm_user_region_t* region) {
    return region != NULL && region->address_space == NULL &&
           !region->registered && region->object == NULL;
}

static bool address_space_ready(const user_program_t* program) {
    return program != NULL && user_program_address_space((user_program_t*)program) != NULL;
}

static bool smoke_ready(const user_program_smoke_t* smoke) {
    return smoke != NULL && smoke->program != NULL &&
           user_program_address_space(smoke->program) != NULL &&
           user_program_process(smoke->program) != NULL;
}

static bool reject_invalid_region_paths(user_program_smoke_t* smoke);

static bool reject_invalid_kernel_mapping_paths(user_program_t* program,
                                                uintptr_t remap_page_paddr) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);

    address_space = user_program_address_space(program);
    return address_space != NULL &&
           !vm_address_space_map_kernel_range(address_space,
                                             alias_vaddr,
                                             remap_page_paddr,
                                             MEMORY_PAGE_SIZE,
                                             VM_PAGE_WRITE) &&
           !vm_address_space_map_kernel_range(
               address_space,
               memory_text_start(),
               memory_text_start(),
               memory_text_end() - memory_text_start(),
               VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER);
}

static bool map_standard_kernel_ranges(user_program_t* program) {
    vm_address_space_t* address_space = user_program_address_space(program);

    return address_space != NULL &&
           vm_address_space_map_identity_1g(address_space,
                                           0,
                                           VM_PAGE_READ | VM_PAGE_WRITE |
                                               VM_PAGE_EXEC) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_text_start(),
                                            memory_text_start(),
                                            memory_text_end() - memory_text_start(),
                                            VM_PAGE_READ | VM_PAGE_EXEC) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_rodata_start(),
                                            memory_rodata_start(),
                                            memory_rodata_end() -
                                                memory_rodata_start(),
                                            VM_PAGE_READ) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_data_start(),
                                            memory_data_start(),
                                            pmm_managed_start() -
                                                memory_data_start(),
                                            VM_PAGE_READ | VM_PAGE_WRITE) &&
           vm_address_space_map_kernel_range(address_space,
                                            pmm_managed_start(),
                                            pmm_managed_start(),
                                            pmm_managed_end() -
                                                pmm_managed_start(),
                                            VM_PAGE_READ | VM_PAGE_WRITE);
}

static bool reject_invalid_fault_range_paths(user_program_t* program,
                                             uintptr_t backing_page_paddr,
                                             uintptr_t remap_page_paddr) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);

    address_space = user_program_address_space(program);
    return address_space != NULL &&
           !vm_address_space_register_fault_range(address_space,
                                                 alias_vaddr + 1U,
                                                 backing_page_paddr,
                                                 MEMORY_PAGE_SIZE,
                                                 VM_PAGE_READ) &&
           !user_program_unmap_region_page(program,
                                           USER_PROGRAM_REGION_ALIAS,
                                           memory_text_start()) &&
           !vm_address_space_register_fault_range(address_space,
                                                 alias_vaddr,
                                                 remap_page_paddr,
                                                 MEMORY_PAGE_SIZE,
                                                 VM_PAGE_READ |
                                                     VM_PAGE_WRITE);
}

static bool prepare_fault_orchestration(user_program_smoke_t* smoke,
                                        uintptr_t remap_page_paddr,
                                        uintptr_t fault_skip_vaddr,
                                        size_t fault_skip_size,
                                        uintptr_t fault_resume_vaddr,
                                        size_t fault_resume_size,
                                        volatile uintptr_t* fault_resume_pc_slot) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uintptr_t alias_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    const uintptr_t remap_vaddr =
        alias_vaddr >= MEMORY_PAGE_SIZE ? alias_vaddr - MEMORY_PAGE_SIZE : 0;
    vm_address_space_t* address_space = user_program_address_space(smoke->program);

    return address_space != NULL &&
           vm_object_init_physical(&smoke->remap_object,
                                   remap_page_paddr,
                                   MEMORY_PAGE_SIZE) &&
           reject_invalid_region_paths(smoke) &&
           vm_address_space_register_fault_skip(address_space,
                                               RISCV_EXC_STORE_PAGE_FAULT,
                                               fault_skip_vaddr,
                                               fault_skip_size) &&
           vm_address_space_register_fault_resume_slot(address_space,
                                                       RISCV_EXC_INSN_PAGE_FAULT,
                                                       fault_resume_vaddr,
                                                       fault_resume_size,
                                                       fault_resume_pc_slot) &&
           user_program_map_object_region(smoke->program,
                                          &smoke->remap_region,
                                          remap_vaddr,
                                          MEMORY_PAGE_SIZE,
                                          user_rw_flags,
                                          &smoke->remap_object);
}

static bool reject_invalid_region_paths(user_program_smoke_t* smoke) {
    vm_process_t* process = NULL;
    vm_object_t* anon_object = NULL;
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uintptr_t alias_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    const uintptr_t anon_tail_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ANON_TAIL_VADDR);

    process = user_program_process(smoke->program);
    anon_object = user_program_object(smoke->program, USER_PROGRAM_OBJECT_ANON);
    if (process == NULL || anon_object == NULL || alias_vaddr < MEMORY_PAGE_SIZE ||
        anon_tail_vaddr < MEMORY_PAGE_SIZE) {
        return false;
    }

    if (user_program_set_region_fault_object(smoke->program,
                                            USER_PROGRAM_REGION_ALIAS,
                                            &smoke->remap_object) ||
        user_program_set_fault_object_region_at(smoke->program,
                                                &smoke->invalid_region,
                                                anon_tail_vaddr - MEMORY_PAGE_SIZE,
                                                MEMORY_PAGE_SIZE,
                                                user_rw_flags,
                                                anon_object,
                                                2U * MEMORY_PAGE_SIZE) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    0,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    memory_text_start(),
                                    memory_text_end() - memory_text_start(),
                                    VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    alias_vaddr,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_user_region_set_fault_object(&smoke->invalid_region,
                                        &smoke->remap_object) ||
        !invalid_region_state_ok(&smoke->invalid_region)) {
        return false;
    }

    return true;
}

void user_program_smoke_init(user_program_smoke_t* smoke) {
    if (smoke == NULL) {
        return;
    }

    smoke->program = NULL;
    clear_region(&smoke->remap_region);
    clear_region(&smoke->invalid_region);
    clear_object(&smoke->remap_object);
}

bool user_program_smoke_validate_vm_lifecycle(uintptr_t user_region_vaddr,
                                              size_t expected_free_pages) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    vm_address_space_t* lifecycle_space = NULL;
    vm_address_space_t* recycled_space = NULL;
    vm_process_t lifecycle_process = {0};
    vm_user_region_t lifecycle_region = {0};
    vm_object_t lifecycle_object = {0};

    return vm_range_is_user(user_region_vaddr, MEMORY_PAGE_SIZE) &&
           vm_address_space_create(&lifecycle_space) &&
           lifecycle_space != NULL &&
           vm_process_create(&lifecycle_process, lifecycle_space) &&
           vm_address_space_activate(lifecycle_space) &&
           vm_address_space_is_active(lifecycle_space) &&
           runtime_context_active_address_space() == lifecycle_space &&
           vm_address_space_disable(lifecycle_space) &&
           !vm_address_space_is_active(lifecycle_space) &&
           runtime_context_active_address_space() == NULL &&
           vm_address_space_map_identity_1g(lifecycle_space,
                                            0,
                                            VM_PAGE_READ | VM_PAGE_WRITE |
                                                VM_PAGE_EXEC) &&
           vm_process_user_region_init(&lifecycle_process,
                                       &lifecycle_region,
                                       user_region_vaddr,
                                       MEMORY_PAGE_SIZE,
                                       user_rw_flags) &&
           !vm_process_set_user_context(&lifecycle_process,
                                        user_region_vaddr,
                                        user_region_vaddr +
                                            MEMORY_PAGE_SIZE) &&
           vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           !vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_user_region_set_fault_object(&lifecycle_region,
                                            &lifecycle_object) &&
           !vm_object_reset(&lifecycle_object) &&
           vm_user_region_clear_object(&lifecycle_region) &&
           vm_user_region_set_fault_object(&lifecycle_region,
                                           &lifecycle_object) &&
           !vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_object_reset(&lifecycle_object) &&
           vm_user_region_clear_object(&lifecycle_region) &&
           !vm_address_space_destroy(lifecycle_space) &&
           vm_process_remove_user_region(&lifecycle_process, &lifecycle_region) &&
           !lifecycle_region.registered &&
           lifecycle_region.address_space == NULL &&
           lifecycle_region.object == NULL &&
           !vm_process_create(&lifecycle_process, lifecycle_space) &&
           vm_process_reset(&lifecycle_process) &&
           vm_object_reset(&lifecycle_object) &&
           lifecycle_process.address_space == NULL &&
           lifecycle_process.entry_pc == 0 &&
           lifecycle_process.user_sp == 0 &&
           lifecycle_process.user_regions[0] == NULL &&
           !lifecycle_object.initialized &&
           lifecycle_object.backing_kind == VM_OBJECT_BACKING_NONE &&
           lifecycle_object.attachment_count == 0 &&
           vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           vm_address_space_destroy(lifecycle_space) &&
           vm_address_space_create(&lifecycle_space) &&
           vm_address_space_create(&recycled_space) &&
           vm_address_space_destroy(lifecycle_space) &&
           vm_address_space_destroy(recycled_space) &&
           vm_object_reset(&lifecycle_object) &&
           pmm_free_pages() == expected_free_pages;
}

bool user_program_smoke_prepare_address_space(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t backing_page_paddr,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot) {
    const uintptr_t alias_vaddr =
        program != NULL
            ? user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR)
            : 0;

    if (smoke == NULL || smoke->program != NULL || !address_space_ready(program) ||
        alias_vaddr < MEMORY_PAGE_SIZE || backing_page_paddr == 0 ||
        remap_page_paddr == 0 || fault_skip_vaddr == 0 || fault_skip_size == 0 ||
        fault_resume_vaddr == 0 ||
        fault_resume_size == 0 || fault_resume_pc_slot == NULL) {
        return false;
    }

    smoke->program = program;
    return smoke_ready(smoke) &&
           reject_invalid_kernel_mapping_paths(program, remap_page_paddr) &&
           map_standard_kernel_ranges(program) &&
           reject_invalid_fault_range_paths(program,
                                            backing_page_paddr,
                                            remap_page_paddr) &&
           prepare_fault_orchestration(smoke,
                                       remap_page_paddr,
                                       fault_skip_vaddr,
                                       fault_skip_size,
                                       fault_resume_vaddr,
                                       fault_resume_size,
                                       fault_resume_pc_slot);
}

bool user_program_smoke_prepare_runtime(
    user_program_smoke_t* smoke,
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
    return smoke_ready(smoke) &&
           !user_program_is_active(smoke->program) &&
           user_program_smoke_unmap_remap_page(smoke) &&
           !user_program_is_runnable(smoke->program) &&
           user_program_prepare_standard(smoke->program,
                                         trap_context,
                                         arg0,
                                         trap_stack_base,
                                         trap_stack_size,
                                         validate,
                                         validate_context,
                                         supervisor_timer_post_handler,
                                         supervisor_timer_post_context,
                                         supervisor_external_post_handler,
                                         supervisor_external_post_context) &&
           user_program_is_runnable(smoke->program) &&
           !user_program_smoke_unmap_remap_page(smoke);
}

bool user_program_smoke_unmap_remap_page(user_program_smoke_t* smoke) {
    return smoke_ready(smoke) && smoke->remap_region.registered &&
           vm_user_region_unmap_page(&smoke->remap_region,
                                     smoke->remap_region.vaddr);
}

bool user_program_smoke_rebind_alias_fault_object(user_program_smoke_t* smoke) {
    return smoke_ready(smoke) &&
           user_program_rebind_region_fault_object(smoke->program,
                                                  USER_PROGRAM_REGION_ALIAS,
                                                  &smoke->remap_object);
}

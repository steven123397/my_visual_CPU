#include "user_program_smoke.h"

#include "memory.h"
#include "riscv.h"

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

static bool smoke_ready(const user_program_smoke_t* smoke) {
    return smoke != NULL && smoke->program != NULL &&
           user_program_address_space(smoke->program) != NULL &&
           user_program_process(smoke->program) != NULL;
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

bool user_program_smoke_prepare_fault_orchestration(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uintptr_t alias_vaddr =
        program != NULL
            ? user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR)
            : 0;
    const uintptr_t remap_vaddr =
        alias_vaddr >= MEMORY_PAGE_SIZE ? alias_vaddr - MEMORY_PAGE_SIZE : 0;
    vm_address_space_t* address_space = NULL;

    if (smoke == NULL || smoke->program != NULL || program == NULL ||
        alias_vaddr < MEMORY_PAGE_SIZE || remap_vaddr == 0 ||
        fault_skip_vaddr == 0 || fault_skip_size == 0 || fault_resume_vaddr == 0 ||
        fault_resume_size == 0 || fault_resume_pc_slot == NULL) {
        return false;
    }

    smoke->program = program;
    address_space = user_program_address_space(program);
    if (!smoke_ready(smoke) || address_space == NULL ||
        !vm_object_init_physical(&smoke->remap_object,
                                 remap_page_paddr,
                                 MEMORY_PAGE_SIZE) ||
        !reject_invalid_region_paths(smoke) ||
        !vm_address_space_register_fault_skip(address_space,
                                              RISCV_EXC_STORE_PAGE_FAULT,
                                              fault_skip_vaddr,
                                              fault_skip_size) ||
        !vm_address_space_register_fault_resume_slot(address_space,
                                                     RISCV_EXC_INSN_PAGE_FAULT,
                                                     fault_resume_vaddr,
                                                     fault_resume_size,
                                                     fault_resume_pc_slot) ||
        !user_program_map_object_region(program,
                                        &smoke->remap_region,
                                        remap_vaddr,
                                        MEMORY_PAGE_SIZE,
                                        user_rw_flags,
                                        &smoke->remap_object)) {
        return false;
    }

    return true;
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

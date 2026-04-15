#include "user_program_smoke.h"

#include "memory.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "timer.h"

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

static bool region_cleared(const vm_user_region_t* region) {
    return region != NULL && region->address_space == NULL && region->vaddr == 0 &&
           region->size == 0 && region->flags == 0 && !region->registered &&
           region->object == NULL && region->object_offset == 0 &&
           region->object_mode == VM_REGION_OBJECT_NONE;
}

static bool object_cleared(const vm_object_t* object) {
    return object != NULL && !object->initialized &&
           object->backing_kind == VM_OBJECT_BACKING_NONE &&
           object->size == 0 && object->attachment_count == 0 &&
           object->backing.anon.page_slots == NULL &&
           object->backing.anon.page_count == 0;
}

static bool invalid_region_state_ok(const vm_user_region_t* region) {
    return region != NULL && region->address_space == NULL &&
           !region->registered && region->object == NULL;
}

static bool runtime_policies_bound(const trap_context_t* trap_context,
                                   const trap_user_runtime_t* user_runtime) {
    return trap_context != NULL && user_runtime != NULL &&
           trap_context->supervisor_timer_policy.user_runtime == user_runtime &&
           trap_context->supervisor_external_policy.user_runtime ==
               user_runtime &&
           trap_context->user_ecall_policy.user_runtime == user_runtime;
}

static bool runtime_policies_cleared(const trap_context_t* trap_context) {
    return trap_context != NULL &&
           trap_context->supervisor_timer_policy.user_runtime == NULL &&
           trap_context->supervisor_external_policy.user_runtime == NULL &&
           trap_context->user_ecall_policy.user_runtime == NULL;
}

static bool address_space_ready(const user_program_t* program) {
    return program != NULL &&
           user_program_address_space((user_program_t*)program) != NULL;
}

static bool smoke_ready(const user_program_smoke_t* smoke) {
    return smoke != NULL && smoke->program != NULL &&
           user_program_address_space(smoke->program) != NULL &&
           user_program_process(smoke->program) != NULL;
}

typedef struct UserProgramStandardPlanValues {
    uintptr_t exec_page_paddr;
    uintptr_t exec_vaddr;
    uintptr_t stack_vaddr;
    uintptr_t alias_vaddr;
    uintptr_t anon_tail_vaddr;
    uintptr_t entry_pc;
    uintptr_t expected_ecall_pc;
    uintptr_t user_sp;
} user_program_standard_plan_values_t;

typedef struct UserProgramSmokePrepareStage {
    user_program_smoke_t* smoke;
    user_program_t* program;
    const user_program_smoke_prepare_t* prepare;
} user_program_smoke_prepare_stage_t;

typedef struct UserProgramSmokeRoundStage {
    user_program_smoke_t* smoke;
    const user_program_smoke_round_t* round;
} user_program_smoke_round_stage_t;

typedef struct UserProgramSmokeActiveMemoryStage {
    user_program_smoke_t* smoke;
    uint32_t* backing_page;
    uint32_t* remap_page;
    const user_program_smoke_active_phase_t* phase;
    uintptr_t alias_vaddr;
    uintptr_t anon_vaddr;
    uintptr_t anon_tail_vaddr;
    uint32_t* alias_page;
    uint32_t* anon_page;
    uint32_t* anon_tail_page;
    uint32_t backing_word0;
} user_program_smoke_active_memory_stage_t;

static bool load_standard_plan_values(
    const user_program_t* program,
    user_program_standard_plan_values_t* values);
static bool standard_plan_pages_aligned(
    const user_program_standard_plan_values_t* values);
static bool standard_plan_layout_within_user_range(
    const user_program_standard_plan_values_t* values,
    uintptr_t user_base,
    uintptr_t user_limit);
static bool standard_plan_exec_range_valid(
    const user_program_standard_plan_values_t* values);
#include "user_program_smoke_prepare.inc"
#include "user_program_smoke_active_memory.inc"
#include "user_program_smoke_lifecycle.inc"
#include "user_program_smoke_prepare_runtime.inc"
#include "user_program_smoke_round.inc"

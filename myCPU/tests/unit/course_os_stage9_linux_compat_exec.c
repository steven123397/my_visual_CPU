#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_shell.h"
#include "../../guest/include/linux_compat_exec.h"
#include "../../guest/kernel/vm_private.h"

typedef struct PageMapCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    uint64_t flags;
} page_map_call_t;

static uint64_t g_pages[48][SV39_LEVEL_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static size_t g_next_page = 0;
static int g_alloc_zeroed_page_calls = 0;
static int g_pmm_free_page_calls = 0;
static void* g_freed_pages[48];
static size_t g_freed_page_count = 0;
static int g_map_page_calls = 0;
static page_map_call_t g_map_page_records[48];
static int g_unmap_page_calls = 0;
static uintptr_t g_unmap_page_vaddrs[48];
static int g_unregister_user_region_calls = 0;
static int g_prepare_standard_calls = 0;
static uintptr_t g_last_prepare_entry_pc = 0;
static uintptr_t g_last_prepare_user_sp = 0;
static int g_linux_policy_install_calls = 0;
static linux_compat_runtime_t* g_last_linux_policy_runtime = NULL;
static int g_runtime_activate_calls = 0;
static int g_runtime_enter_calls = 0;
static int g_runtime_deactivate_calls = 0;

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void reset_stub_state(void) {
    memset(g_pages, 0, sizeof(g_pages));
    g_next_page = 0;
    g_alloc_zeroed_page_calls = 0;
    g_pmm_free_page_calls = 0;
    memset(g_freed_pages, 0, sizeof(g_freed_pages));
    g_freed_page_count = 0;
    g_map_page_calls = 0;
    memset(g_map_page_records, 0, sizeof(g_map_page_records));
    g_unmap_page_calls = 0;
    memset(g_unmap_page_vaddrs, 0, sizeof(g_unmap_page_vaddrs));
    g_unregister_user_region_calls = 0;
    g_prepare_standard_calls = 0;
    g_last_prepare_entry_pc = 0;
    g_last_prepare_user_sp = 0;
    g_linux_policy_install_calls = 0;
    g_last_linux_policy_runtime = NULL;
    g_runtime_activate_calls = 0;
    g_runtime_enter_calls = 0;
    g_runtime_deactivate_calls = 0;
}

void* alloc_zeroed_page(void) {
    uint64_t* page = NULL;

    g_alloc_zeroed_page_calls += 1;
    if (g_next_page >= (sizeof(g_pages) / sizeof(g_pages[0]))) {
        return NULL;
    }
    page = g_pages[g_next_page++];
    memset(page, 0, MEMORY_PAGE_SIZE);
    return page;
}

bool pmm_free_page(void* page) {
    if (g_freed_page_count < (sizeof(g_freed_pages) / sizeof(g_freed_pages[0]))) {
        g_freed_pages[g_freed_page_count++] = page;
    }
    g_pmm_free_page_calls += 1;
    return page != NULL;
}

bool can_map_page(vm_address_space_t* address_space, uintptr_t vaddr) {
    int i = 0;

    (void)address_space;
    for (i = 0; i < g_map_page_calls; ++i) {
        if (g_map_page_records[i].vaddr == vaddr) {
            return false;
        }
    }
    return true;
}

bool map_page_internal(vm_address_space_t* address_space,
                       uintptr_t vaddr,
                       uintptr_t paddr,
                       uint64_t flags) {
    if (address_space == NULL ||
        g_map_page_calls >= (int)(sizeof(g_map_page_records) /
                                  sizeof(g_map_page_records[0]))) {
        return false;
    }
    g_map_page_records[g_map_page_calls++] = (page_map_call_t){
        .vaddr = vaddr,
        .paddr = paddr,
        .flags = flags,
    };
    return true;
}

bool unmap_page_internal(vm_address_space_t* address_space, uintptr_t vaddr) {
    int i = 0;

    if (address_space == NULL ||
        g_unmap_page_calls >= (int)(sizeof(g_unmap_page_vaddrs) /
                                    sizeof(g_unmap_page_vaddrs[0]))) {
        return false;
    }
    g_unmap_page_vaddrs[g_unmap_page_calls++] = vaddr;
    for (i = 0; i < g_map_page_calls; ++i) {
        if (g_map_page_records[i].vaddr == vaddr) {
            g_map_page_records[i].vaddr = UINTPTR_MAX;
            break;
        }
    }
    return true;
}

void flush_tlb_if_enabled(void) {}

uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space) {
    if (address_space == NULL || !address_space->allocated) {
        return 0;
    }
    return address_space->root_table_pa;
}

bool vm_address_space_enable(vm_address_space_t* address_space) {
    return address_space != NULL;
}

bool vm_address_space_user_region_init(vm_address_space_t* address_space,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags) {
    if (address_space == NULL || region == NULL ||
        !page_span_args_valid(vaddr, size) ||
        !vm_range_is_user(vaddr, size) ||
        !user_flags_valid(flags)) {
        return false;
    }
    region->address_space = address_space;
    region->vaddr = vaddr;
    region->size = size;
    region->flags = flags;
    region->registered = true;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    return true;
}

bool vm_address_space_unregister_user_region_internal(
    vm_address_space_t* address_space,
    vm_user_region_t* region) {
    g_unregister_user_region_calls += 1;
    return address_space != NULL && region != NULL &&
           region->address_space == address_space;
}

void runtime_context_activate_process(vm_process_t* process) {
    (void)process;
}

void runtime_context_clear_process(const vm_process_t* process) {
    (void)process;
}

bool runtime_context_process_is_active(const vm_process_t* process) {
    (void)process;
    return false;
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, VM_USER_VADDR_LIMIT);
}

uintptr_t vm_user_base(void) {
    return VM_USER_VADDR_BASE;
}

uintptr_t vm_user_limit(void) {
    return VM_USER_VADDR_LIMIT;
}

bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size) {
    return region != NULL && region->registered &&
           range_within_window(vaddr,
                               size,
                               region->vaddr,
                               region->vaddr + (uintptr_t)region->size);
}

bool trap_user_runtime_prepare_standard(
    trap_user_runtime_t* user_runtime,
    trap_context_t* trap_context,
    vm_process_t* process,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context) {
    (void)arg0;
    (void)trap_stack_base;
    (void)trap_stack_size;
    (void)expected_ecall_pc;
    (void)validate;
    (void)validate_context;
    (void)supervisor_timer_post_handler;
    (void)supervisor_timer_post_context;
    (void)supervisor_external_post_handler;
    (void)supervisor_external_post_context;
    g_prepare_standard_calls += 1;
    g_last_prepare_entry_pc = entry_pc;
    g_last_prepare_user_sp = user_sp;
    if (user_runtime != NULL) {
        user_runtime->trap_context = trap_context;
        user_runtime->process = process;
        user_runtime->resume_pc = 0xfeed0000U;
    }
    if (process != NULL) {
        process->entry_pc = entry_pc;
        process->user_sp = user_sp;
    }
    return user_runtime != NULL && trap_context != NULL && process != NULL;
}

bool trap_context_install_linux_compat_syscall_policy(
    trap_context_t* trap_context,
    trap_user_runtime_t* user_runtime,
    linux_compat_runtime_t* runtime) {
    (void)trap_context;
    (void)user_runtime;
    g_linux_policy_install_calls += 1;
    g_last_linux_policy_runtime = runtime;
    return runtime != NULL;
}

bool trap_user_runtime_activate(trap_user_runtime_t* user_runtime) {
    g_runtime_activate_calls += 1;
    return user_runtime != NULL;
}

bool trap_user_runtime_enter(const trap_user_runtime_t* user_runtime) {
    (void)user_runtime;
    g_runtime_enter_calls += 1;
    if (g_last_linux_policy_runtime != NULL) {
        g_last_linux_policy_runtime->exited = true;
        g_last_linux_policy_runtime->exit_code = 0;
    }
    return true;
}

bool trap_user_runtime_deactivate(trap_user_runtime_t* user_runtime) {
    g_runtime_deactivate_calls += 1;
    return user_runtime != NULL;
}

static bool make_process(vm_address_space_t* address_space,
                         vm_process_t* process,
                         linux_compat_vm_t* vm) {
    memset(address_space, 0, sizeof(*address_space));
    memset(process, 0, sizeof(*process));
    address_space->allocated = true;
    address_space->root_table = g_pages[47];
    address_space->root_table_pa = (uintptr_t)g_pages[47];
    if (!vm_process_create(process, address_space)) {
        return false;
    }
    linux_compat_vm_init(vm, address_space, process);
    return true;
}

static void write_u16_le(uint8_t* image, size_t offset, uint16_t value) {
    image[offset] = (uint8_t)(value & 0xffU);
    image[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
}

static void write_u32_le(uint8_t* image, size_t offset, uint32_t value) {
    image[offset] = (uint8_t)(value & 0xffU);
    image[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    image[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    image[offset + 3U] = (uint8_t)((value >> 24U) & 0xffU);
}

static void write_u64_le(uint8_t* image, size_t offset, uint64_t value) {
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        image[offset + i] = (uint8_t)((value >> (i * 8U)) & 0xffU);
    }
}

static uint64_t read_u64_le(const uint8_t* image, size_t offset) {
    uint64_t value = 0;
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        value |= (uint64_t)image[offset + i] << (i * 8U);
    }
    return value;
}

static void make_elf_header(uint8_t* image,
                            size_t size,
                            uint16_t type,
                            uint64_t entry,
                            uint16_t phnum) {
    memset(image, 0, size);
    image[0] = 0x7fU;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 2U;
    image[5] = 1U;
    image[6] = 1U;
    write_u16_le(image, 16U, type);
    write_u16_le(image, 18U, 243U);
    write_u32_le(image, 20U, 1U);
    write_u64_le(image, 24U, entry);
    write_u64_le(image, 32U, 64U);
    write_u16_le(image, 52U, 64U);
    write_u16_le(image, 54U, 56U);
    write_u16_le(image, 56U, phnum);
}

static void write_program_header(uint8_t* image,
                                 size_t index,
                                 uint32_t type,
                                 uint32_t flags,
                                 uint64_t offset,
                                 uint64_t vaddr,
                                 uint64_t filesz,
                                 uint64_t memsz) {
    const size_t base = 64U + (index * 56U);

    write_u32_le(image, base + 0U, type);
    write_u32_le(image, base + 4U, flags);
    write_u64_le(image, base + 8U, offset);
    write_u64_le(image, base + 16U, vaddr);
    write_u64_le(image, base + 24U, vaddr);
    write_u64_le(image, base + 32U, filesz);
    write_u64_le(image, base + 40U, memsz);
    write_u64_le(image, base + 48U, 0x1000U);
}

static linux_compat_vm_region_t* find_region(linux_compat_vm_t* vm,
                                             uintptr_t vaddr) {
    size_t i = 0;

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (vm->regions[i].used && vm->regions[i].vaddr == vaddr) {
            return &vm->regions[i];
        }
    }
    return NULL;
}

static linux_compat_vm_region_t* find_region_containing(linux_compat_vm_t* vm,
                                                        uintptr_t vaddr) {
    size_t i = 0;

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (vm->regions[i].used &&
            vaddr >= vm->regions[i].vaddr &&
            vaddr < vm->regions[i].vaddr + vm->regions[i].length) {
            return &vm->regions[i];
        }
    }
    return NULL;
}

static int test_exec_load_maps_segment_and_copies_file_bytes(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uint8_t image[512];
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;
    uintptr_t entry_pc = 0;
    linux_compat_vm_region_t* region = NULL;
    const uint8_t text[] = {0x13U, 0x00U, 0x00U, 0x00U};
    uint8_t* mapped_page = NULL;

    reset_stub_state();
    if (!make_process(&address_space, &process, &vm)) {
        return fail("expected process setup to succeed");
    }
    make_elf_header(image, sizeof(image), 2U, 0x400000U, 1U);
    write_program_header(image, 0U, 1U, 5U, 0x100U, 0x400000U,
                         sizeof(text), MEMORY_PAGE_SIZE);
    memcpy(image + 0x100U, text, sizeof(text));

    if (linux_compat_build_load_plan(image, sizeof(image), 1U, 0U, &plan, &trace) !=
        LINUX_COMPAT_OK) {
        return fail("expected load plan to build");
    }
    if (linux_compat_exec_load(&vm, image, sizeof(image), &plan, &entry_pc, &trace) !=
            LINUX_COMPAT_OK ||
        entry_pc != 0x400000U) {
        return fail("expected exec_load to accept static ET_EXEC segment");
    }
    region = find_region(&vm, 0x400000U);
    if (region == NULL || g_map_page_calls != 1 ||
        (g_map_page_records[0].flags & (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) !=
            (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) {
        return fail("expected exec_load to map PT_LOAD with executable user flags");
    }
    mapped_page = (uint8_t*)region->object.backing.anon.page_slots[0];
    if (mapped_page == NULL || memcmp(mapped_page, text, sizeof(text)) != 0 ||
        mapped_page[sizeof(text)] != 0U) {
        return fail("expected exec_load to copy file bytes and leave bss zeroed");
    }
    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_exec_build_stack_writes_argc_argv_and_auxv(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uint8_t image[512];
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;
    const char* argv[] = {"/bin/busybox", "echo", "hello"};
    uintptr_t user_sp = 0;
    linux_compat_vm_region_t* stack = NULL;
    uint8_t* stack_page = NULL;
    size_t sp_offset = 0;
    size_t sp_page_index = 0;
    size_t sp_page_offset = 0;
    uint64_t argc = 0;
    uint64_t argv0 = 0;
    size_t aux_offset = 0;
    bool saw_entry = false;

    reset_stub_state();
    if (!make_process(&address_space, &process, &vm)) {
        return fail("expected process setup to succeed");
    }
    make_elf_header(image, sizeof(image), 2U, 0x401000U, 1U);
    write_program_header(image, 0U, 1U, 5U, 0x100U, 0x401000U, 4U, 4U);
    if (linux_compat_build_load_plan(image, sizeof(image), 3U, 0U, &plan, &trace) !=
        LINUX_COMPAT_OK) {
        return fail("expected load plan to build");
    }

    if (linux_compat_exec_build_stack(&vm, &plan, 3U, argv, &user_sp, &trace) !=
            LINUX_COMPAT_OK ||
        user_sp == 0U || (user_sp & 15U) != 0U) {
        return fail("expected stack builder to return aligned user sp");
    }
    stack = find_region_containing(&vm, user_sp);
    if (stack == NULL) {
        return fail("expected stack builder to map a stack region");
    }
    if (stack->length < 2U * MEMORY_PAGE_SIZE) {
        return fail("expected stack builder to reserve more than one stack page");
    }
    sp_offset = (size_t)(user_sp - stack->vaddr);
    sp_page_index = sp_offset / MEMORY_PAGE_SIZE;
    sp_page_offset = sp_offset % MEMORY_PAGE_SIZE;
    stack_page = (uint8_t*)stack->object.backing.anon.page_slots[sp_page_index];
    argc = read_u64_le(stack_page, sp_page_offset);
    argv0 = read_u64_le(stack_page, sp_page_offset + 8U);
    if (argc != 3U ||
        strcmp((const char*)stack->object.backing.anon.page_slots
                   [(argv0 - stack->vaddr) / MEMORY_PAGE_SIZE] +
                   ((argv0 - stack->vaddr) % MEMORY_PAGE_SIZE),
               argv[0]) != 0) {
        return fail("expected stack argc and argv[0] to be readable");
    }
    aux_offset = sp_offset + (1U + 3U + 1U + 1U) * sizeof(uint64_t);
    while (aux_offset + 16U <= stack->length) {
        const size_t aux_page_index = aux_offset / MEMORY_PAGE_SIZE;
        const size_t aux_page_offset = aux_offset % MEMORY_PAGE_SIZE;
        uint8_t* aux_page =
            (uint8_t*)stack->object.backing.anon.page_slots[aux_page_index];
        const uint64_t type = read_u64_le(aux_page, aux_page_offset);
        const uint64_t value = read_u64_le(aux_page, aux_page_offset + 8U);

        if (type == 0U) {
            break;
        }
        if (type == 9U && value == plan.entry) {
            saw_entry = true;
            break;
        }
        aux_offset += 16U;
    }
    if (!saw_entry) {
        return fail("expected auxv to include AT_ENTRY");
    }
    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_exec_build_stack_for_dynamic_plan_includes_at_base(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uint8_t image[512];
    const char interp[] = "/lib/ld-musl-riscv64.so.1";
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;
    const char* argv[] = {"/usr/bin/dynamic-app"};
    uintptr_t user_sp = 0;
    linux_compat_vm_region_t* stack = NULL;
    size_t aux_offset = 0;
    bool saw_base = false;

    reset_stub_state();
    if (!make_process(&address_space, &process, &vm)) {
        return fail("expected process setup to succeed");
    }
    make_elf_header(image, sizeof(image), 3U, 0x1000U, 2U);
    write_program_header(image, 0U, 1U, 5U, 0x100U, 0U, 4U, 4U);
    write_program_header(image, 1U, 3U, 4U, 0x180U, 0U,
                         sizeof(interp), sizeof(interp));
    memcpy(image + 0x180U, interp, sizeof(interp));
    if (linux_compat_build_load_plan(image, sizeof(image), 1U, 0U, &plan, &trace) !=
        LINUX_COMPAT_OK) {
        return fail("expected dyn load plan to build");
    }
    plan.interp_load_bias = LINUX_COMPAT_INTERP_LOAD_BIAS;
    plan.interp_entry = LINUX_COMPAT_INTERP_LOAD_BIAS + 0x1200U;

    if (linux_compat_exec_build_stack(&vm, &plan, 1U, argv, &user_sp, &trace) !=
        LINUX_COMPAT_OK) {
        return fail("expected dynamic stack builder to accept interp metadata");
    }
    stack = find_region_containing(&vm, user_sp);
    if (stack == NULL) {
        return fail("expected dynamic stack builder to map stack");
    }
    aux_offset = (size_t)(user_sp - stack->vaddr) +
                 (1U + 1U + 1U + 1U) * sizeof(uint64_t);
    while (aux_offset + 16U <= stack->length) {
        const size_t aux_page_index = aux_offset / MEMORY_PAGE_SIZE;
        const size_t aux_page_offset = aux_offset % MEMORY_PAGE_SIZE;
        uint8_t* aux_page =
            (uint8_t*)stack->object.backing.anon.page_slots[aux_page_index];
        const uint64_t type = read_u64_le(aux_page, aux_page_offset);
        const uint64_t value = read_u64_le(aux_page, aux_page_offset + 8U);

        if (type == 0U) {
            break;
        }
        if (type == 7U && value == LINUX_COMPAT_INTERP_LOAD_BIAS) {
            saw_base = true;
            break;
        }
        aux_offset += 16U;
    }
    if (!saw_base) {
        return fail("expected dynamic stack auxv to include AT_BASE");
    }
    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_exec_load_maps_interp_main_plan(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uint8_t image[512];
    const char interp[] = "/lib/ld-musl-riscv64.so.1";
    linux_compat_load_plan_t plan;
    linux_compat_trace_t trace;
    uintptr_t entry_pc = 0;

    reset_stub_state();
    if (!make_process(&address_space, &process, &vm)) {
        return fail("expected process setup to succeed");
    }
    make_elf_header(image, sizeof(image), 3U, 0x1000U, 2U);
    write_program_header(image, 0U, 1U, 5U, 0x100U, 0U, 4U, 4U);
    write_program_header(image, 1U, 3U, 4U, 0x180U, 0U,
                         sizeof(interp), sizeof(interp));
    memcpy(image + 0x180U, interp, sizeof(interp));
    if (linux_compat_build_load_plan(image, sizeof(image), 1U, 0U, &plan, &trace) !=
        LINUX_COMPAT_OK) {
        return fail("expected dyn load plan to build");
    }
    if (linux_compat_exec_load(&vm, image, sizeof(image), &plan, &entry_pc, &trace) !=
            LINUX_COMPAT_OK ||
        entry_pc != plan.entry ||
        find_region(&vm, LINUX_COMPAT_DYN_LOAD_BIAS) == NULL) {
        return fail("expected exec_load to map dynamic main ELF and preserve entry");
    }
    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_exec_enter_installs_linux_policy_and_returns_after_exit(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    trap_context_t trap_context = {0};
    trap_user_runtime_t user_runtime = {0};
    linux_compat_runtime_t runtime = {0};
    linux_compat_trace_t trace;
    uint8_t trap_stack[512] __attribute__((aligned(16)));

    reset_stub_state();
    if (!make_process(&address_space, &process, &vm)) {
        return fail("expected process setup to succeed");
    }
    if (linux_compat_exec_enter(&vm,
                                &trap_context,
                                &user_runtime,
                                trap_stack,
                                sizeof(trap_stack),
                                0x401000U,
                                LINUX_COMPAT_STACK_TOP - 16U,
                                &runtime,
                                &trace) != LINUX_COMPAT_OK) {
        return fail("expected exec_enter to run until linux runtime exit");
    }
    if (g_prepare_standard_calls != 1 ||
        g_last_prepare_entry_pc != 0x401000U ||
        g_last_prepare_user_sp != LINUX_COMPAT_STACK_TOP - 16U ||
        g_linux_policy_install_calls != 1 ||
        g_last_linux_policy_runtime != &runtime ||
        g_runtime_activate_calls != 1 ||
        g_runtime_enter_calls != 1 ||
        g_runtime_deactivate_calls != 1 ||
        !runtime.exited) {
        return fail("expected exec_enter to prepare, install policy, enter and deactivate");
    }
    return 0;
}

static int test_course_shell_linux_compat_trap_stack_is_aligned(void) {
    static course_shell_t shell;
    const uintptr_t trap_stack_base =
        (uintptr_t)shell.linux_compat_trap_stack;

    if ((trap_stack_base & (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) != 0U) {
        return fail("expected course shell Linux compat trap stack to stay 16-byte aligned");
    }
    if (sizeof(shell.linux_compat_trap_stack) < 4096U) {
        return fail("expected course shell Linux compat trap stack to cover real-exec syscall dispatch");
    }
    return 0;
}

int main(void) {
    if (test_exec_load_maps_segment_and_copies_file_bytes() != 0 ||
        test_exec_build_stack_writes_argc_argv_and_auxv() != 0 ||
        test_exec_build_stack_for_dynamic_plan_includes_at_base() != 0 ||
        test_exec_load_maps_interp_main_plan() != 0 ||
        test_exec_enter_installs_linux_policy_and_returns_after_exit() != 0 ||
        test_course_shell_linux_compat_trap_stack_is_aligned() != 0) {
        return 1;
    }
    return 0;
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat.h"
#include "../../guest/include/linux_compat_vm.h"
#include "../../guest/kernel/vm_private.h"

typedef struct PageMapCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    uint64_t flags;
} page_map_call_t;

static uint64_t g_pages[32][SV39_LEVEL_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static size_t g_next_page = 0;
static int g_alloc_zeroed_page_calls = 0;
static int g_pmm_free_page_calls = 0;
static void* g_freed_pages[32];
static size_t g_freed_page_count = 0;
static int g_can_map_page_calls = 0;
static int g_map_page_calls = 0;
static page_map_call_t g_map_page_records[32];
static int g_unmap_page_calls = 0;
static uintptr_t g_unmap_page_vaddrs[32];
static int g_flush_if_enabled_calls = 0;
static int g_unregister_user_region_calls = 0;

static void reset_stub_state(void) {
    memset(g_pages, 0, sizeof(g_pages));
    g_next_page = 0;
    g_alloc_zeroed_page_calls = 0;
    g_pmm_free_page_calls = 0;
    memset(g_freed_pages, 0, sizeof(g_freed_pages));
    g_freed_page_count = 0;
    g_can_map_page_calls = 0;
    g_map_page_calls = 0;
    memset(g_map_page_records, 0, sizeof(g_map_page_records));
    g_unmap_page_calls = 0;
    memset(g_unmap_page_vaddrs, 0, sizeof(g_unmap_page_vaddrs));
    g_flush_if_enabled_calls = 0;
    g_unregister_user_region_calls = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
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
    g_can_map_page_calls += 1;
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

void flush_tlb_if_enabled(void) {
    g_flush_if_enabled_calls += 1;
}

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

void runtime_context_clear_process(const vm_process_t* process) {
    (void)process;
}

void runtime_context_activate_process(vm_process_t* process) {
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

static bool make_process(vm_address_space_t* address_space,
                         vm_process_t* process) {
    memset(address_space, 0, sizeof(*address_space));
    memset(process, 0, sizeof(*process));
    address_space->allocated = true;
    address_space->root_table = g_pages[31];
    address_space->root_table_pa = (uintptr_t)g_pages[31];
    return vm_process_create(process, address_space);
}

static int test_brk_maps_heap_pages_and_shrink_releases_backing(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    const uintptr_t first_break = LINUX_COMPAT_BRK_BASE + 2U * MEMORY_PAGE_SIZE;
    const uintptr_t shrunk_break = LINUX_COMPAT_BRK_BASE + MEMORY_PAGE_SIZE;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    if (linux_compat_vm_brk(&vm, 0) != LINUX_COMPAT_BRK_BASE) {
        return fail("expected brk(0) to report initial linux compat break");
    }
    if (linux_compat_vm_brk(&vm, first_break) != first_break) {
        return fail("expected brk extension to return requested break");
    }
    if (g_map_page_calls != 2 ||
        g_map_page_records[0].vaddr != LINUX_COMPAT_BRK_BASE ||
        g_map_page_records[1].vaddr != LINUX_COMPAT_BRK_BASE + MEMORY_PAGE_SIZE ||
        (g_map_page_records[0].flags & (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER)) !=
            (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER)) {
        return fail("expected brk extension to map real writable user pages");
    }

    if (linux_compat_vm_brk(&vm, shrunk_break) != shrunk_break) {
        return fail("expected brk shrink to return requested break");
    }
    if (g_unmap_page_calls < 2 || g_pmm_free_page_calls < 3 ||
        g_unregister_user_region_calls == 0) {
        return fail("expected brk shrink to unmap and release prior heap backing");
    }

    linux_compat_vm_destroy(&vm);
    if (g_pmm_free_page_calls < 5) {
        return fail("expected destroy to release remaining heap backing");
    }

    return 0;
}

static int test_mmap_maps_user_pages_and_munmap_releases_them(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uintptr_t addr = 0;
    const size_t before_map_calls = 0;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    addr = linux_compat_vm_mmap(&vm,
                                0,
                                2U * MEMORY_PAGE_SIZE,
                                LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_EXEC,
                                0);
    if (addr != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected mmap without hint to use deterministic base");
    }
    if (g_map_page_calls != (int)(before_map_calls + 2U) ||
        g_map_page_records[0].vaddr != LINUX_COMPAT_MMAP_BASE ||
        g_map_page_records[1].vaddr != LINUX_COMPAT_MMAP_BASE + MEMORY_PAGE_SIZE ||
        (g_map_page_records[0].flags &
         (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) !=
            (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) {
        return fail("expected mmap to create executable user page mappings");
    }

    if (linux_compat_vm_munmap(&vm, addr, 2U * MEMORY_PAGE_SIZE) != 0) {
        return fail("expected munmap of exact mapping to succeed");
    }
    if (g_unmap_page_calls != 2 || g_pmm_free_page_calls < 3 ||
        g_unregister_user_region_calls != 1) {
        return fail("expected munmap to unmap region and free backing object");
    }

    if (linux_compat_vm_munmap(&vm, addr, MEMORY_PAGE_SIZE) != -22) {
        return fail("expected second munmap of released region to fail closed");
    }

    return 0;
}

static int test_syscall_dispatch_uses_bound_linux_compat_vm(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uintptr_t mmap_addr = 0;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);
    runtime.vm = &vm;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_BRK;
    request.addr = LINUX_COMPAT_BRK_BASE + MEMORY_PAGE_SIZE;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)request.addr ||
        g_map_page_calls != 1) {
        return fail("expected brk syscall to map heap through bound VM");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_EXEC;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_MMAP_BASE ||
        g_map_page_calls != 2 ||
        (g_map_page_records[1].flags & (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) !=
            (VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER)) {
        return fail("expected mmap syscall to map through bound VM");
    }
    mmap_addr = (uintptr_t)response.value;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MUNMAP;
    request.addr = mmap_addr;
    request.length = MEMORY_PAGE_SIZE;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0 ||
        g_unmap_page_calls == 0) {
        return fail("expected munmap syscall to release mapping through bound VM");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

int main(void) {
    if (test_brk_maps_heap_pages_and_shrink_releases_backing() != 0 ||
        test_mmap_maps_user_pages_and_munmap_releases_them() != 0 ||
        test_syscall_dispatch_uses_bound_linux_compat_vm() != 0) {
        return 1;
    }

    return 0;
}

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/linux_compat.h"
#include "../../guest/include/linux_compat_loader.h"
#include "../../guest/include/linux_compat_rootfs.h"
#include "../../guest/include/linux_compat_vm.h"
#include "../../guest/kernel/vm_private.h"
#include "../../include/platform_mmio.h"

typedef struct PageMapCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    uint64_t flags;
} page_map_call_t;

static uint64_t g_pages[4096][SV39_LEVEL_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static size_t g_next_page = 0;
static size_t g_alloc_zeroed_page_limit = 0;
static int g_alloc_zeroed_page_calls = 0;
static int g_pmm_free_page_calls = 0;
static void* g_freed_pages[4096];
static size_t g_freed_page_count = 0;
static int g_can_map_page_calls = 0;
static int g_map_page_calls = 0;
static page_map_call_t g_map_page_records[4096];
static int g_unmap_page_calls = 0;
static uintptr_t g_unmap_page_vaddrs[4096];
static int g_flush_if_enabled_calls = 0;
static int g_unregister_user_region_calls = 0;

static void reset_stub_state(void) {
    memset(g_pages, 0, sizeof(g_pages));
    g_next_page = 0;
    g_alloc_zeroed_page_limit = sizeof(g_pages) / sizeof(g_pages[0]);
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
    if (g_next_page >= (sizeof(g_pages) / sizeof(g_pages[0])) ||
        g_next_page >= g_alloc_zeroed_page_limit) {
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
    size_t i = 0;
    vm_user_region_t** slot = NULL;

    if (address_space == NULL || region == NULL ||
        address_space->root_table == NULL ||
        !page_span_args_valid(vaddr, size) ||
        !vm_range_is_user(vaddr, size) ||
        !user_flags_valid(flags)) {
        return false;
    }
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        vm_user_region_t* used = address_space->user_regions[i];

        if (used == NULL) {
            if (slot == NULL) {
                slot = &address_space->user_regions[i];
            }
            continue;
        }
        if (used == region ||
            ranges_overlap(vaddr, size, used->vaddr, used->size)) {
            return false;
        }
    }
    if (slot == NULL) {
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
    *slot = region;
    return true;
}

bool vm_address_space_unregister_user_region_internal(
    vm_address_space_t* address_space,
    vm_user_region_t* region) {
    size_t i = 0;

    g_unregister_user_region_calls += 1;
    if (address_space == NULL || region == NULL ||
        region->address_space != address_space) {
        return false;
    }
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] == region) {
            address_space->user_regions[i] = NULL;
            return true;
        }
    }
    return false;
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

static linux_compat_vm_region_t* find_vm_region(linux_compat_vm_t* vm,
                                                uintptr_t vaddr) {
    size_t i = 0;

    if (vm == NULL) {
        return NULL;
    }
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (vm->regions[i].used && vm->regions[i].vaddr == vaddr) {
            return &vm->regions[i];
        }
    }
    return NULL;
}

static uintptr_t align_up_test_page(uintptr_t value) {
    const uintptr_t mask = (uintptr_t)MEMORY_PAGE_SIZE - 1U;

    return (value + mask) & ~mask;
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
    linux_compat_vm_region_t* region = NULL;
    uintptr_t addr = 0;

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
    region = find_vm_region(&vm, addr);
    if (region == NULL ||
        region->region.object_mode != VM_REGION_OBJECT_FAULT ||
        region->object.backing_kind != VM_OBJECT_BACKING_ANON ||
        region->object.backing.anon.page_slots == 0 ||
        region->object.backing.anon.page_slots[0] != 0U ||
        g_map_page_calls != 0) {
        return fail("expected anonymous mmap to register a lazy fault-backed user region");
    }

    if (linux_compat_vm_munmap(&vm, addr, 2U * MEMORY_PAGE_SIZE) != 0) {
        return fail("expected munmap of exact mapping to succeed");
    }
    if (g_unmap_page_calls != 0 || g_pmm_free_page_calls < 1 ||
        g_unregister_user_region_calls != 1) {
        return fail("expected munmap to unregister lazy region and release slot table");
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
        g_map_page_calls != 1 ||
        find_vm_region(&vm, LINUX_COMPAT_MMAP_BASE) == NULL) {
        return fail("expected mmap syscall to register a lazy region through bound VM");
    }
    mmap_addr = (uintptr_t)response.value;

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MUNMAP;
    request.addr = mmap_addr;
    request.length = MEMORY_PAGE_SIZE;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != 0 ||
        g_unregister_user_region_calls == 0) {
        return fail("expected munmap syscall to unregister lazy mapping through bound VM");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_syscall_mmap_fd_copies_rootfs_file_bytes(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    int32_t fd = -1;
    const linux_compat_rootfs_node_t* node = NULL;
    linux_compat_vm_region_t* region = NULL;
    uint8_t* mapped_page = NULL;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/usr/bin/git";
    request.flags = LINUX_COMPAT_O_RDONLY;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected test rootfs file to open before mmap");
    }
    fd = (int32_t)response.value;
    node = (const linux_compat_rootfs_node_t*)runtime.fds[fd].node;
    if (runtime.fds[fd].overlay_node || node == NULL || node->data == NULL ||
        node->size < 16U) {
        return fail("expected opened fd to reference lower rootfs bytes");
    }

    runtime.vm = &vm;
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_EXEC;
    request.fd = fd;
    request.offset = 0;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_MMAP_BASE) {
        return fail("expected mmap(fd) to allocate a deterministic VM region");
    }

    region = find_vm_region(&vm, LINUX_COMPAT_MMAP_BASE);
    if (region == NULL || region->object.backing.anon.page_slots[0] == 0U) {
        return fail("expected mmap(fd) to create a mapped backing page");
    }
    mapped_page = (uint8_t*)region->object.backing.anon.page_slots[0];
    if (memcmp(mapped_page, node->data, 16U) != 0) {
        return fail("expected mmap(fd) page to contain rootfs file bytes");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_mmap_base_avoids_platform_mmio_ranges(void) {
    const uintptr_t mmap_base = LINUX_COMPAT_MMAP_BASE;

    if (ranges_overlap(mmap_base,
                       MEMORY_PAGE_SIZE,
                       UART_BASE,
                       MEMORY_PAGE_SIZE) ||
        ranges_overlap(mmap_base,
                       MEMORY_PAGE_SIZE,
                       STORAGE_BASE,
                       MEMORY_PAGE_SIZE) ||
        ranges_overlap(mmap_base,
                       MEMORY_PAGE_SIZE,
                       AI_ACCEL_BASE,
                       MEMORY_PAGE_SIZE)) {
        return fail("expected Linux compat mmap base to avoid supervisor MMIO fault ranges");
    }
    return 0;
}

static int test_syscall_mmap_fixed_fd_replaces_existing_page(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    int32_t fd = -1;
    const linux_compat_rootfs_node_t* node = NULL;
    linux_compat_vm_region_t* fixed_region = NULL;
    uint8_t* mapped_page = NULL;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/usr/bin/git";
    request.flags = LINUX_COMPAT_O_RDONLY;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected test rootfs file to open before fixed mmap");
    }
    fd = (int32_t)response.value;
    node = (const linux_compat_rootfs_node_t*)runtime.fds[fd].node;
    if (node == NULL || node->data == NULL ||
        node->size < MEMORY_PAGE_SIZE + 16U) {
        return fail("expected lower rootfs file to cover a nonzero page offset");
    }

    runtime.vm = &vm;
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.length = 2U * MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ;
    request.fd = fd;
    request.offset = 0;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_MMAP_BASE) {
        return fail("expected initial mmap(fd) to allocate a two-page region");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = LINUX_COMPAT_MMAP_BASE;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE;
    request.flags = LINUX_COMPAT_MAP_FIXED;
    request.fd = fd;
    request.offset = MEMORY_PAGE_SIZE;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_MMAP_BASE) {
        return fail("expected MAP_FIXED mmap(fd) to replace the existing page");
    }

    fixed_region = find_vm_region(&vm, LINUX_COMPAT_MMAP_BASE);
    if (fixed_region == NULL ||
        fixed_region->object.backing.anon.page_slots[0] == 0U) {
        return fail("expected MAP_FIXED mmap(fd) to own a backing page");
    }
    mapped_page = (uint8_t*)fixed_region->object.backing.anon.page_slots[0];
    if (memcmp(mapped_page, node->data + MEMORY_PAGE_SIZE, 16U) != 0) {
        return fail("expected MAP_FIXED mmap(fd) to copy bytes from file offset");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_syscall_mmap_fd_accepts_unaligned_nonfixed_hint(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    int32_t fd = -1;
    const linux_compat_rootfs_node_t* node = NULL;
    linux_compat_vm_region_t* region = NULL;
    uint8_t* mapped_page = NULL;
    const uintptr_t hint = LINUX_COMPAT_MMAP_BASE + 0x710U;
    const uintptr_t expected = align_up_test_page(hint);

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_OPENAT;
    request.dirfd = LINUX_COMPAT_AT_FDCWD;
    request.path = "/usr/bin/git";
    request.flags = LINUX_COMPAT_O_RDONLY;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value < 3) {
        return fail("expected test rootfs file to open before hinted mmap");
    }
    fd = (int32_t)response.value;
    node = (const linux_compat_rootfs_node_t*)runtime.fds[fd].node;
    if (node == NULL || node->data == NULL || node->size < 16U) {
        return fail("expected lower rootfs file bytes for hinted mmap");
    }

    runtime.vm = &vm;
    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = hint;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_EXEC;
    request.flags = 0x2U;
    request.fd = fd;
    request.offset = 0;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)expected ||
        vm.next_mmap != expected + MEMORY_PAGE_SIZE) {
        return fail("expected non-fixed unaligned mmap hint to map at page-aligned hint");
    }

    region = find_vm_region(&vm, expected);
    if (region == NULL || region->object.backing.anon.page_slots[0] == 0U) {
        return fail("expected hinted mmap(fd) to create a mapped backing page");
    }
    mapped_page = (uint8_t*)region->object.backing.anon.page_slots[0];
    if (memcmp(mapped_page, node->data, 16U) != 0) {
        return fail("expected hinted mmap(fd) page to contain file bytes");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_mmap_has_room_for_dynamic_interp_and_shared_library_regions(void) {
    static uint8_t libpcre2_file[0x95000U]
        __attribute__((aligned(MEMORY_PAGE_SIZE)));
    static uint8_t libz_file[0x15000U]
        __attribute__((aligned(MEMORY_PAGE_SIZE)));
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    size_t i = 0;
    uintptr_t mapped = 0;
    const uintptr_t main_segments[] = {
        LINUX_COMPAT_DYN_LOAD_BIAS,
        LINUX_COMPAT_DYN_LOAD_BIAS + 0x1000U,
        LINUX_COMPAT_DYN_LOAD_BIAS + 0x201000U,
        LINUX_COMPAT_DYN_LOAD_BIAS + 0x280000U,
    };
    const uintptr_t interp_segments[] = {
        LINUX_COMPAT_INTERP_LOAD_BIAS,
        LINUX_COMPAT_INTERP_LOAD_BIAS + 0x1000U,
        LINUX_COMPAT_INTERP_LOAD_BIAS + 0x94000U,
    };

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);
    runtime.vm = &vm;

    for (i = 0; i < sizeof(main_segments) / sizeof(main_segments[0]); ++i) {
        if (linux_compat_vm_map_fixed(&vm,
                                      main_segments[i],
                                      MEMORY_PAGE_SIZE,
                                      LINUX_COMPAT_PROT_READ |
                                          LINUX_COMPAT_PROT_EXEC,
                                      0U) == NULL) {
            return fail("expected dynamic main segments to fit before shared libraries");
        }
    }
    for (i = 0; i < sizeof(interp_segments) / sizeof(interp_segments[0]); ++i) {
        if (linux_compat_vm_map_fixed(&vm,
                                      interp_segments[i],
                                      MEMORY_PAGE_SIZE,
                                      LINUX_COMPAT_PROT_READ |
                                          LINUX_COMPAT_PROT_EXEC,
                                      0U) == NULL) {
            return fail("expected interpreter segments to fit before shared libraries");
        }
    }
    if (linux_compat_vm_map_fixed(&vm,
                                  LINUX_COMPAT_STACK_TOP -
                                      (8U * MEMORY_PAGE_SIZE),
                                  8U * MEMORY_PAGE_SIZE,
                                  LINUX_COMPAT_PROT_READ |
                                      LINUX_COMPAT_PROT_WRITE,
                                  0U) == NULL) {
        return fail("expected dynamic exec stack to fit before shared libraries");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_BRK;
    request.addr = LINUX_COMPAT_BRK_BASE + MEMORY_PAGE_SIZE;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)request.addr) {
        return fail("expected dynamic loader brk region to fit before shared libraries");
    }

    mapped = linux_compat_vm_mmap_file(&vm,
                                       LINUX_COMPAT_INTERP_LOAD_BIAS + 0x35710U,
                                       sizeof(libpcre2_file),
                                       LINUX_COMPAT_PROT_READ |
                                           LINUX_COMPAT_PROT_EXEC,
                                       0x2U,
                                       libpcre2_file,
                                       sizeof(libpcre2_file),
                                       0U);
    if (mapped != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected readonly libpcre2 mmap to fall back and fit after dynamic exec setup");
    }
    mapped = linux_compat_vm_mmap_file(&vm,
                                       LINUX_COMPAT_INTERP_LOAD_BIAS + 0x35710U,
                                       sizeof(libz_file),
                                       LINUX_COMPAT_PROT_READ |
                                           LINUX_COMPAT_PROT_EXEC,
                                       0x2U,
                                       libz_file,
                                       sizeof(libz_file),
                                       0U);
    if ((intptr_t)mapped < 0 || find_vm_region(&vm, mapped) == NULL) {
        return fail("expected second shared library mmap to fit after libpcre2");
    }
    for (i = 0; i < 24U; ++i) {
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        request.number = LINUX_COMPAT_SYS_MMAP;
        request.length = MEMORY_PAGE_SIZE;
        request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE;
        request.flags = 0x22U;
        request.fd = -1;
        if (linux_compat_syscall_dispatch(&runtime,
                                          &request,
                                          &response,
                                          &trace) != LINUX_COMPAT_OK ||
            response.value < 0 ||
            find_vm_region(&vm, (uintptr_t)response.value) == NULL) {
            return fail("expected Git commit anonymous mmap growth to fit after shared libraries");
        }
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = LINUX_COMPAT_INTERP_LOAD_BIAS + 0x35710U;
    request.length = 69632U;
    request.prot = LINUX_COMPAT_PROT_READ | LINUX_COMPAT_PROT_WRITE;
    request.flags = 0x22U;
    request.fd = -1;
    if (linux_compat_syscall_dispatch(&runtime,
                                      &request,
                                      &response,
                                      &trace) != LINUX_COMPAT_OK ||
        response.value < 0 ||
        find_vm_region(&vm, (uintptr_t)response.value) == NULL) {
        return fail("expected Git deflate anonymous mmap with loader hint to fit under region pressure");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_syscall_mmap_nonfixed_hint_collision_falls_back_to_next_mmap(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    const uintptr_t occupied = LINUX_COMPAT_MMAP_BASE + MEMORY_PAGE_SIZE;
    const uintptr_t hint = occupied - 0x710U;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);
    runtime.vm = &vm;

    if (linux_compat_vm_map_fixed(&vm,
                                  occupied,
                                  MEMORY_PAGE_SIZE,
                                  LINUX_COMPAT_PROT_READ,
                                  0U) == NULL) {
        return fail("expected fixed setup mapping to succeed");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = hint;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = LINUX_COMPAT_PROT_READ;
    request.flags = 0x2U;
    request.fd = -1;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_MMAP_BASE ||
        find_vm_region(&vm, LINUX_COMPAT_MMAP_BASE) == NULL) {
        return fail("expected non-fixed mmap hint collision to fall back to next_mmap");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_syscall_mremap_maymove_grows_anonymous_mapping(void) {
    const uint64_t kSysMremap = 216U;
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;
    uintptr_t old_addr = 0;
    uintptr_t new_addr = 0;
    uintptr_t old_page = 0;
    linux_compat_vm_region_t* old_region = NULL;
    linux_compat_vm_region_t* moved_region = NULL;
    const char payload[] = "mremap";
    char readback[sizeof(payload)] = {0};

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);
    linux_compat_runtime_init(&runtime);
    runtime.vm = &vm;

    old_addr = linux_compat_vm_mmap(&vm,
                                    0U,
                                    MEMORY_PAGE_SIZE,
                                    LINUX_COMPAT_PROT_READ |
                                        LINUX_COMPAT_PROT_WRITE,
                                    0x22U);
    if ((intptr_t)old_addr < 0 ||
        !linux_compat_vm_write_user(&vm,
                                    old_addr,
                                    payload,
                                    sizeof(payload))) {
        return fail("expected mremap setup mapping to be writable");
    }
    old_region = find_vm_region(&vm, old_addr);
    if (old_region == NULL ||
        old_region->object.backing.anon.page_slots[0] == 0U) {
        return fail("expected mremap setup to allocate the source anon page");
    }
    old_page = old_region->object.backing.anon.page_slots[0];

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.number = kSysMremap;
    request.addr = old_addr;
    request.length = MEMORY_PAGE_SIZE;
    request.offset = 2U * MEMORY_PAGE_SIZE;
    request.flags = 1U;
    if (linux_compat_syscall_dispatch(&runtime,
                                      &request,
                                      &response,
                                      &trace) != LINUX_COMPAT_OK ||
        response.value < 0) {
        return fail("expected mremap MAYMOVE growth to succeed");
    }
    new_addr = (uintptr_t)response.value;
    moved_region = find_vm_region(&vm, new_addr);
    if (new_addr == old_addr ||
        find_vm_region(&vm, old_addr) != NULL ||
        moved_region == NULL ||
        moved_region->object.backing.anon.page_slots[0] != old_page ||
        !linux_compat_vm_read_user(&vm,
                                   new_addr,
                                   readback,
                                   sizeof(readback)) ||
        memcmp(readback, payload, sizeof(payload)) != 0) {
        return fail("expected mremap to move anon backing into a larger mapping");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_mmap_allows_git_sized_anonymous_growth(void) {
    const size_t git_realloc_size = 8601380U;
    const size_t mapped_length = align_up_test_page(git_realloc_size);
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uintptr_t mapped = 0;
    linux_compat_vm_region_t* region = NULL;

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    mapped = linux_compat_vm_mmap(&vm,
                                  0U,
                                  git_realloc_size,
                                  LINUX_COMPAT_PROT_READ |
                                      LINUX_COMPAT_PROT_WRITE,
                                  0x22U);
    if (mapped != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected Git-sized anonymous mmap to fit Linux compat VM");
    }
    region = find_vm_region(&vm, mapped);
    if (region == NULL ||
        region->length != mapped_length ||
        region->region.object_mode != VM_REGION_OBJECT_FAULT ||
        region->object.backing_kind != VM_OBJECT_BACKING_ANON ||
        g_map_page_calls != 0) {
        return fail("expected Git-sized anonymous mmap to register the rounded lazy span");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_anonymous_mmap_defers_page_allocation_under_pmm_pressure(void) {
    const size_t mapping_size = 256U * MEMORY_PAGE_SIZE;
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_vm_region_t* region = NULL;
    uintptr_t mapped = 0;

    reset_stub_state();
    g_alloc_zeroed_page_limit = 4U;
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    mapped = linux_compat_vm_mmap(&vm,
                                  0U,
                                  mapping_size,
                                  LINUX_COMPAT_PROT_READ |
                                      LINUX_COMPAT_PROT_WRITE,
                                  0x22U);
    if (mapped != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected anonymous mmap to succeed without eagerly consuming all PMM pages");
    }

    region = find_vm_region(&vm, mapped);
    if (region == NULL ||
        region->region.object_mode != VM_REGION_OBJECT_FAULT ||
        region->object.backing_kind != VM_OBJECT_BACKING_ANON ||
        region->object.backing.anon.page_slots == 0 ||
        region->object.backing.anon.page_slots[0] != 0U ||
        g_map_page_calls != 0) {
        return fail("expected anonymous mmap to register a lazy fault-backed object");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_mmap_file_uses_physical_mapping_for_aligned_readonly_asset(void) {
    static uint8_t padded_file[2U * MEMORY_PAGE_SIZE]
        __attribute__((aligned(MEMORY_PAGE_SIZE)));
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_vm_region_t* region = NULL;
    uintptr_t mapped = 0;

    reset_stub_state();
    memset(padded_file, 0, sizeof(padded_file));
    padded_file[0] = 0x7fU;
    padded_file[1] = 'E';
    padded_file[2] = 'L';
    padded_file[3] = 'F';
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    mapped = linux_compat_vm_mmap_file(&vm,
                                       0U,
                                       sizeof(padded_file),
                                       LINUX_COMPAT_PROT_READ |
                                           LINUX_COMPAT_PROT_EXEC,
                                       0x2U,
                                       padded_file,
                                       MEMORY_PAGE_SIZE + 32U,
                                       0U);
    if (mapped != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected aligned readonly file mmap to succeed");
    }

    region = find_vm_region(&vm, mapped);
    if (region == NULL ||
        region->object.backing_kind != VM_OBJECT_BACKING_PHYSICAL ||
        region->object.backing.physical.base_paddr !=
            (uintptr_t)padded_file) {
        return fail("expected aligned readonly file mmap to use physical backing");
    }
    if (region->region.object_mode != VM_REGION_OBJECT_FAULT ||
        g_map_page_calls != 0) {
        return fail("expected aligned readonly file mmap to register a lazy fault-backed mapping");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_mmap_fixed_writable_file_segment_over_physical_mapping(void) {
    static uint8_t padded_file[3U * MEMORY_PAGE_SIZE]
        __attribute__((aligned(MEMORY_PAGE_SIZE)));
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_vm_region_t* region = NULL;
    uintptr_t mapped = 0;
    uint8_t patched[16];
    size_t i = 0;

    reset_stub_state();
    for (i = 0; i < sizeof(padded_file); ++i) {
        padded_file[i] = (uint8_t)(i & 0xffU);
    }
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    mapped = linux_compat_vm_mmap_file(&vm,
                                       0U,
                                       sizeof(padded_file),
                                       LINUX_COMPAT_PROT_READ |
                                           LINUX_COMPAT_PROT_EXEC,
                                       0x2U,
                                       padded_file,
                                       sizeof(padded_file),
                                       0U);
    if (mapped != LINUX_COMPAT_MMAP_BASE) {
        return fail("expected initial physical file mmap to succeed");
    }

    mapped = linux_compat_vm_mmap_file(&vm,
                                       LINUX_COMPAT_MMAP_BASE +
                                           MEMORY_PAGE_SIZE,
                                       MEMORY_PAGE_SIZE,
                                       LINUX_COMPAT_PROT_READ |
                                           LINUX_COMPAT_PROT_WRITE,
                                       LINUX_COMPAT_MAP_FIXED | 0x2U,
                                       padded_file,
                                       sizeof(padded_file),
                                       MEMORY_PAGE_SIZE);
    if (mapped != LINUX_COMPAT_MMAP_BASE + MEMORY_PAGE_SIZE) {
        return fail("expected MAP_FIXED writable file segment over physical mmap to succeed");
    }
    region = find_vm_region(&vm, LINUX_COMPAT_MMAP_BASE);
    if (region == NULL ||
        region->object.backing_kind != VM_OBJECT_BACKING_ANON) {
        return fail("expected MAP_FIXED writable private segment to detach from physical asset backing");
    }
    if (!linux_compat_vm_read_user(&vm,
                                   LINUX_COMPAT_MMAP_BASE + MEMORY_PAGE_SIZE,
                                   patched,
                                   sizeof(patched)) ||
        memcmp(patched, padded_file + MEMORY_PAGE_SIZE, sizeof(patched)) != 0) {
        return fail("expected fixed writable file segment to expose file bytes");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_syscall_mmap_fixed_prot_none_replaces_existing_page(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    linux_compat_runtime_t runtime;
    linux_compat_syscall_request_t request;
    linux_compat_syscall_response_t response;
    linux_compat_trace_t trace;

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
        return fail("expected brk setup to map the page being replaced");
    }

    memset(&request, 0, sizeof(request));
    request.number = LINUX_COMPAT_SYS_MMAP;
    request.addr = LINUX_COMPAT_BRK_BASE;
    request.length = MEMORY_PAGE_SIZE;
    request.prot = 0;
    request.flags = LINUX_COMPAT_MAP_FIXED | 0x20U | 0x2U;
    request.fd = -1;
    if (linux_compat_syscall_dispatch(&runtime, &request, &response, &trace) !=
            LINUX_COMPAT_OK ||
        response.value != (int64_t)LINUX_COMPAT_BRK_BASE ||
        g_unmap_page_calls == 0 ||
        !can_map_page(&address_space, LINUX_COMPAT_BRK_BASE)) {
        return fail("expected MAP_FIXED PROT_NONE mmap to replace an existing page");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

static int test_write_only_mmap_supports_kernel_copyin(void) {
    vm_address_space_t address_space;
    vm_process_t process;
    linux_compat_vm_t vm;
    uintptr_t addr = 0;
    linux_compat_vm_region_t* region = NULL;
    const char payload[] = "git object buffer";
    char readback[sizeof(payload)] = {0};

    reset_stub_state();
    if (!make_process(&address_space, &process)) {
        return fail("expected test process setup to succeed");
    }
    linux_compat_vm_init(&vm, &address_space, &process);

    addr = linux_compat_vm_mmap(&vm,
                                0U,
                                MEMORY_PAGE_SIZE,
                                LINUX_COMPAT_PROT_WRITE,
                                0x22U);
    region = find_vm_region(&vm, addr);
    if (addr != LINUX_COMPAT_MMAP_BASE ||
        region == NULL ||
        (region->region.flags & VM_PAGE_READ) == 0U ||
        (region->region.flags & VM_PAGE_WRITE) == 0U) {
        return fail("expected PROT_WRITE mmap to install readable writable RISC-V pages");
    }
    if (!linux_compat_vm_write_user(&vm,
                                    addr,
                                    payload,
                                    sizeof(payload))) {
        return fail("expected write-only mmap setup to accept user writes");
    }
    if (!linux_compat_vm_read_user(&vm,
                                   addr,
                                   readback,
                                   sizeof(readback)) ||
        memcmp(readback, payload, sizeof(payload)) != 0) {
        return fail("expected kernel copy-in to read from PROT_WRITE mmap");
    }

    linux_compat_vm_destroy(&vm);
    return 0;
}

int main(void) {
    if (test_brk_maps_heap_pages_and_shrink_releases_backing() != 0 ||
        test_mmap_maps_user_pages_and_munmap_releases_them() != 0 ||
        test_syscall_dispatch_uses_bound_linux_compat_vm() != 0 ||
        test_syscall_mmap_fd_copies_rootfs_file_bytes() != 0 ||
        test_mmap_base_avoids_platform_mmio_ranges() != 0 ||
        test_syscall_mmap_fixed_fd_replaces_existing_page() != 0 ||
        test_syscall_mmap_fd_accepts_unaligned_nonfixed_hint() != 0 ||
        test_mmap_has_room_for_dynamic_interp_and_shared_library_regions() != 0 ||
        test_syscall_mmap_nonfixed_hint_collision_falls_back_to_next_mmap() != 0 ||
        test_syscall_mremap_maymove_grows_anonymous_mapping() != 0 ||
        test_mmap_allows_git_sized_anonymous_growth() != 0 ||
        test_anonymous_mmap_defers_page_allocation_under_pmm_pressure() != 0 ||
        test_mmap_file_uses_physical_mapping_for_aligned_readonly_asset() != 0 ||
        test_mmap_fixed_writable_file_segment_over_physical_mapping() != 0 ||
        test_syscall_mmap_fixed_prot_none_replaces_existing_page() != 0 ||
        test_write_only_mmap_supports_kernel_copyin() != 0) {
        return 1;
    }

    return 0;
}

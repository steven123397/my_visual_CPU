#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"

typedef struct PageMapCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    uint64_t flags;
} page_map_call_t;

static uint64_t g_pages[8][SV39_LEVEL_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static size_t g_next_page = 0;
static int g_alloc_zeroed_page_calls = 0;
static int g_pmm_free_page_calls = 0;
static void* g_freed_pages[8];
static size_t g_freed_page_count = 0;
static bool g_can_map_page_default = true;
static bool g_can_map_page_responses[8];
static size_t g_can_map_page_response_count = 0;
static size_t g_can_map_page_response_index = 0;
static int g_can_map_page_calls = 0;
static int g_map_page_calls = 0;
static page_map_call_t g_map_page_records[8];
static bool g_map_page_result = true;
static int g_unmap_page_calls = 0;
static uintptr_t g_unmap_page_vaddrs[8];
static bool g_unmap_page_result = true;
static int g_flush_if_enabled_calls = 0;

static void reset_stub_state(void);
static int fail(const char* message);
static void queue_can_map_page_response(bool value);
static int test_object_init_resolve_and_reset(void);
static int test_region_map_clear_and_fault_binding(void);

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
    (void)address_space;
    (void)vaddr;
    g_can_map_page_calls += 1;
    if (g_can_map_page_response_index < g_can_map_page_response_count) {
        return g_can_map_page_responses[g_can_map_page_response_index++];
    }

    return g_can_map_page_default;
}

bool map_page_internal(vm_address_space_t* address_space,
                       uintptr_t vaddr,
                       uintptr_t paddr,
                       uint64_t flags) {
    if (address_space == NULL || !g_map_page_result ||
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
    if (address_space == NULL || !g_unmap_page_result ||
        g_unmap_page_calls >= (int)(sizeof(g_unmap_page_vaddrs) /
                                    sizeof(g_unmap_page_vaddrs[0]))) {
        return false;
    }

    g_unmap_page_vaddrs[g_unmap_page_calls++] = vaddr;
    return true;
}

void flush_tlb_if_enabled(void) {
    g_flush_if_enabled_calls += 1;
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, VM_USER_VADDR_LIMIT);
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

static void reset_stub_state(void) {
    memset(g_pages, 0, sizeof(g_pages));
    g_next_page = 0;
    g_alloc_zeroed_page_calls = 0;
    g_pmm_free_page_calls = 0;
    memset(g_freed_pages, 0, sizeof(g_freed_pages));
    g_freed_page_count = 0;
    g_can_map_page_default = true;
    memset(g_can_map_page_responses, 0, sizeof(g_can_map_page_responses));
    g_can_map_page_response_count = 0;
    g_can_map_page_response_index = 0;
    g_can_map_page_calls = 0;
    g_map_page_calls = 0;
    memset(g_map_page_records, 0, sizeof(g_map_page_records));
    g_map_page_result = true;
    g_unmap_page_calls = 0;
    memset(g_unmap_page_vaddrs, 0, sizeof(g_unmap_page_vaddrs));
    g_unmap_page_result = true;
    g_flush_if_enabled_calls = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void queue_can_map_page_response(bool value) {
    if (g_can_map_page_response_count <
        (sizeof(g_can_map_page_responses) / sizeof(g_can_map_page_responses[0]))) {
        g_can_map_page_responses[g_can_map_page_response_count++] = value;
    }
}

static int test_object_init_resolve_and_reset(void) {
    vm_object_t physical = {0};
    vm_object_t anon = {0};
    uintptr_t paddr = 0;
    uintptr_t* anon_slot_page = NULL;

    reset_stub_state();
    if (!vm_object_init_physical(&physical, 4U * MEMORY_PAGE_SIZE, 2U * MEMORY_PAGE_SIZE)) {
        return fail("expected physical object init to succeed");
    }

    if (!object_resolve_page(&physical, MEMORY_PAGE_SIZE, false, &paddr) ||
        paddr != 5U * MEMORY_PAGE_SIZE) {
        return fail("expected physical object resolve to return direct backing");
    }

    if (!vm_object_reset(&physical) || physical.initialized) {
        return fail("expected physical object reset to clear descriptor");
    }

    if (!vm_object_init_anon(&anon, 2U * MEMORY_PAGE_SIZE) ||
        g_alloc_zeroed_page_calls != 1 || anon.backing.anon.page_slots == NULL) {
        return fail("expected anon object init to allocate slot page");
    }
    anon_slot_page = anon.backing.anon.page_slots;

    if (object_resolve_page(&anon, 0, false, &paddr)) {
        return fail("expected anon object resolve without create to fail on empty slot");
    }

    if (!object_resolve_page(&anon, MEMORY_PAGE_SIZE, true, &paddr) ||
        paddr != (uintptr_t)g_pages[1]) {
        return fail("expected anon object resolve to allocate page on demand");
    }

    if (!vm_object_reset(&anon) || anon.initialized ||
        g_pmm_free_page_calls != 2 ||
        g_freed_pages[0] != g_pages[1] ||
        g_freed_pages[1] != anon_slot_page) {
        return fail("expected anon object reset to free pages and slot page");
    }

    return 0;
}

static int test_region_map_clear_and_fault_binding(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = g_pages[7],
    };
    vm_user_region_t region = {
        .address_space = &address_space,
        .vaddr = 0,
        .size = MEMORY_PAGE_SIZE,
        .flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
        .registered = true,
    };
    vm_object_t mapped = {0};
    vm_object_t fault = {0};

    reset_stub_state();
    if (!vm_object_init_physical(&mapped, 8U * MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE) ||
        !vm_object_init_physical(&fault, 12U * MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE)) {
        return fail("expected object init before region binding to succeed");
    }

    if (!vm_user_region_map_object_at(&region, &mapped, 0)) {
        return fail("expected mapped region-object bind to succeed");
    }

    if (g_can_map_page_calls != 1 || g_map_page_calls != 1 ||
        g_map_page_records[0].vaddr != region.vaddr ||
        g_map_page_records[0].paddr != mapped.backing.physical.base_paddr ||
        g_map_page_records[0].flags != region.flags ||
        g_flush_if_enabled_calls != 1 || mapped.attachment_count != 1 ||
        region.object != &mapped || region.object_mode != VM_REGION_OBJECT_MAPPED) {
        return fail("expected mapped bind to map pages and flush");
    }

    queue_can_map_page_response(false);
    if (!vm_user_region_clear_object(&region)) {
        return fail("expected region clear object to succeed");
    }

    if (g_unmap_page_calls != 1 || g_unmap_page_vaddrs[0] != region.vaddr ||
        g_flush_if_enabled_calls != 2 || mapped.attachment_count != 0 ||
        region.object != NULL || region.object_mode != VM_REGION_OBJECT_NONE) {
        return fail("expected clear object to unmap pages, detach object and flush");
    }

    if (!vm_user_region_set_fault_object_at(&region, &fault, 0)) {
        return fail("expected fault object bind to succeed");
    }

    if (g_map_page_calls != 1 || g_flush_if_enabled_calls != 2 ||
        fault.attachment_count != 1 || region.object != &fault ||
        region.object_mode != VM_REGION_OBJECT_FAULT) {
        return fail("expected fault bind to avoid eager mapping and flush");
    }

    return 0;
}

int main(void) {
    if (test_object_init_resolve_and_reset() != 0 ||
        test_region_map_clear_and_fault_binding() != 0) {
        return 1;
    }

    return 0;
}

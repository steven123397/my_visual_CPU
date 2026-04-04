#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/memory.h"
#include "../../guest/include/supervisor_demo_smoke.h"

enum {
    TEST_PAGE_COUNT = 5U,
};

static _Alignas(TRAP_USER_RUNTIME_STACK_ALIGNMENT)
    uint8_t g_page_pool[TEST_PAGE_COUNT][MEMORY_PAGE_SIZE];
static size_t g_total_pages = 0;
static size_t g_allocated_pages = 0;

static void reset_stub_state(size_t total_pages);
static int fail(const char* message);
static int test_alloc_pages_rejects_null_output(void);
static int test_alloc_pages_fills_five_pages_when_capacity_is_available(void);

void pmm_init(void) {
}

void* pmm_alloc_page(void) {
    if (g_allocated_pages >= g_total_pages ||
        g_allocated_pages >= TEST_PAGE_COUNT) {
        return NULL;
    }

    return g_page_pool[g_allocated_pages++];
}

bool pmm_free_page(void* page) {
    (void)page;
    return false;
}

uintptr_t pmm_managed_start(void) {
    return 0;
}

uintptr_t pmm_managed_end(void) {
    return 0;
}

size_t pmm_total_pages(void) {
    return g_total_pages;
}

size_t pmm_free_pages(void) {
    return g_total_pages - g_allocated_pages;
}

size_t pmm_used_pages(void) {
    return g_allocated_pages;
}

static void reset_stub_state(size_t total_pages) {
    memset(g_page_pool, 0, sizeof(g_page_pool));
    g_total_pages = total_pages;
    g_allocated_pages = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_alloc_pages_rejects_null_output(void) {
    reset_stub_state(TEST_PAGE_COUNT);

    if (supervisor_demo_smoke_alloc_pages_for_test(NULL)) {
        return fail("expected alloc_pages_for_test to reject null output");
    }

    if (g_allocated_pages != 0) {
        return fail("expected null output path to avoid any pmm allocation");
    }

    return 0;
}

static int test_alloc_pages_fills_five_pages_when_capacity_is_available(void) {
    supervisor_demo_smoke_pages_t pages = {0};

    reset_stub_state(TEST_PAGE_COUNT);

    if (!supervisor_demo_smoke_alloc_pages_for_test(&pages)) {
        return fail("expected alloc_pages_for_test to succeed with five pages");
    }

    if (pages.backing_page != (uint32_t*)g_page_pool[0] ||
        pages.remap_page != (uint32_t*)g_page_pool[1] ||
        pages.nx_page != (uint32_t*)g_page_pool[2] ||
        pages.user_stack_page != (uint32_t*)g_page_pool[3] ||
        pages.user_trap_stack_page != g_page_pool[4]) {
        return fail("expected alloc_pages_for_test to assign each pmm page in order");
    }

    if ((((uintptr_t)pages.user_trap_stack_page) &
         (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) != 0) {
        return fail("expected user trap stack page to preserve trap alignment");
    }

    if (pmm_used_pages() != TEST_PAGE_COUNT || pmm_free_pages() != 0) {
        return fail("expected alloc_pages_for_test to consume exactly five pages");
    }

    return 0;
}

int main(void) {
    if (test_alloc_pages_rejects_null_output() != 0 ||
        test_alloc_pages_fills_five_pages_when_capacity_is_available() != 0) {
        return 1;
    }

    return 0;
}

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
static bool g_page_in_use[TEST_PAGE_COUNT];
static size_t g_pmm_free_page_calls = 0;
static bool g_free_pages_override_enabled = false;
static size_t g_free_pages_override_value = 0;
static bool g_used_pages_override_enabled = false;
static size_t g_used_pages_override_value = 0;

static void reset_stub_state(size_t total_pages);
static size_t allocated_page_count(void);
static void set_free_pages_override(size_t value);
static void set_used_pages_override(size_t value);
static bool pages_cleared(const supervisor_demo_smoke_pages_t* pages);
static int fail(const char* message);
static int test_alloc_pages_rejects_null_output(void);
static int test_alloc_pages_fills_five_pages_when_capacity_is_available(void);
static int test_alloc_pages_releases_partial_allocation_on_failure(void);
static int test_alloc_pages_releases_fully_allocated_pages_on_tail_check_failure(void);
static int test_probe_storage_page_releases_allocated_page_on_failure(void);

void pmm_init(void) {
}

void* pmm_alloc_page(void) {
    size_t index = 0;
    for (index = 0; index < g_total_pages && index < TEST_PAGE_COUNT; ++index) {
        if (!g_page_in_use[index]) {
            g_page_in_use[index] = true;
            return g_page_pool[index];
        }
    }

    return NULL;
}

bool pmm_free_page(void* page) {
    size_t index = 0;
    g_pmm_free_page_calls += 1;
    for (index = 0; index < g_total_pages && index < TEST_PAGE_COUNT; ++index) {
        if (page == g_page_pool[index]) {
            if (!g_page_in_use[index]) {
                return false;
            }

            g_page_in_use[index] = false;
            return true;
        }
    }

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
    if (g_free_pages_override_enabled) {
        return g_free_pages_override_value;
    }

    return g_total_pages - allocated_page_count();
}

size_t pmm_used_pages(void) {
    if (g_used_pages_override_enabled) {
        return g_used_pages_override_value;
    }

    return allocated_page_count();
}

static void reset_stub_state(size_t total_pages) {
    memset(g_page_pool, 0, sizeof(g_page_pool));
    memset(g_page_in_use, 0, sizeof(g_page_in_use));
    g_total_pages = total_pages;
    g_pmm_free_page_calls = 0;
    g_free_pages_override_enabled = false;
    g_free_pages_override_value = 0;
    g_used_pages_override_enabled = false;
    g_used_pages_override_value = 0;
}

static size_t allocated_page_count(void) {
    size_t count = 0;
    size_t index = 0;

    for (index = 0; index < g_total_pages && index < TEST_PAGE_COUNT; ++index) {
        if (g_page_in_use[index]) {
            count += 1;
        }
    }

    return count;
}

static void set_free_pages_override(size_t value) {
    g_free_pages_override_enabled = true;
    g_free_pages_override_value = value;
}

static void set_used_pages_override(size_t value) {
    g_used_pages_override_enabled = true;
    g_used_pages_override_value = value;
}

static bool pages_cleared(const supervisor_demo_smoke_pages_t* pages) {
    return pages != NULL && pages->backing_page == NULL &&
           pages->remap_page == NULL && pages->nx_page == NULL &&
           pages->user_stack_page == NULL &&
           pages->user_trap_stack_page == NULL;
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

    if (allocated_page_count() != 0) {
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

static int test_alloc_pages_releases_partial_allocation_on_failure(void) {
    supervisor_demo_smoke_pages_t pages = {0};

    reset_stub_state(3U);

    if (supervisor_demo_smoke_alloc_pages_for_test(&pages)) {
        return fail("expected alloc_pages_for_test to fail when PMM runs out mid-allocation");
    }

    if (allocated_page_count() != 0) {
        return fail("expected partial allocation failure to release all allocated pages");
    }

    if (g_pmm_free_page_calls != 3U) {
        return fail("expected partial allocation failure to free each allocated page once");
    }

    if (!pages_cleared(&pages)) {
        return fail("expected partial allocation failure to clear the output page bundle");
    }

    return 0;
}

static int test_alloc_pages_releases_fully_allocated_pages_on_tail_check_failure(void) {
    supervisor_demo_smoke_pages_t pages = {0};

    reset_stub_state(TEST_PAGE_COUNT);
    set_used_pages_override(4U);

    if (supervisor_demo_smoke_alloc_pages_for_test(&pages)) {
        return fail("expected alloc_pages_for_test to fail when tail PMM usage validation fails");
    }

    if (allocated_page_count() != 0) {
        return fail("expected tail validation failure to release all five allocated pages");
    }

    if (g_pmm_free_page_calls != TEST_PAGE_COUNT) {
        return fail("expected tail validation failure to free each allocated page once");
    }

    if (!pages_cleared(&pages)) {
        return fail("expected tail validation failure to clear the output page bundle");
    }

    return 0;
}

static int test_probe_storage_page_releases_allocated_page_on_failure(void) {
    reset_stub_state(TEST_PAGE_COUNT);
    set_free_pages_override(2U);

    if (supervisor_demo_smoke_probe_storage_page_for_test()) {
        return fail("expected probe_storage_page_for_test to fail when PMM accounting check fails");
    }

    if (allocated_page_count() != 0) {
        return fail("expected probe_storage_page failure to release the allocated storage page");
    }

    if (g_pmm_free_page_calls != 1U) {
        return fail("expected probe_storage_page failure to free the storage page exactly once");
    }

    return 0;
}

int main(void) {
    if (test_alloc_pages_rejects_null_output() != 0 ||
        test_alloc_pages_fills_five_pages_when_capacity_is_available() != 0 ||
        test_alloc_pages_releases_partial_allocation_on_failure() != 0 ||
        test_alloc_pages_releases_fully_allocated_pages_on_tail_check_failure() != 0 ||
        test_probe_storage_page_releases_allocated_page_on_failure() != 0) {
        return 1;
    }

    return 0;
}

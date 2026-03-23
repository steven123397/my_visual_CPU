#include "pmm.h"

#include <stddef.h>
#include <stdint.h>

#include "memory.h"

static uint8_t* page_bitmap = NULL;
static uintptr_t managed_start_ptr = 0;
static uintptr_t managed_end_ptr = 0;
static size_t managed_page_count = 0;
static size_t free_page_count = 0;
static size_t next_search_index = 0;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    const uintptr_t mask = (uintptr_t)alignment - 1U;
    return (value + mask) & ~mask;
}

static size_t bitmap_bytes_for_pages(size_t page_count) {
    return (page_count + 7U) / 8U;
}

static bool page_bit_is_set(size_t page_index) {
    return (page_bitmap[page_index / 8U] &
            (uint8_t)(1U << (page_index & 7U))) != 0;
}

static void page_bit_set(size_t page_index) {
    page_bitmap[page_index / 8U] |= (uint8_t)(1U << (page_index & 7U));
}

static void page_bit_clear(size_t page_index) {
    page_bitmap[page_index / 8U] &= (uint8_t)~(1U << (page_index & 7U));
}

void pmm_init(void) {
    const uintptr_t physical_limit = memory_heap_limit();
    const uintptr_t early_cursor = memory_heap_current();
    const uintptr_t aligned_cursor = align_up(early_cursor, MEMORY_PAGE_SIZE);
    const size_t max_page_count =
        aligned_cursor >= physical_limit
            ? 0
            : (size_t)((physical_limit - aligned_cursor) / MEMORY_PAGE_SIZE);
    const size_t bitmap_bytes = bitmap_bytes_for_pages(max_page_count);
    size_t i = 0;

    page_bitmap = NULL;
    managed_start_ptr = 0;
    managed_end_ptr = 0;
    managed_page_count = 0;
    free_page_count = 0;
    next_search_index = 0;

    if (bitmap_bytes != 0) {
        page_bitmap = (uint8_t*)memory_alloc(bitmap_bytes, sizeof(uintptr_t));
    }

    managed_start_ptr = memory_finalize_early_allocator();
    managed_end_ptr = physical_limit;

    if (managed_start_ptr == 0 || managed_start_ptr >= managed_end_ptr) {
        managed_start_ptr = managed_end_ptr;
        return;
    }

    managed_page_count =
        (size_t)((managed_end_ptr - managed_start_ptr) / MEMORY_PAGE_SIZE);
    if (managed_page_count == 0 || page_bitmap == NULL) {
        managed_start_ptr = managed_end_ptr;
        managed_page_count = 0;
        return;
    }

    for (i = 0; i < bitmap_bytes; ++i) {
        page_bitmap[i] = 0;
    }

    free_page_count = managed_page_count;
}

void* pmm_alloc_page(void) {
    size_t probe = 0;

    if (free_page_count == 0 || managed_page_count == 0 || page_bitmap == NULL) {
        return NULL;
    }

    for (probe = 0; probe < managed_page_count; ++probe) {
        const size_t page_index = (next_search_index + probe) % managed_page_count;

        if (page_bit_is_set(page_index)) {
            continue;
        }

        page_bit_set(page_index);
        free_page_count -= 1;
        next_search_index = (page_index + 1U) % managed_page_count;
        return (void*)(managed_start_ptr +
                       page_index * (uintptr_t)MEMORY_PAGE_SIZE);
    }

    return NULL;
}

bool pmm_free_page(void* page) {
    const uintptr_t address = (uintptr_t)page;
    size_t page_index = 0;

    if (page == NULL ||
        managed_page_count == 0 ||
        address < managed_start_ptr ||
        address >= managed_end_ptr ||
        (address & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    page_index = (size_t)((address - managed_start_ptr) / MEMORY_PAGE_SIZE);
    if (!page_bit_is_set(page_index)) {
        return false;
    }

    page_bit_clear(page_index);
    free_page_count += 1;
    if (page_index < next_search_index) {
        next_search_index = page_index;
    }
    return true;
}

uintptr_t pmm_managed_start(void) {
    return managed_start_ptr;
}

uintptr_t pmm_managed_end(void) {
    return managed_end_ptr;
}

size_t pmm_total_pages(void) {
    return managed_page_count;
}

size_t pmm_free_pages(void) {
    return free_page_count;
}

size_t pmm_used_pages(void) {
    return managed_page_count - free_page_count;
}

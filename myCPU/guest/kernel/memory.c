#include "memory.h"

#include <stddef.h>
#include <stdint.h>

#include "platform_mmio.h"

extern char __kernel_start[];
extern char __kernel_end[];
extern char __heap_start[];

static uintptr_t heap_start_ptr = 0;
static uintptr_t heap_cursor = 0;
static uintptr_t heap_limit_ptr = 0;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    const uintptr_t mask = (uintptr_t)alignment - 1U;
    return (value + mask) & ~mask;
}

void memory_init(void) {
    heap_start_ptr = (uintptr_t)__heap_start;
    heap_cursor = heap_start_ptr;
    heap_limit_ptr = MEM_BASE + MEM_SIZE;
}

void* memory_alloc(size_t size, size_t alignment) {
    uintptr_t aligned = 0;

    if (alignment == 0 || (alignment & (alignment - 1U)) != 0) {
        return NULL;
    }

    aligned = align_up(heap_cursor, alignment);
    if (aligned > heap_limit_ptr || size > heap_limit_ptr - aligned) {
        return NULL;
    }

    heap_cursor = aligned + size;
    return (void*)aligned;
}

void* memory_alloc_pages(size_t page_count) {
    if (page_count == 0 || page_count > ((size_t)-1) / MEMORY_PAGE_SIZE) {
        return NULL;
    }

    return memory_alloc(page_count * MEMORY_PAGE_SIZE, MEMORY_PAGE_SIZE);
}

uintptr_t memory_kernel_start(void) {
    return (uintptr_t)__kernel_start;
}

uintptr_t memory_kernel_end(void) {
    return (uintptr_t)__kernel_end;
}

uintptr_t memory_heap_start(void) {
    return heap_start_ptr;
}

uintptr_t memory_heap_current(void) {
    return heap_cursor;
}

uintptr_t memory_heap_limit(void) {
    return heap_limit_ptr;
}

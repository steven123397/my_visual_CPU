#include "memory.h"

#include <stddef.h>
#include <stdint.h>

#include "platform_mmio.h"

extern char __kernel_start[];
extern char __kernel_end[];
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];
extern char __heap_start[];

static uintptr_t heap_start_ptr = 0;
static uintptr_t heap_cursor = 0;
static uintptr_t heap_limit_ptr = 0;
static uintptr_t heap_alloc_limit_ptr = 0;

/* 把 value 向上对齐到 alignment（必须为 2 的幂）。 */
static uintptr_t align_up(uintptr_t value, size_t alignment) {
    const uintptr_t mask = (uintptr_t)alignment - 1U;
    return (value + mask) & ~mask;
}

/* freestanding memset：把 ptr 的 size 字节置为 value。 */
void* memset(void* ptr, int value, size_t size) {
    size_t i = 0;
    uint8_t* bytes = (uint8_t*)ptr;

    if (ptr == NULL) {
        return NULL;
    }
    for (i = 0; i < size; ++i) {
        bytes[i] = (uint8_t)value;
    }
    return ptr;
}

void memory_init(void) {
    heap_start_ptr = (uintptr_t)__heap_start;
    heap_cursor = heap_start_ptr;
    heap_limit_ptr = MEM_BASE + MEM_SIZE;
    heap_alloc_limit_ptr = heap_limit_ptr;
}

void* memory_alloc(size_t size, size_t alignment) {
    uintptr_t aligned = 0;

    if (alignment == 0 || (alignment & (alignment - 1U)) != 0) {
        return NULL;
    }

    aligned = align_up(heap_cursor, alignment);
    if (aligned > heap_alloc_limit_ptr || size > heap_alloc_limit_ptr - aligned) {
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

uintptr_t memory_finalize_early_allocator(void) {
    const uintptr_t aligned = align_up(heap_cursor, MEMORY_PAGE_SIZE);

    if (aligned > heap_alloc_limit_ptr) {
        return 0;
    }

    heap_cursor = aligned;
    heap_alloc_limit_ptr = aligned;
    return aligned;
}

uintptr_t memory_kernel_start(void) {
    return (uintptr_t)__kernel_start;
}

uintptr_t memory_kernel_end(void) {
    return (uintptr_t)__kernel_end;
}

uintptr_t memory_text_start(void) {
    return (uintptr_t)__text_start;
}

uintptr_t memory_text_end(void) {
    return (uintptr_t)__text_end;
}

uintptr_t memory_rodata_start(void) {
    return (uintptr_t)__rodata_start;
}

uintptr_t memory_rodata_end(void) {
    return (uintptr_t)__rodata_end;
}

uintptr_t memory_data_start(void) {
    return (uintptr_t)__data_start;
}

uintptr_t memory_data_end(void) {
    return (uintptr_t)__data_end;
}

uintptr_t memory_bss_start(void) {
    return (uintptr_t)__bss_start;
}

uintptr_t memory_bss_end(void) {
    return (uintptr_t)__bss_end;
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

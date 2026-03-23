#pragma once

#include <stddef.h>
#include <stdint.h>

#define MEMORY_PAGE_SIZE 4096U

void memory_init(void);
void* memory_alloc(size_t size, size_t alignment);
void* memory_alloc_pages(size_t page_count);

uintptr_t memory_kernel_start(void);
uintptr_t memory_kernel_end(void);
uintptr_t memory_heap_start(void);
uintptr_t memory_heap_current(void);
uintptr_t memory_heap_limit(void);

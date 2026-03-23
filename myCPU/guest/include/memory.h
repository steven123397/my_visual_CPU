#pragma once

#include <stddef.h>
#include <stdint.h>

#define MEMORY_PAGE_SIZE 4096U

void memory_init(void);
void* memory_alloc(size_t size, size_t alignment);
void* memory_alloc_pages(size_t page_count);
uintptr_t memory_finalize_early_allocator(void);

uintptr_t memory_kernel_start(void);
uintptr_t memory_kernel_end(void);
uintptr_t memory_text_start(void);
uintptr_t memory_text_end(void);
uintptr_t memory_rodata_start(void);
uintptr_t memory_rodata_end(void);
uintptr_t memory_data_start(void);
uintptr_t memory_data_end(void);
uintptr_t memory_bss_start(void);
uintptr_t memory_bss_end(void);
uintptr_t memory_heap_start(void);
uintptr_t memory_heap_current(void);
uintptr_t memory_heap_limit(void);

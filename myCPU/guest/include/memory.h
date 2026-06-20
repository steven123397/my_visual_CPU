#pragma once

#include <stddef.h>
#include <stdint.h>

/* 早期内存分配器与内核段边界：基于 linker symbol 的 bump allocator，
   在 PMM 接管前供 bring-up 使用，并提供各内核段起止地址查询。 */
#define MEMORY_PAGE_SIZE 4096U

/* 初始化早期 bump 分配器：堆游标指向 __heap_start，上限为 RAM 末尾。 */
void memory_init(void);
/* 按 alignment 对齐分配 size 字节，越界返回 NULL（早期 bump，不回收）。 */
void* memory_alloc(size_t size, size_t alignment);
/* 分配连续 page_count 个 4KB 页。 */
void* memory_alloc_pages(size_t page_count);
/* 把堆游标对齐到页边界并固定上限，返回 PMM 起始地址。 */
uintptr_t memory_finalize_early_allocator(void);

/* 内核镜像起始地址（linker symbol）。 */
uintptr_t memory_kernel_start(void);
/* 内核镜像结束地址。 */
uintptr_t memory_kernel_end(void);
/* .text 段起始地址。 */
uintptr_t memory_text_start(void);
/* .text 段结束地址。 */
uintptr_t memory_text_end(void);
/* .rodata 段起始地址。 */
uintptr_t memory_rodata_start(void);
/* .rodata 段结束地址。 */
uintptr_t memory_rodata_end(void);
/* .data 段起始地址。 */
uintptr_t memory_data_start(void);
/* .data 段结束地址。 */
uintptr_t memory_data_end(void);
/* .bss 段起始地址。 */
uintptr_t memory_bss_start(void);
/* .bss 段结束地址。 */
uintptr_t memory_bss_end(void);
/* 早期堆起始地址（__heap_start）。 */
uintptr_t memory_heap_start(void);
/* 早期堆当前游标。 */
uintptr_t memory_heap_current(void);
/* 早期堆上限（RAM 末尾）。 */
uintptr_t memory_heap_limit(void);

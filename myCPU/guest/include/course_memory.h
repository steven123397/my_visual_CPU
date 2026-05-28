#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_MEMORY_MAX_FRAMES 16U
#define COURSE_MEMORY_MAX_KMALLOC_BLOCKS 8U
#define COURSE_MEMORY_KMALLOC_BLOCK_SIZE 64U

typedef struct CourseMemoryStats {
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t used_pages;
    uint32_t page_faults;
    uint32_t page_reclaims;
    uint32_t kmalloc_allocs;
    uint32_t kfree_calls;
    uint32_t kmalloc_reuses;
} course_memory_stats_t;

typedef struct CourseMemoryFrame {
    bool resident;
    uint32_t page_id;
    bool referenced;
    bool dirty;
} course_memory_frame_t;

typedef struct CourseKmallocBlock {
    bool allocated;
    bool ever_allocated;
    size_t size;
    unsigned char storage[COURSE_MEMORY_KMALLOC_BLOCK_SIZE];
} course_kmalloc_block_t;

typedef struct CourseMemory {
    course_memory_frame_t frames[COURSE_MEMORY_MAX_FRAMES];
    course_kmalloc_block_t blocks[COURSE_MEMORY_MAX_KMALLOC_BLOCKS];
    uint32_t frame_count;
    uint32_t clock_hand;
    course_memory_stats_t stats;
} course_memory_t;

void course_memory_init(course_memory_t* memory, uint32_t frame_count);
bool course_memory_touch(course_memory_t* memory, uint32_t page_id, bool write);
bool course_memory_stats(const course_memory_t* memory,
                         course_memory_stats_t* out_stats);
void* course_kmalloc(course_memory_t* memory, size_t size);
void course_kfree(course_memory_t* memory, void* ptr);

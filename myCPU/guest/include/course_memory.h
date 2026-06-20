#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 课程 OS 内存模块：用固定小数组模拟页框、Clock 置换和 kmalloc 复用证据。
   它服务课程 smoke 与 /proc 展示，不替代 guest/kernel/ 下的真实页表与 PMM。 */
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
    /* resident 表示这个模拟页框里是否装入了一个课程级虚拟页。 */
    bool resident;
    uint32_t page_id;
    /* referenced/dirty 是 Clock 置换与写访问统计的可观察证据。 */
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
    /* frame_count 可小于数组容量，便于单测构造可预测的缺页/回收序列。 */
    course_memory_frame_t frames[COURSE_MEMORY_MAX_FRAMES];
    course_kmalloc_block_t blocks[COURSE_MEMORY_MAX_KMALLOC_BLOCKS];
    uint32_t frame_count;
    uint32_t clock_hand;
    course_memory_stats_t stats;
} course_memory_t;

/* 初始化课程内存模型：设置页框数量、清空页框与 kmalloc 块、归零统计。 */
void course_memory_init(course_memory_t* memory, uint32_t frame_count);
/* 访问一个课程虚拟页：命中则更新访问/脏位，缺页则装入或 Clock 置换。 */
bool course_memory_touch(course_memory_t* memory, uint32_t page_id, bool write);
/* 拷贝出内存模型统计（缺页、回收、kmalloc 分配/复用等）。 */
bool course_memory_stats(const course_memory_t* memory,
                         course_memory_stats_t* out_stats);
/* 从固定块池分配最多 64 字节，复用已释放块并记复用计数。 */
void* course_kmalloc(course_memory_t* memory, size_t size);
/* 释放 kmalloc 返回的指针，标记块可用但不擦除内容。 */
void course_kfree(course_memory_t* memory, void* ptr);

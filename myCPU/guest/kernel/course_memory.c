#include "course_memory.h"

#include <stddef.h>

static uint32_t count_used_frames(const course_memory_t* memory) {
    uint32_t i = 0;
    uint32_t used = 0;

    for (i = 0; i < memory->frame_count; ++i) {
        if (memory->frames[i].resident) {
            used += 1U;
        }
    }

    return used;
}

static int find_resident_frame(const course_memory_t* memory, uint32_t page_id) {
    uint32_t i = 0;

    for (i = 0; i < memory->frame_count; ++i) {
        if (memory->frames[i].resident && memory->frames[i].page_id == page_id) {
            return (int)i;
        }
    }

    return -1;
}

static int find_free_frame(const course_memory_t* memory) {
    uint32_t i = 0;

    for (i = 0; i < memory->frame_count; ++i) {
        if (!memory->frames[i].resident) {
            return (int)i;
        }
    }

    return -1;
}

static uint32_t clock_select_victim(course_memory_t* memory) {
    uint32_t probes = 0;

    while (probes < memory->frame_count * 2U) {
        course_memory_frame_t* frame = &memory->frames[memory->clock_hand];
        const uint32_t selected = memory->clock_hand;

        memory->clock_hand = (memory->clock_hand + 1U) % memory->frame_count;
        if (!frame->referenced) {
            return selected;
        }
        frame->referenced = false;
        probes += 1U;
    }

    return memory->clock_hand;
}

static void install_page(course_memory_t* memory,
                         uint32_t frame_index,
                         uint32_t page_id,
                         bool write) {
    course_memory_frame_t* frame = &memory->frames[frame_index];

    frame->resident = true;
    frame->page_id = page_id;
    frame->referenced = true;
    frame->dirty = write;
}

void course_memory_init(course_memory_t* memory, uint32_t frame_count) {
    uint32_t i = 0;

    if (memory == NULL) {
        return;
    }

    if (frame_count > COURSE_MEMORY_MAX_FRAMES) {
        frame_count = COURSE_MEMORY_MAX_FRAMES;
    }

    memory->frame_count = frame_count;
    memory->clock_hand = 0;
    memory->stats.total_pages = frame_count;
    memory->stats.free_pages = frame_count;
    memory->stats.used_pages = 0;
    memory->stats.page_faults = 0;
    memory->stats.page_reclaims = 0;
    memory->stats.kmalloc_allocs = 0;
    memory->stats.kfree_calls = 0;
    memory->stats.kmalloc_reuses = 0;

    for (i = 0; i < COURSE_MEMORY_MAX_FRAMES; ++i) {
        memory->frames[i].resident = false;
        memory->frames[i].page_id = 0;
        memory->frames[i].referenced = false;
        memory->frames[i].dirty = false;
    }
    for (i = 0; i < COURSE_MEMORY_MAX_KMALLOC_BLOCKS; ++i) {
        memory->blocks[i].allocated = false;
        memory->blocks[i].ever_allocated = false;
        memory->blocks[i].size = 0;
    }
}

bool course_memory_touch(course_memory_t* memory, uint32_t page_id, bool write) {
    int frame_index = 0;

    if (memory == NULL || memory->frame_count == 0) {
        return false;
    }

    frame_index = find_resident_frame(memory, page_id);
    if (frame_index >= 0) {
        memory->frames[frame_index].referenced = true;
        memory->frames[frame_index].dirty =
            memory->frames[frame_index].dirty || write;
        return true;
    }

    memory->stats.page_faults += 1U;
    frame_index = find_free_frame(memory);
    if (frame_index < 0) {
        frame_index = (int)clock_select_victim(memory);
        memory->stats.page_reclaims += 1U;
    }

    install_page(memory, (uint32_t)frame_index, page_id, write);
    memory->stats.used_pages = count_used_frames(memory);
    memory->stats.free_pages = memory->stats.total_pages - memory->stats.used_pages;
    return true;
}

bool course_memory_stats(const course_memory_t* memory,
                         course_memory_stats_t* out_stats) {
    if (memory == NULL || out_stats == NULL) {
        return false;
    }

    *out_stats = memory->stats;
    return true;
}

void* course_kmalloc(course_memory_t* memory, size_t size) {
    uint32_t i = 0;

    if (memory == NULL || size == 0 ||
        size > COURSE_MEMORY_KMALLOC_BLOCK_SIZE) {
        return NULL;
    }

    for (i = 0; i < COURSE_MEMORY_MAX_KMALLOC_BLOCKS; ++i) {
        course_kmalloc_block_t* block = &memory->blocks[i];

        if (block->allocated) {
            continue;
        }
        if (block->ever_allocated) {
            memory->stats.kmalloc_reuses += 1U;
        }
        block->allocated = true;
        block->ever_allocated = true;
        block->size = size;
        memory->stats.kmalloc_allocs += 1U;
        return block->storage;
    }

    return NULL;
}

void course_kfree(course_memory_t* memory, void* ptr) {
    uint32_t i = 0;

    if (memory == NULL || ptr == NULL) {
        return;
    }

    for (i = 0; i < COURSE_MEMORY_MAX_KMALLOC_BLOCKS; ++i) {
        course_kmalloc_block_t* block = &memory->blocks[i];

        if (ptr == block->storage && block->allocated) {
            block->allocated = false;
            block->size = 0;
            memory->stats.kfree_calls += 1U;
            return;
        }
    }
}

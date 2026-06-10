#pragma once

#include <stdint.h>

#include "course_process.h"

#define COURSE_SYNC_MAX_WAITERS COURSE_PROCESS_MAX_PROCESSES

typedef enum CourseSyncResult {
    COURSE_SYNC_OK = 0,
    COURSE_SYNC_BLOCKED = 1,
    COURSE_SYNC_ERR_BAD_PROCESS = -1,
    COURSE_SYNC_ERR_NOT_OWNER = -2,
} course_sync_result_t;

typedef struct CourseSemaphore {
    course_process_table_t* processes;
    int32_t value;
    uint32_t waiters[COURSE_SYNC_MAX_WAITERS];
    uint32_t waiter_count;
} course_semaphore_t;

typedef struct CourseMutex {
    course_process_table_t* processes;
    uint32_t owner_pid;
    uint32_t waiters[COURSE_SYNC_MAX_WAITERS];
    uint32_t waiter_count;
    uint32_t misuse_guard_count;
} course_mutex_t;

void course_semaphore_init(course_semaphore_t* semaphore,
                           course_process_table_t* processes,
                           int32_t value);
course_sync_result_t course_semaphore_wait(course_semaphore_t* semaphore,
                                           uint32_t pid);
course_sync_result_t course_semaphore_post(course_semaphore_t* semaphore);

void course_mutex_init(course_mutex_t* mutex, course_process_table_t* processes);
course_sync_result_t course_mutex_lock(course_mutex_t* mutex, uint32_t pid);
course_sync_result_t course_mutex_unlock(course_mutex_t* mutex, uint32_t pid);

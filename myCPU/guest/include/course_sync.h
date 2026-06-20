#pragma once

#include <stdint.h>

#include "course_process.h"

/* 课程同步模型：用可计数 semaphore 和 owner-based mutex 展示等待/唤醒语义。
   它不试图实现完整 pthread/futex，只服务课程 OS 的同步演示和单测。 */
#define COURSE_SYNC_MAX_WAITERS COURSE_PROCESS_MAX_PROCESSES

typedef enum CourseSyncResult {
    COURSE_SYNC_OK = 0,
    COURSE_SYNC_BLOCKED = 1,
    COURSE_SYNC_ERR_BAD_PROCESS = -1,
    COURSE_SYNC_ERR_NOT_OWNER = -2,
} course_sync_result_t;

typedef struct CourseSemaphore {
    /* processes 是外部进程表引用；wait/post 只负责切换课程进程状态。 */
    course_process_table_t* processes;
    int32_t value;
    uint32_t waiters[COURSE_SYNC_MAX_WAITERS];
    uint32_t waiter_count;
} course_semaphore_t;

typedef struct CourseMutex {
    /* owner_pid 为 0 表示未持有；misuse_guard_count 记录非 owner 解锁等误用。 */
    course_process_table_t* processes;
    uint32_t owner_pid;
    uint32_t waiters[COURSE_SYNC_MAX_WAITERS];
    uint32_t waiter_count;
    uint32_t misuse_guard_count;
} course_mutex_t;

/* 初始化信号量，绑定进程表并设初值。 */
void course_semaphore_init(course_semaphore_t* semaphore,
                           course_process_table_t* processes,
                           int32_t value);
/* wait：value>0 则减 1，否则把调用进程置 BLOCKED 入等待队列。 */
course_sync_result_t course_semaphore_wait(course_semaphore_t* semaphore,
                                           uint32_t pid);
/* post：value++，若有等待者唤醒队首。 */
course_sync_result_t course_semaphore_post(course_semaphore_t* semaphore);

/* 初始化互斥锁，绑定进程表。 */
void course_mutex_init(course_mutex_t* mutex, course_process_table_t* processes);
/* lock：未持有则取锁，已被持有则把调用进程入等待队列。 */
course_sync_result_t course_mutex_lock(course_mutex_t* mutex, uint32_t pid);
/* unlock：owner 释放锁并唤醒队首等待者，非 owner 记误用。 */
course_sync_result_t course_mutex_unlock(course_mutex_t* mutex, uint32_t pid);

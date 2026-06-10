#include "course_sync.h"

static void clear_waiters(uint32_t* waiters, uint32_t count) {
    uint32_t i = 0;

    if (waiters == 0) {
        return;
    }
    for (i = 0; i < count; ++i) {
        waiters[i] = 0;
    }
}

static course_process_t* find_process(course_process_table_t* table,
                                      uint32_t pid) {
    course_process_t* process = course_process_find(table, pid);

    if (process == 0 || process->state == COURSE_PROCESS_DEAD ||
        process->state == COURSE_PROCESS_UNUSED) {
        return 0;
    }
    return process;
}

static bool enqueue_waiter(uint32_t* waiters,
                           uint32_t* waiter_count,
                           uint32_t pid) {
    if (waiters == 0 || waiter_count == 0 ||
        *waiter_count >= COURSE_SYNC_MAX_WAITERS) {
        return false;
    }
    waiters[*waiter_count] = pid;
    *waiter_count += 1U;
    return true;
}

static uint32_t pop_waiter(uint32_t* waiters, uint32_t* waiter_count) {
    uint32_t pid = 0;
    uint32_t i = 0;

    if (waiters == 0 || waiter_count == 0 || *waiter_count == 0U) {
        return 0;
    }
    pid = waiters[0];
    for (i = 1U; i < *waiter_count; ++i) {
        waiters[i - 1U] = waiters[i];
    }
    *waiter_count -= 1U;
    waiters[*waiter_count] = 0;
    return pid;
}

void course_semaphore_init(course_semaphore_t* semaphore,
                           course_process_table_t* processes,
                           int32_t value) {
    if (semaphore == 0) {
        return;
    }
    semaphore->processes = processes;
    semaphore->value = value;
    semaphore->waiter_count = 0;
    clear_waiters(semaphore->waiters, COURSE_SYNC_MAX_WAITERS);
}

course_sync_result_t course_semaphore_wait(course_semaphore_t* semaphore,
                                           uint32_t pid) {
    course_process_t* process = 0;

    if (semaphore == 0) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    process = find_process(semaphore->processes, pid);
    if (process == 0) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    if (semaphore->value > 0) {
        semaphore->value -= 1;
        return COURSE_SYNC_OK;
    }
    if (!enqueue_waiter(semaphore->waiters, &semaphore->waiter_count, pid) ||
        !course_process_set_state(semaphore->processes,
                                  pid,
                                  COURSE_PROCESS_BLOCKED)) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    return COURSE_SYNC_BLOCKED;
}

course_sync_result_t course_semaphore_post(course_semaphore_t* semaphore) {
    uint32_t pid = 0;

    if (semaphore == 0) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    pid = pop_waiter(semaphore->waiters, &semaphore->waiter_count);
    if (pid != 0U) {
        if (!course_process_set_state(semaphore->processes,
                                      pid,
                                      COURSE_PROCESS_READY)) {
            return COURSE_SYNC_ERR_BAD_PROCESS;
        }
        return COURSE_SYNC_OK;
    }
    semaphore->value += 1;
    return COURSE_SYNC_OK;
}

void course_mutex_init(course_mutex_t* mutex, course_process_table_t* processes) {
    if (mutex == 0) {
        return;
    }
    mutex->processes = processes;
    mutex->owner_pid = 0;
    mutex->waiter_count = 0;
    mutex->misuse_guard_count = 0;
    clear_waiters(mutex->waiters, COURSE_SYNC_MAX_WAITERS);
}

course_sync_result_t course_mutex_lock(course_mutex_t* mutex, uint32_t pid) {
    course_process_t* process = 0;

    if (mutex == 0) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    process = find_process(mutex->processes, pid);
    if (process == 0) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    if (mutex->owner_pid == 0U) {
        mutex->owner_pid = pid;
        return COURSE_SYNC_OK;
    }
    if (!enqueue_waiter(mutex->waiters, &mutex->waiter_count, pid) ||
        !course_process_set_state(mutex->processes,
                                  pid,
                                  COURSE_PROCESS_BLOCKED)) {
        return COURSE_SYNC_ERR_BAD_PROCESS;
    }
    return COURSE_SYNC_BLOCKED;
}

course_sync_result_t course_mutex_unlock(course_mutex_t* mutex, uint32_t pid) {
    uint32_t next_pid = 0;

    if (mutex == 0 || mutex->owner_pid == 0U) {
        return COURSE_SYNC_ERR_NOT_OWNER;
    }
    if (mutex->owner_pid != pid) {
        mutex->misuse_guard_count += 1U;
        return COURSE_SYNC_ERR_NOT_OWNER;
    }
    next_pid = pop_waiter(mutex->waiters, &mutex->waiter_count);
    if (next_pid != 0U) {
        mutex->owner_pid = next_pid;
        if (!course_process_set_state(mutex->processes,
                                      next_pid,
                                      COURSE_PROCESS_READY)) {
            return COURSE_SYNC_ERR_BAD_PROCESS;
        }
    } else {
        mutex->owner_pid = 0;
    }
    return COURSE_SYNC_OK;
}

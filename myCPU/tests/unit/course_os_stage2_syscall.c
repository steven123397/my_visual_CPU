#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/course_syscall.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_syscall_numbers_and_errors_are_stable(void) {
    if (COURSE_SYSCALL_READ != 0U ||
        COURSE_SYSCALL_WRITE != 1U ||
        COURSE_SYSCALL_OPEN != 2U ||
        COURSE_SYSCALL_CLOSE != 3U ||
        COURSE_SYSCALL_SEEK != 4U ||
        COURSE_SYSCALL_EXIT != 5U ||
        COURSE_SYSCALL_FORK != 6U ||
        COURSE_SYSCALL_EXEC != 7U ||
        COURSE_SYSCALL_WAIT != 8U ||
        COURSE_SYSCALL_WAITPID != 9U ||
        COURSE_SYSCALL_GETPID != 10U ||
        COURSE_SYSCALL_PS != 11U ||
        COURSE_SYSCALL_KILL != 12U ||
        COURSE_SYSCALL_COUNT != 13U) {
        return fail("expected syscall ABI numbers to remain stable");
    }

    if (COURSE_SYSCALL_OK != 0 ||
        COURSE_SYSCALL_ERR_INVALID_SYSCALL >= 0 ||
        COURSE_SYSCALL_ERR_BAD_USER_POINTER >= 0 ||
        COURSE_SYSCALL_ERR_BAD_FD >= 0 ||
        COURSE_SYSCALL_ERR_NO_SUCH_FILE >= 0 ||
        COURSE_SYSCALL_ERR_NO_CHILD >= 0 ||
        COURSE_SYSCALL_ERR_NO_MEMORY >= 0 ||
        COURSE_SYSCALL_ERR_PERMISSION_DENIED >= 0) {
        return fail("expected syscall error values to use negative errno style");
    }

    return 0;
}

static int test_write_exit_getpid_and_stats(void) {
    course_syscall_t syscalls;
    course_syscall_stats_t stats;
    char user_memory[64] = "hello";

    course_syscall_init(&syscalls,
                        42U,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));

    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_WRITE,
                                1U,
                                (uintptr_t)user_memory,
                                5U,
                                0U) != 5) {
        return fail("expected write(stdout) to return byte count");
    }
    if (!course_syscall_stdout_equals(&syscalls, "hello")) {
        return fail("expected write(stdout) to append to stdout buffer");
    }

    memcpy(user_memory, "err", 4U);
    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_WRITE,
                                2U,
                                (uintptr_t)user_memory,
                                3U,
                                0U) != 3) {
        return fail("expected write(stderr) to return byte count");
    }
    if (!course_syscall_stderr_equals(&syscalls, "err")) {
        return fail("expected write(stderr) to append to stderr buffer");
    }

    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_GETPID,
                                0U,
                                0U,
                                0U,
                                0U) != 42) {
        return fail("expected getpid to return process pid");
    }

    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_EXIT,
                                7U,
                                0U,
                                0U,
                                0U) != COURSE_SYSCALL_OK ||
        !course_syscall_exited(&syscalls) ||
        course_syscall_exit_code(&syscalls) != 7) {
        return fail("expected exit to record exit status");
    }

    if (!course_syscall_stats(&syscalls, &stats) ||
        stats.total_calls != 4U ||
        stats.calls[COURSE_SYSCALL_WRITE] != 2U ||
        stats.calls[COURSE_SYSCALL_GETPID] != 1U ||
        stats.calls[COURSE_SYSCALL_EXIT] != 1U ||
        stats.failures != 0U ||
        stats.last_error != COURSE_SYSCALL_OK) {
        return fail("expected syscall stats to track calls and success state");
    }

    return 0;
}

static int test_invalid_syscall_and_bad_user_pointer_are_isolated(void) {
    course_syscall_t syscalls;
    course_syscall_stats_t stats;
    char user_memory[16] = "abc";

    course_syscall_init(&syscalls,
                        7U,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));

    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_WRITE,
                                1U,
                                (uintptr_t)(user_memory + sizeof(user_memory)),
                                1U,
                                0U) != COURSE_SYSCALL_ERR_BAD_USER_POINTER) {
        return fail("expected bad user pointer to return an error");
    }
    if (!course_syscall_stdout_equals(&syscalls, "")) {
        return fail("expected bad user pointer to avoid stdout writes");
    }

    if (course_syscall_dispatch(&syscalls,
                                99U,
                                0U,
                                0U,
                                0U,
                                0U) != COURSE_SYSCALL_ERR_INVALID_SYSCALL) {
        return fail("expected invalid syscall number to return an error");
    }

    if (!course_syscall_stats(&syscalls, &stats) ||
        stats.total_calls != 2U ||
        stats.failures != 2U ||
        stats.last_error != COURSE_SYSCALL_ERR_INVALID_SYSCALL) {
        return fail("expected syscall failures to be counted");
    }

    return 0;
}

static int test_procfs_syscalls_output(void) {
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;
    course_syscall_t syscalls;
    procfs_t procfs;
    char user_memory[16] = "x";
    char out[512];

    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_fs_init(&fs);
    course_syscall_init(&syscalls,
                        1U,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));
    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_syscalls(&procfs, &syscalls);

    (void)course_syscall_dispatch(&syscalls,
                                  COURSE_SYSCALL_WRITE,
                                  1U,
                                  (uintptr_t)user_memory,
                                  1U,
                                  0U);
    (void)course_syscall_dispatch(&syscalls, 77U, 0U, 0U, 0U, 0U);

    if (!procfs_read(&procfs, "/proc/syscalls", out, sizeof(out)) ||
        !contains(out, "total_calls=2") ||
        !contains(out, "failures=1") ||
        !contains(out, "last_error=-1") ||
        !contains(out, "write=1") ||
        !contains(out, "getpid=0")) {
        return fail("expected /proc/syscalls to expose syscall counters");
    }

    return 0;
}

int main(void) {
    if (test_syscall_numbers_and_errors_are_stable() != 0 ||
        test_write_exit_getpid_and_stats() != 0 ||
        test_invalid_syscall_and_bad_user_pointer_are_isolated() != 0 ||
        test_procfs_syscalls_output() != 0) {
        return 1;
    }

    return 0;
}

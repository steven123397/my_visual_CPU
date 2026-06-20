/* Stage2 COW/crash 单测：验证用户崩溃隔离和父子页写入隔离。 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_process.h"
#include "../../guest/include/course_shell.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_cow_fork_shares_and_isolates_pages(void) {
    course_process_table_t table;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;
    course_process_cow_stats_t stats;
    uint8_t value = 0;

    course_process_table_init(&table);
    parent = course_process_spawn(&table, 0U, "parent");
    if (parent == NULL ||
        !course_process_map_user_page(&table, parent->pid, 0U, 'A') ||
        !course_process_read_user_byte(&table, parent->pid, 0U, 0U, &value) ||
        value != 'A') {
        return fail("expected parent user page to be mapped and readable");
    }

    child = course_process_fork(&table, parent->pid, "child");
    if (child == NULL ||
        !course_process_cow_stats(&table, &stats) ||
        stats.mapped_pages != 2U ||
        stats.shared_pages != 1U ||
        stats.cow_faults != 0U ||
        stats.copied_pages != 0U) {
        return fail("expected fork to share one COW page");
    }

    if (!course_process_write_user_byte(&table, child->pid, 0U, 0U, 'B') ||
        !course_process_read_user_byte(&table, parent->pid, 0U, 0U, &value) ||
        value != 'A' ||
        !course_process_read_user_byte(&table, child->pid, 0U, 0U, &value) ||
        value != 'B') {
        return fail("expected child COW write to isolate parent data");
    }

    if (!course_process_cow_stats(&table, &stats) ||
        stats.shared_pages != 0U ||
        stats.cow_faults != 1U ||
        stats.copied_pages != 1U) {
        return fail("expected COW fault and copy counters after first write");
    }

    return 0;
}

static int test_procfs_cow_and_crashlog(void) {
    course_process_table_t table;
    course_process_t* shell = NULL;
    course_process_t* child = NULL;
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;
    procfs_t procfs;
    char out[1024];
    int32_t status = 0;

    course_process_table_init(&table);
    shell = course_process_spawn(&table, 0U, "shell");
    if (shell == NULL ||
        !course_process_map_user_page(&table, shell->pid, 0U, 'x') ||
        course_process_fork(&table, shell->pid, "worker") == NULL) {
        return fail("expected process table to contain a COW fork");
    }

    child = course_process_fork(&table, shell->pid, "crash");
    if (child == NULL ||
        !course_process_record_crash(&table,
                                     child->pid,
                                     0x1000U,
                                     13U,
                                     0xBADU,
                                     "load-page-fault")) {
        return fail("expected crash report to be recorded");
    }

    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_fs_init(&fs);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_processes(&procfs, &table);

    if (!procfs_read(&procfs, "/proc/cow", out, sizeof(out)) ||
        !contains(out, "shared_pages=1") ||
        !contains(out, "cow_faults=0") ||
        !contains(out, "copied_pages=0")) {
        return fail("expected /proc/cow to expose COW counters");
    }

    if (!procfs_read(&procfs, "/proc/crashlog", out, sizeof(out)) ||
        !contains(out, "pid=3") ||
        !contains(out, "name=crash") ||
        !contains(out, "sepc=4096") ||
        !contains(out, "scause=13") ||
        !contains(out, "stval=2989") ||
        !contains(out, "reason=load-page-fault")) {
        return fail("expected /proc/crashlog to expose last user crash");
    }

    if (course_process_waitpid(&table,
                               shell->pid,
                               child->pid,
                               &status) != COURSE_PROCESS_OK ||
        status != COURSE_PROCESS_EXIT_CRASH) {
        return fail("expected parent to reap crashed child");
    }

    return 0;
}

static int test_shell_continues_after_crash_program(void) {
    static course_shell_t shell;
    char out[1024];

    course_shell_init(&shell);
    if (!course_shell_run_line(&shell, "crash", out, sizeof(out)) ||
        !contains(out, "program=crash") ||
        !contains(out, "exit=-128") ||
        !contains(out, "crash=isolated")) {
        return fail("expected crash user program to be isolated");
    }

    if (!course_shell_run_line(&shell, "echo alive", out, sizeof(out)) ||
        !contains(out, "alive")) {
        return fail("expected shell to keep running after user crash");
    }

    if (!course_shell_run_line(&shell, "cat /proc/crashlog", out, sizeof(out)) ||
        !contains(out, "name=crash") ||
        !contains(out, "reason=user-crash")) {
        return fail("expected shell to expose crashlog through /proc");
    }

    return 0;
}

int main(void) {
    if (test_cow_fork_shares_and_isolates_pages() != 0 ||
        test_procfs_cow_and_crashlog() != 0 ||
        test_shell_continues_after_crash_program() != 0) {
        return 1;
    }

    return 0;
}

/* Stage2 进程单测：覆盖 spawn/fork/exec/wait/kill 和 procfs 进程视图。 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_process.h"
#include "../../guest/include/course_user_programs.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_pid_parent_state_exit_wait(void) {
    course_process_table_t table;
    course_process_t* init = NULL;
    course_process_t* child = NULL;
    int32_t status = 0;

    course_process_table_init(&table);
    init = course_process_spawn(&table, 0U, "init");
    child = course_process_fork(&table, init->pid, "child");
    if (init == NULL || child == NULL ||
        init->pid != 1U ||
        child->pid != 2U ||
        child->ppid != init->pid ||
        init->state != COURSE_PROCESS_READY ||
        child->state != COURSE_PROCESS_READY) {
        return fail("expected pid allocation and parent relation");
    }

    if (!course_process_set_state(&table, child->pid, COURSE_PROCESS_RUNNING) ||
        child->state != COURSE_PROCESS_RUNNING ||
        !course_process_exit(&table, child->pid, 17) ||
        child->state != COURSE_PROCESS_ZOMBIE ||
        child->exit_code != 17) {
        return fail("expected running child to exit into zombie");
    }

    if (course_process_waitpid(&table, child->pid, 999U, &status) !=
            COURSE_PROCESS_ERR_NO_CHILD ||
        course_process_waitpid(&table, init->pid, child->pid, &status) !=
            COURSE_PROCESS_OK ||
        status != 17 ||
        child->state != COURSE_PROCESS_DEAD) {
        return fail("expected waitpid to enforce parent ownership and reap child");
    }

    if (course_process_wait(&table, init->pid, &status) !=
        COURSE_PROCESS_ERR_NO_CHILD) {
        return fail("expected wait to report no child after reap");
    }

    return 0;
}

static int test_exec_user_program_catalog_and_procfs_ps(void) {
    course_process_table_t table;
    course_process_t* proc = NULL;
    course_user_program_t program;
    char ps[512];
    procfs_t procfs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;

    course_process_table_init(&table);
    proc = course_process_spawn(&table, 0U, "shell");
    if (proc == NULL) {
        return fail("expected process spawn to succeed");
    }

    if (course_user_program_count() < 5U ||
        !course_user_program_lookup("hello", &program) ||
        program.kind != COURSE_USER_PROGRAM_HELLO ||
        !course_user_program_lookup("echo", &program) ||
        !course_user_program_lookup("cat", &program) ||
        !course_user_program_lookup("forktest", &program) ||
        !course_user_program_lookup("crash", &program)) {
        return fail("expected five course user programs");
    }

    if (course_process_exec(&table, proc->pid, "missing", "x") !=
            COURSE_PROCESS_ERR_NO_SUCH_PROGRAM ||
        course_process_exec(&table, proc->pid, "hello", "arg0") !=
            COURSE_PROCESS_OK ||
        strcmp(proc->name, "hello") != 0 ||
        strcmp(proc->argv, "arg0") != 0 ||
        proc->entry_pc == 0 ||
        proc->user_sp == 0) {
        return fail("expected exec to load a known course user program");
    }

    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_fs_init(&fs);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_processes(&procfs, &table);
    if (!procfs_read(&procfs, "/proc/ps", ps, sizeof(ps)) ||
        !contains(ps, "pid=1") ||
        !contains(ps, "ppid=0") ||
        !contains(ps, "state=ready") ||
        !contains(ps, "name=hello")) {
        return fail("expected /proc/ps to expose real process table");
    }

    return 0;
}

static int test_crash_report_and_wait_reap(void) {
    course_process_table_t table;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;
    int32_t status = 0;

    course_process_table_init(&table);
    parent = course_process_spawn(&table, 0U, "shell");
    child = course_process_fork(&table, parent->pid, "crash");
    if (parent == NULL || child == NULL ||
        !course_process_record_crash(&table,
                                     child->pid,
                                     0x1000U,
                                     13U,
                                     0xBADU,
                                     "load-page-fault") ||
        child->state != COURSE_PROCESS_ZOMBIE ||
        child->exit_code != COURSE_PROCESS_EXIT_CRASH ||
        strcmp(child->crash_reason, "load-page-fault") != 0 ||
        child->crash_sepc != 0x1000U ||
        child->crash_scause != 13U ||
        child->crash_stval != 0xBADU) {
        return fail("expected crash report to zombie the child");
    }

    if (course_process_wait(&table, parent->pid, &status) != COURSE_PROCESS_OK ||
        status != COURSE_PROCESS_EXIT_CRASH ||
        child->state != COURSE_PROCESS_DEAD) {
        return fail("expected parent to reap crashed child");
    }

    return 0;
}

static int test_kill_sets_zombie_and_prevents_self_and_init(void) {
    course_process_table_t table;
    course_process_t* init = NULL;
    course_process_t* child = NULL;
    course_process_t* child2 = NULL;
    int32_t status = 0;

    course_process_table_init(&table);
    init = course_process_spawn(&table, 0U, "init");
    child = course_process_fork(&table, init->pid, "child");
    child2 = course_process_fork(&table, init->pid, "child2");
    if (init == NULL || child == NULL || child2 == NULL) {
        return fail("expected process table setup");
    }

    if (course_process_kill(&table, 999U, child->pid)) {
        return fail("expected kill from nonexistent caller to fail");
    }

    if (course_process_kill(&table, child->pid, child->pid)) {
        return fail("expected kill self to fail");
    }

    if (course_process_kill(&table, child->pid, init->pid)) {
        return fail("expected child to be unable to kill init");
    }

    if (!course_process_kill(&table, init->pid, child2->pid) ||
        child2->state != COURSE_PROCESS_ZOMBIE ||
        child2->exit_code != 9) {
        return fail("expected kill to set target to zombie with exit_code=9");
    }

    if (course_process_waitpid(&table, init->pid, child2->pid, &status) !=
            COURSE_PROCESS_OK ||
        status != 9 ||
        child2->state != COURSE_PROCESS_DEAD) {
        return fail("expected parent to reap killed child");
    }

    if (course_process_kill(&table, init->pid, 999U)) {
        return fail("expected kill nonexistent pid to fail");
    }

    return 0;
}

int main(void) {
    if (test_pid_parent_state_exit_wait() != 0 ||
        test_exec_user_program_catalog_and_procfs_ps() != 0 ||
        test_crash_report_and_wait_reap() != 0 ||
        test_kill_sets_zombie_and_prevents_self_and_init() != 0) {
        return 1;
    }

    return 0;
}

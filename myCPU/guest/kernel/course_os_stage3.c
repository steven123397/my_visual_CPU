#include "course_os_stage3.h"

#include "course_libc.h"
#include "course_sync.h"
#include "course_user_programs.h"

static size_t str_len(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return 0;
    }
    while (value[i] != '\0') {
        i += 1U;
    }
    return i;
}

static bool str_contains(const char* haystack, const char* needle) {
    size_t i = 0;
    const size_t needle_len = str_len(needle);

    if (haystack == 0 || needle == 0) {
        return false;
    }
    if (needle_len == 0) {
        return true;
    }
    while (haystack[i] != '\0') {
        size_t j = 0;

        while (haystack[i + j] != '\0' && needle[j] != '\0' &&
               haystack[i + j] == needle[j]) {
            j += 1U;
        }
        if (j == needle_len) {
            return true;
        }
        i += 1U;
    }
    return false;
}

static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == 0 || used == 0 || *used + 1U >= out_size) {
        return false;
    }
    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

static bool append_str(char* out,
                       size_t out_size,
                       size_t* used,
                       const char* value) {
    size_t i = 0;

    if (value == 0) {
        return false;
    }
    while (value[i] != '\0') {
        if (!append_char(out, out_size, used, value[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static bool ensure_demo_files(course_shell_t* shell) {
    const char* input = "stage3-cat";
    const char* script =
        "#!/bin/sh\n"
        "echo stage3-profile\n"
        "cat /proc/cpuinfo\n"
        "cat /proc/uptime\n"
        "cat /proc/meminfo\n"
        "ps\n"
        "exec hello\n"
        "exec forktest\n"
        "exec crashdemo\n"
        "cat /proc/crashlog\n"
        "cat /proc/1/status\n"
        "cat /proc/1/fd\n"
        "cat /proc/1/maps\n";

    return shell != 0 &&
           (course_fs_lookup(&shell->fs, "/demo") ||
            course_fs_mkdir(&shell->fs, "/demo")) &&
           course_fs_create(&shell->fs, "/demo/input.txt", false) &&
           course_fs_write(&shell->fs,
                           "/demo/input.txt",
                           0U,
                           input,
                           str_len(input)) &&
           course_fs_create(&shell->fs, "/demo/stage3.sh", false) &&
           course_fs_write(&shell->fs,
                           "/demo/stage3.sh",
                           0U,
                           script,
                           str_len(script));
}

static bool run_elf_libc_demo(course_os_stage3_t* stage) {
    course_user_program_t program;
    char out[1024];

    return course_user_program_stage3_count() == 5U &&
           course_user_program_lookup("hello", &program) &&
           program.elf_image != 0 &&
           course_user_program_lookup("echo", &program) &&
           course_user_program_lookup("cat", &program) &&
           course_user_program_lookup("forktest", &program) &&
           course_user_program_lookup("crashdemo", &program) &&
           course_process_exec(&stage->shell.processes,
                               stage->shell.shell_pid,
                               "hello",
                               "") == COURSE_PROCESS_OK &&
           course_shell_run_line(&stage->shell, "exec hello", out, sizeof(out)) &&
           str_contains(out, "program=hello") &&
           course_shell_run_line(&stage->shell,
                                 "exec echo stage3",
                                 out,
                                 sizeof(out)) &&
           str_contains(out, "program=echo") &&
           course_shell_run_line(&stage->shell,
                                 "exec cat /demo/input.txt",
                                 out,
                                 sizeof(out)) &&
           str_contains(out, "program=cat") &&
           str_contains(out, "stage3-cat");
}

static bool run_sched_sync_demo(course_os_stage3_t* stage) {
    course_scheduler_summary_t summary;
    course_process_t* owner = 0;
    course_process_t* waiter = 0;
    course_semaphore_t semaphore;
    course_mutex_t mutex;

    course_scheduler_init(&stage->shell.scheduler);
    if (!course_scheduler_add_task(&stage->shell.scheduler, 1U, 0U, 5U) ||
        !course_scheduler_add_task(&stage->shell.scheduler, 2U, 1U, 3U) ||
        !course_scheduler_add_task(&stage->shell.scheduler, 3U, 2U, 2U) ||
        !course_scheduler_run(&stage->shell.scheduler,
                              COURSE_SCHED_POLICY_FCFS,
                              0U) ||
        !course_scheduler_run(&stage->shell.scheduler,
                              COURSE_SCHED_POLICY_RR,
                              2U) ||
        !course_scheduler_summary(&stage->shell.scheduler, &summary) ||
        summary.preempt_count == 0U ||
        !course_scheduler_run(&stage->shell.scheduler,
                              COURSE_SCHED_POLICY_CFS_LITE,
                              2U) ||
        !course_scheduler_summary(&stage->shell.scheduler, &summary) ||
        summary.policy_runs[COURSE_SCHED_POLICY_FCFS] == 0U ||
        summary.policy_runs[COURSE_SCHED_POLICY_RR] == 0U ||
        summary.policy_runs[COURSE_SCHED_POLICY_CFS_LITE] == 0U) {
        return false;
    }

    owner = course_process_spawn(&stage->shell.processes, 0U, "sync-owner");
    waiter = course_process_spawn(&stage->shell.processes, 0U, "sync-waiter");
    if (owner == 0 || waiter == 0) {
        return false;
    }
    course_semaphore_init(&semaphore, &stage->shell.processes, 0);
    course_mutex_init(&mutex, &stage->shell.processes);
    return course_semaphore_wait(&semaphore, waiter->pid) ==
               COURSE_SYNC_BLOCKED &&
           course_semaphore_post(&semaphore) == COURSE_SYNC_OK &&
           course_mutex_lock(&mutex, owner->pid) == COURSE_SYNC_OK &&
           course_mutex_lock(&mutex, waiter->pid) == COURSE_SYNC_BLOCKED &&
           course_mutex_unlock(&mutex, waiter->pid) ==
               COURSE_SYNC_ERR_NOT_OWNER &&
           course_mutex_unlock(&mutex, owner->pid) == COURSE_SYNC_OK;
}

static bool run_vm_cow_demo(course_os_stage3_t* stage) {
    course_process_t* child = 0;
    course_process_cow_stats_t stats;
    uint8_t value = 0;

    if (!course_process_map_user_page(&stage->shell.processes,
                                      stage->shell.shell_pid,
                                      0U,
                                      (uint8_t)'S')) {
        return false;
    }
    child = course_process_fork(&stage->shell.processes,
                                stage->shell.shell_pid,
                                "stage3-cow");
    return child != 0 &&
           course_process_handle_cow_store_fault(&stage->shell.processes,
                                                 child->pid,
                                                 0U,
                                                 0U) &&
           course_process_write_user_byte(&stage->shell.processes,
                                          child->pid,
                                          0U,
                                          0U,
                                          (uint8_t)'C') &&
           course_process_read_user_byte(&stage->shell.processes,
                                         stage->shell.shell_pid,
                                         0U,
                                         0U,
                                         &value) &&
           value == (uint8_t)'S' &&
           course_process_cow_stats(&stage->shell.processes, &stats) &&
           stats.cow_faults > 0U &&
           stats.saved_pages > 0U &&
           stats.leak_free;
}

static bool run_fs_shell_proc_demo(course_os_stage3_t* stage) {
    char out[2048];

    return course_shell_run_line(&stage->shell,
                                 "sh /demo/stage3.sh",
                                 out,
                                 sizeof(out)) &&
           str_contains(out, "stage3-profile") &&
           str_contains(out, "isa=rv64im") &&
           str_contains(out, "ticks=") &&
           str_contains(out, "program=hello") &&
           str_contains(out, "program=forktest") &&
           str_contains(out, "program=crashdemo") &&
           str_contains(out, "reason=user-crash") &&
           str_contains(out, "code") &&
           str_contains(out, "stack");
}

void course_os_stage3_init(course_os_stage3_t* stage) {
    if (stage == 0) {
        return;
    }
    course_shell_init(&stage->shell);
    stage->elf_libc_ok = false;
    stage->sched_sync_ok = false;
    stage->vm_cow_ok = false;
    stage->fs_shell_ok = false;
    stage->proc_ok = false;
}

bool course_os_stage3_run(course_os_stage3_t* stage) {
    if (stage == 0 || !ensure_demo_files(&stage->shell)) {
        return false;
    }

    stage->elf_libc_ok = run_elf_libc_demo(stage);
    stage->sched_sync_ok = run_sched_sync_demo(stage);
    stage->vm_cow_ok = run_vm_cow_demo(stage);
    stage->fs_shell_ok = run_fs_shell_proc_demo(stage);
    stage->proc_ok = stage->fs_shell_ok;

    return stage->elf_libc_ok &&
           stage->sched_sync_ok &&
           stage->vm_cow_ok &&
           stage->fs_shell_ok &&
           stage->proc_ok;
}

bool course_os_stage3_summary(const course_os_stage3_t* stage,
                              char* out,
                              size_t out_size) {
    size_t used = 0;

    if (stage == 0 || out == 0 || out_size == 0 ||
        !stage->elf_libc_ok ||
        !stage->sched_sync_ok ||
        !stage->vm_cow_ok ||
        !stage->fs_shell_ok ||
        !stage->proc_ok) {
        return false;
    }
    out[0] = '\0';
    return append_str(out, out_size, &used, COURSE_OS_STAGE3_MARKER);
}

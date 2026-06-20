/* Stage3 procfs 单测：验证 cpuinfo/uptime/pid/fd/maps 等只读证据节点。 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fd.h"
#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_process.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

typedef struct TestFdResolverContext {
    uint32_t shell_pid;
    uint32_t child_pid;
    const course_fd_table_t* shell_fds;
    const course_fd_table_t* child_fds;
} test_fd_resolver_context_t;

static const course_fd_table_t* resolve_test_fd_table(const void* context,
                                                      uint32_t pid) {
    const test_fd_resolver_context_t* resolver =
        (const test_fd_resolver_context_t*)context;

    if (resolver == NULL) {
        return NULL;
    }
    if (pid == resolver->shell_pid) {
        return resolver->shell_fds;
    }
    if (pid == resolver->child_pid) {
        return resolver->child_fds;
    }
    return NULL;
}

static int test_procfs_stage3_global_and_per_pid_nodes(void) {
    static course_fs_t fs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    course_process_table_t processes;
    course_process_t* shell = NULL;
    course_process_t* child = NULL;
    procfs_t procfs;
    course_fd_table_t fds;
    course_fd_table_t child_fds;
    test_fd_resolver_context_t fd_resolver;
    char out[1024];
    int proc_fd = -1;
    int child_proc_fd = -1;

    course_fs_mkfs(&fs);
    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_process_table_init(&processes);
    shell = course_process_spawn(&processes, 0U, "shell");
    if (shell == NULL ||
        course_process_exec(&processes, shell->pid, "hello", "") !=
            COURSE_PROCESS_OK) {
        return fail("expected process with ELF maps");
    }
    child = course_process_fork(&processes, shell->pid, "child");
    if (child == NULL) {
        return fail("expected fork child with inherited maps");
    }

    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_processes(&procfs, &processes);
    course_fd_table_init(&fds, &fs, &procfs);
    procfs_attach_fd_table(&procfs, shell->pid, &fds);
    course_fd_table_init(&child_fds, &fs, &procfs);
    fd_resolver.shell_pid = shell->pid;
    fd_resolver.child_pid = child->pid;
    fd_resolver.shell_fds = &fds;
    fd_resolver.child_fds = &child_fds;
    procfs_attach_fd_table_resolver(&procfs,
                                    resolve_test_fd_table,
                                    &fd_resolver);
    proc_fd = course_fd_open(&fds, "/proc/cpuinfo", COURSE_FD_OPEN_READ);
    child_proc_fd =
        course_fd_open(&child_fds, "/proc/uptime", COURSE_FD_OPEN_READ);
    if (proc_fd < 3) {
        return fail("expected fd table to hold an open proc fd");
    }
    if (child_proc_fd < 3) {
        return fail("expected child fd table to hold an open proc fd");
    }

    if (!procfs_read(&procfs, "/proc/cpuinfo", out, sizeof(out)) ||
        !contains(out, "isa=rv64im") ||
        !contains(out, "backend=myCPU") ||
        !contains(out, "stage=kernel_alpha_stage3") ||
        !contains(out, "timer_hz=100")) {
        return fail("expected /proc/cpuinfo");
    }
    if (!procfs_read(&procfs, "/proc/uptime", out, sizeof(out)) ||
        !contains(out, "ticks=")) {
        return fail("expected /proc/uptime");
    }
    if (!procfs_read(&procfs, "/proc/1/status", out, sizeof(out)) ||
        !contains(out, "pid=1") ||
        !contains(out, "ppid=0") ||
        !contains(out, "state=ready") ||
        !contains(out, "name=hello") ||
        !contains(out, "exit_code=0")) {
        return fail("expected /proc/<pid>/status");
    }
    if (!procfs_read(&procfs, "/proc/1/fd", out, sizeof(out)) ||
        !contains(out, "fd=0 kind=stdio") ||
        !contains(out, "fd=1 kind=stdio") ||
        !contains(out, "fd=3 kind=proc path=/proc/cpuinfo")) {
        return fail("expected /proc/<pid>/fd");
    }
    if (contains(out, "path=/proc/uptime")) {
        return fail("expected /proc/1/fd not to include child proc fd");
    }
    if (!procfs_read(&procfs, "/proc/2/fd", out, sizeof(out)) ||
        !contains(out, "fd=0 kind=stdio") ||
        !contains(out, "fd=3 kind=proc path=/proc/uptime") ||
        contains(out, "path=/proc/cpuinfo")) {
        return fail("expected /proc/<pid>/fd to resolve child fd table");
    }
    if (!procfs_read(&procfs, "/proc/1/maps", out, sizeof(out)) ||
        !contains(out, "code") ||
        !contains(out, "data") ||
        !contains(out, "heap") ||
        !contains(out, "stack")) {
        return fail("expected /proc/<pid>/maps");
    }
    if (!procfs_read(&procfs, "/proc/2/maps", out, sizeof(out)) ||
        !contains(out, "code") ||
        !contains(out, "stack")) {
        return fail("expected fork child /proc/<pid>/maps");
    }
    if (procfs_read(&procfs, "/proc/999/status", out, sizeof(out)) ||
        procfs_write(&procfs, "/proc/cpuinfo", "x", 1U)) {
        return fail("expected unknown pid and proc writes to fail closed");
    }

    return 0;
}

int main(void) {
    if (test_procfs_stage3_global_and_per_pid_nodes() != 0) {
        return 1;
    }

    return 0;
}

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_elf_loader.h"
#include "../../guest/include/course_fd.h"
#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_libc.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_process.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/course_syscall.h"
#include "../../guest/include/course_user_programs.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void write_le64(uint8_t* image, size_t offset, uint64_t value) {
    size_t i = 0;

    for (i = 0; i < 8U; ++i) {
        image[offset + i] = (uint8_t)((value >> (i * 8U)) & 0xFFU);
    }
}

static int test_elf_catalog_and_loader_maps(void) {
    course_user_program_t program;
    course_elf_load_result_t load;
    char argv[] = "stage3";

    if (course_user_program_stage3_count() != 5U ||
        !course_user_program_lookup("hello", &program) ||
        program.elf_image == NULL ||
        program.elf_size == 0U) {
        return fail("expected hello to be backed by a stage3 ELF image");
    }

    if (course_elf_loader_load(program.elf_image,
                               program.elf_size,
                               argv,
                               &load) != COURSE_ELF_OK ||
        load.entry_pc != program.entry_pc ||
        load.user_sp == 0U ||
        load.argc != 1U ||
        strcmp(load.argv, "stage3") != 0 ||
        strcmp(load.envp, "PATH=/bin") != 0 ||
        load.map_count < 4U ||
        strcmp(load.maps[0].name, "code") != 0 ||
        strcmp(load.maps[1].name, "data") != 0 ||
        strcmp(load.maps[load.map_count - 2U].name, "heap") != 0 ||
        strcmp(load.maps[load.map_count - 1U].name, "stack") != 0) {
        return fail("expected ELF loader to validate image and record maps/argv");
    }

    {
        uint8_t malformed[8] = {0};

        if (course_elf_loader_load(malformed,
                                   sizeof(malformed),
                                   "",
                                   &load) != COURSE_ELF_ERR_BAD_MAGIC) {
            return fail("expected malformed ELF to fail closed");
        }
    }

    {
        uint8_t non_exec_entry[256];

        memcpy(non_exec_entry, program.elf_image, program.elf_size);
        write_le64(non_exec_entry, 24U, 0x40001000U);
        if (course_elf_loader_load(non_exec_entry,
                                   program.elf_size,
                                   "",
                                   &load) != COURSE_ELF_ERR_BAD_PROGRAM_HEADER) {
            return fail("expected ELF entry outside executable segment to fail closed");
        }
    }

    return 0;
}

static int test_process_exec_uses_elf_without_mutating_on_bad_image(void) {
    course_process_table_t table;
    course_process_t* process = NULL;

    course_process_table_init(&table);
    process = course_process_spawn(&table, 0U, "shell");
    if (process == NULL) {
        return fail("expected process spawn");
    }

    if (course_process_exec(&table, process->pid, "hello", "arg0") !=
            COURSE_PROCESS_OK ||
        strcmp(process->name, "hello") != 0 ||
        strcmp(process->argv, "arg0") != 0 ||
        process->entry_pc == 0U ||
        process->user_sp == 0U ||
        process->map_count < 4U ||
        strcmp(process->maps[0].name, "code") != 0 ||
        strcmp(process->maps[1].name, "data") != 0 ||
        strcmp(process->maps[process->map_count - 1U].name, "stack") != 0) {
        return fail("expected exec to populate ELF entry, stack and maps");
    }

    if (course_process_exec(&table, process->pid, "badelf", "x") !=
            COURSE_PROCESS_ERR_BAD_ELF ||
        strcmp(process->name, "hello") != 0 ||
        strcmp(process->argv, "arg0") != 0) {
        return fail("expected bad ELF exec to leave current process image intact");
    }

    return 0;
}

static int test_process_fork_inherits_elf_maps(void) {
    course_process_table_t table;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;

    course_process_table_init(&table);
    parent = course_process_spawn(&table, 0U, "parent");
    if (parent == NULL ||
        course_process_exec(&table, parent->pid, "hello", "") !=
            COURSE_PROCESS_OK) {
        return fail("expected parent exec before fork maps test");
    }

    child = course_process_fork(&table, parent->pid, "child");
    if (child == NULL ||
        child->map_count != parent->map_count ||
        child->map_count == 0U ||
        strcmp(child->maps[0].name, "code") != 0 ||
        strcmp(child->maps[child->map_count - 1U].name, "stack") != 0) {
        return fail("expected fork child to inherit parent ELF maps");
    }

    return 0;
}

static int test_libc_wrappers_drive_syscall_and_fd_paths(void) {
    static course_fs_t fs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    procfs_t procfs;
    course_fd_table_t fds;
    course_syscall_t syscalls;
    course_process_table_t processes;
    course_process_t* shell = NULL;
    course_libc_t libc;
    char user_memory[256];
    char* path = user_memory;
    char* read_buffer = user_memory + 64U;
    char* write_buffer = user_memory + 96U;
    char* exec_argv = user_memory + 128U;
    char* child_name = user_memory + 160U;
    int32_t* user_status = (int32_t*)(void*)(user_memory + 192U);
    int fd = -1;
    int32_t child_pid = 0;

    course_fs_mkfs(&fs);
    course_fs_mkdir(&fs, "/demo");
    course_fs_create(&fs, "/demo/input.txt", false);
    course_fs_write(&fs, "/demo/input.txt", 0U, "stage3-cat", 10U);
    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_process_table_init(&processes);
    shell = course_process_spawn(&processes, 0U, "shell");
    if (shell == NULL || shell->pid != 1U) {
        return fail("expected shell process for libc process syscalls");
    }
    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_processes(&procfs, &processes);
    course_fd_table_init(&fds, &fs, &procfs);
    course_syscall_init(&syscalls,
                        1U,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));
    course_syscall_attach_fd_table(&syscalls, &fds);
    course_syscall_attach_process_table(&syscalls, &processes);
    course_libc_init(&libc, &syscalls);

    strcpy(path, "/demo/input.txt");
    fd = course_libc_open(&libc, path, COURSE_FD_OPEN_READ);
    if (fd < 3 ||
        course_libc_read(&libc, fd, read_buffer, 6U) != 6 ||
        memcmp(read_buffer, "stage3", 6U) != 0 ||
        course_libc_seek(&libc, fd, 7U) != 0 ||
        course_libc_read(&libc, fd, read_buffer, 3U) != 3 ||
        memcmp(read_buffer, "cat", 3U) != 0 ||
        course_libc_close(&libc, fd) != 0) {
        return fail("expected libc open/read/seek/close to use syscall fd path");
    }

    strcpy(write_buffer, "hello from elf");
    if (course_libc_write(&libc, 1, write_buffer, 14U) != 14 ||
        !course_syscall_stdout_equals(&syscalls, "hello from elf")) {
        return fail("expected libc write to append to syscall stdout");
    }

    strcpy(path, "echo");
    strcpy(exec_argv, "stage3");
    strcpy(child_name, "child");
    if (course_libc_exec(&libc, path, exec_argv) != COURSE_SYSCALL_OK ||
        course_libc_fork(&libc, child_name) <= 0) {
        return fail("expected libc exec and fork wrappers");
    }
    child_pid = (int32_t)(processes.next_pid - 1U);
    if (!course_process_exit(&processes, (uint32_t)child_pid, 23) ||
        course_libc_waitpid(&libc, child_pid, user_status) != COURSE_SYSCALL_OK ||
        *user_status != 23) {
        return fail("expected libc waitpid wrapper to reap child status");
    }

    if (course_libc_exit(&libc, 7) != COURSE_SYSCALL_OK ||
        !course_syscall_exited(&syscalls) ||
        course_syscall_exit_code(&syscalls) != 7 ||
        shell->state != COURSE_PROCESS_ZOMBIE ||
        shell->exit_code != 7) {
        return fail("expected libc exit wrapper to update process state");
    }

    return 0;
}

static int test_wait_syscall_reaps_any_zombie_child(void) {
    course_process_table_t processes;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;
    course_syscall_t syscalls;
    char user_memory[64];
    int32_t* user_status = (int32_t*)(void*)user_memory;

    course_process_table_init(&processes);
    parent = course_process_spawn(&processes, 0U, "parent");
    child = parent != NULL ? course_process_fork(&processes,
                                                 parent->pid,
                                                 "child")
                           : NULL;
    if (parent == NULL || child == NULL ||
        !course_process_exit(&processes, child->pid, 31)) {
        return fail("expected zombie child for wait syscall");
    }

    course_syscall_init(&syscalls,
                        parent->pid,
                        (uintptr_t)user_memory,
                        sizeof(user_memory));
    course_syscall_attach_process_table(&syscalls, &processes);
    *user_status = 0;
    if (course_syscall_dispatch(&syscalls,
                                COURSE_SYSCALL_WAIT,
                                (uintptr_t)user_status,
                                0U,
                                0U,
                                0U) != COURSE_SYSCALL_OK ||
        *user_status != 31 ||
        child->state != COURSE_PROCESS_DEAD) {
        return fail("expected wait syscall to reap any zombie child");
    }

    return 0;
}

static int test_fixed_stage3_programs_are_available(void) {
    course_user_program_t program;

    if (!course_user_program_lookup("hello", &program) ||
        program.kind != COURSE_USER_PROGRAM_HELLO ||
        !course_user_program_lookup("echo", &program) ||
        program.kind != COURSE_USER_PROGRAM_ECHO ||
        !course_user_program_lookup("cat", &program) ||
        program.kind != COURSE_USER_PROGRAM_CAT ||
        !course_user_program_lookup("forktest", &program) ||
        program.kind != COURSE_USER_PROGRAM_FORKTEST ||
        !course_user_program_lookup("crashdemo", &program) ||
        program.kind != COURSE_USER_PROGRAM_CRASH) {
        return fail("expected fixed Stage 3 hello/echo/cat/forktest/crashdemo programs");
    }

    return 0;
}

int main(void) {
    if (test_elf_catalog_and_loader_maps() != 0 ||
        test_process_exec_uses_elf_without_mutating_on_bad_image() != 0 ||
        test_process_fork_inherits_elf_maps() != 0 ||
        test_libc_wrappers_drive_syscall_and_fd_paths() != 0 ||
        test_wait_syscall_reaps_any_zombie_child() != 0 ||
        test_fixed_stage3_programs_are_available() != 0) {
        return 1;
    }

    return 0;
}

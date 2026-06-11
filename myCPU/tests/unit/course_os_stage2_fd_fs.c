#include <stdbool.h>
#include <stdint.h>
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

static void fill_pattern(char* out, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        out[i] = (char)('A' + (i % 26U));
    }
}

static int test_fs_explicit_storage_isolates_sparse_file_bytes(void) {
    static course_fs_t left;
    static course_fs_t right;
    static course_fs_storage_t left_storage;
    static course_fs_storage_t right_storage;
    const char seed[4] = {'A', 'B', 'C', 'D'};
    const char tail[1] = {'Z'};
    char out[4] = {'?', '?', '?', '?'};

    course_fs_mkfs_with_storage(&left, &left_storage);
    course_fs_mkfs_with_storage(&right, &right_storage);

    if (!course_fs_create(&left, "/same.bin", false) ||
        !course_fs_write(&left, "/same.bin", 0U, seed, sizeof(seed))) {
        return fail("expected seed write into left fs backing");
    }
    if (!course_fs_create(&right, "/same.bin", false) ||
        !course_fs_write(&right, "/same.bin", 3U, tail, sizeof(tail)) ||
        !course_fs_read(&right, "/same.bin", 0U, out, sizeof(out))) {
        return fail("expected sparse write/read through explicit fs backing");
    }
    if (out[0] != '\0' || out[1] != '\0' || out[2] != '\0' ||
        out[3] != 'Z') {
        return fail("expected explicit fs backing to isolate sparse bytes");
    }

    if (!course_fs_read(&left, "/same.bin", 0U, out, sizeof(out)) ||
        memcmp(out, seed, sizeof(seed)) != 0) {
        return fail("expected left fs backing to retain original bytes");
    }

    return 0;
}

static int test_fs_capacity_depth_and_large_file(void) {
    static course_fs_t fs;
    static char data[COURSE_FS_MAX_DATA];
    char name[64];
    char tail[4];
    size_t i = 0;

    course_fs_mkfs(&fs);
    if (!course_fs_mkdir(&fs, "/home") ||
        !course_fs_mkdir(&fs, "/home/user") ||
        !course_fs_mkdir(&fs, "/home/user/docs")) {
        return fail("expected three-level directory setup");
    }

    for (i = 0; i < 128U; ++i) {
        snprintf(name, sizeof(name), "/home/user/docs/f%03zu", i);
        if (!course_fs_create(&fs, name, false)) {
            return fail("expected fs to support at least 128 files");
        }
    }

    fill_pattern(data, sizeof(data));
    if (!course_fs_create(&fs, "/home/user/docs/big.bin", false) ||
        !course_fs_write(&fs, "/home/user/docs/big.bin", 0U, data, sizeof(data)) ||
        !course_fs_read(&fs, "/home/user/docs/big.bin", sizeof(data) - 4U, tail, 4U) ||
        tail[0] != data[sizeof(data) - 4U] ||
        !course_fs_lookup(&fs, "/home/user/docs/f127")) {
        return fail("expected 64KB file, seek read and deep lookup to work");
    }

    return 0;
}

static int test_fd_table_file_proc_and_errors(void) {
    static course_fs_t fs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    procfs_t procfs;
    course_fd_table_t fds;
    int fd = -1;
    int proc_fd = -1;
    char data[16] = "hello";
    char exact[6] = {'?', '?', '?', '?', '?', 'Z'};
    char out[256];
    int read_count = 0;

    course_fs_mkfs(&fs);
    course_fs_mkdir(&fs, "/home");
    course_fs_mkdir(&fs, "/home/user");
    course_fs_mkdir(&fs, "/home/user/docs");
    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    course_fd_table_init(&fds, &fs, &procfs);

    if (course_fd_set_cwd(&fds, "/home/user") != COURSE_FD_OK ||
        strcmp(course_fd_cwd(&fds), "/home/user") != 0) {
        return fail("expected cwd to be tracked");
    }

    fd = course_fd_open(&fds,
                        "docs/log.txt",
                        COURSE_FD_OPEN_CREATE | COURSE_FD_OPEN_READ |
                            COURSE_FD_OPEN_WRITE);
    if (fd < 3 ||
        course_fd_write(&fds, fd, data, 5U) != 5 ||
        course_fd_seek(&fds, fd, 0U) != 0 ||
        course_fd_read(&fds, fd, exact, 5U) != 5) {
        return fail("expected relative file open/read/write/seek through fd");
    }
    if (memcmp(exact, "hello", 5U) != 0 || exact[5] != 'Z') {
        return fail("expected exact-size fd read to preserve canary byte");
    }
    if (course_fd_seek(&fds, fd, 0U) != 0 ||
        course_fd_read(&fds, fd, out, 5U) != 5) {
        return fail("expected second fd readback to work");
    }
    out[5] = '\0';
    if (strcmp(out, "hello") != 0) {
        return fail("expected fd readback to match written bytes");
    }

    if (course_fd_close(&fds, fd) != 0 ||
        course_fd_read(&fds, fd, out, 1U) != COURSE_FD_ERR_BAD_FD) {
        return fail("expected closed fd to be rejected");
    }

    proc_fd = course_fd_open(&fds, "/proc/fsstat", COURSE_FD_OPEN_READ);
    read_count = proc_fd >= 3
                     ? course_fd_read(&fds, proc_fd, out, sizeof(out) - 1U)
                     : COURSE_FD_ERR_BAD_FD;
    if (read_count > 0) {
        out[read_count] = '\0';
    }
    if (proc_fd < 3 ||
        read_count <= 0 ||
        !contains(out, "open_calls=") ||
        !contains(out, "max_files=128") ||
        course_fd_write(&fds, proc_fd, "x", 1U) !=
            COURSE_FD_ERR_PERMISSION_DENIED) {
        return fail("expected /proc to be readable and write denied through fd");
    }

    return 0;
}

int main(void) {
    if (test_fs_explicit_storage_isolates_sparse_file_bytes() != 0 ||
        test_fs_capacity_depth_and_large_file() != 0 ||
        test_fd_table_file_proc_and_errors() != 0) {
        return 1;
    }

    return 0;
}

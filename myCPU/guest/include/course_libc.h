#pragma once

#include <stddef.h>
#include <stdint.h>

#include "course_syscall.h"

typedef struct CourseLibc {
    course_syscall_t* syscalls;
} course_libc_t;

void course_libc_init(course_libc_t* libc, course_syscall_t* syscalls);
int64_t course_libc_read(course_libc_t* libc, int fd, void* out, size_t size);
int64_t course_libc_write(course_libc_t* libc,
                          int fd,
                          const void* data,
                          size_t size);
int64_t course_libc_open(course_libc_t* libc, const char* path, uint32_t flags);
int64_t course_libc_close(course_libc_t* libc, int fd);
int64_t course_libc_seek(course_libc_t* libc, int fd, size_t offset);
int64_t course_libc_exit(course_libc_t* libc, int32_t code);
int64_t course_libc_fork(course_libc_t* libc, const char* child_name);
int64_t course_libc_exec(course_libc_t* libc,
                         const char* program_name,
                         const char* argv);
int64_t course_libc_waitpid(course_libc_t* libc,
                            int32_t child_pid,
                            int32_t* out_status);

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "course_syscall.h"

/* 简化 libc 门面：把课程用户程序的函数调用转换成 course_syscall_dispatch。 */
typedef struct CourseLibc {
    course_syscall_t* syscalls;
} course_libc_t;

/* 初始化 libc 门面，绑定 syscall 分发上下文。 */
void course_libc_init(course_libc_t* libc, course_syscall_t* syscalls);
/* libc read：转发到 read syscall。 */
int64_t course_libc_read(course_libc_t* libc, int fd, void* out, size_t size);
/* libc write：转发到 write syscall。 */
int64_t course_libc_write(course_libc_t* libc,
                          int fd,
                          const void* data,
                          size_t size);
/* libc open：转发到 open syscall。 */
int64_t course_libc_open(course_libc_t* libc, const char* path, uint32_t flags);
/* libc close：转发到 close syscall。 */
int64_t course_libc_close(course_libc_t* libc, int fd);
/* libc seek：转发到 seek syscall。 */
int64_t course_libc_seek(course_libc_t* libc, int fd, size_t offset);
/* libc exit：转发到 exit syscall。 */
int64_t course_libc_exit(course_libc_t* libc, int32_t code);
/* libc fork：转发到 fork syscall。 */
int64_t course_libc_fork(course_libc_t* libc, const char* child_name);
/* libc exec：转发到 exec syscall。 */
int64_t course_libc_exec(course_libc_t* libc,
                         const char* program_name,
                         const char* argv);
/* libc waitpid：转发到 waitpid syscall。 */
int64_t course_libc_waitpid(course_libc_t* libc,
                            int32_t child_pid,
                            int32_t* out_status);

#include "course_libc.h"

void course_libc_init(course_libc_t* libc, course_syscall_t* syscalls) {
    if (libc == 0) {
        return;
    }
    libc->syscalls = syscalls;
}

int64_t course_libc_read(course_libc_t* libc, int fd, void* out, size_t size) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_READ,
                                   (uint64_t)fd,
                                   (uint64_t)(uintptr_t)out,
                                   (uint64_t)size,
                                   0U);
}

int64_t course_libc_write(course_libc_t* libc,
                          int fd,
                          const void* data,
                          size_t size) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_WRITE,
                                   (uint64_t)fd,
                                   (uint64_t)(uintptr_t)data,
                                   (uint64_t)size,
                                   0U);
}

int64_t course_libc_open(course_libc_t* libc, const char* path, uint32_t flags) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_OPEN,
                                   (uint64_t)(uintptr_t)path,
                                   flags,
                                   0U,
                                   0U);
}

int64_t course_libc_close(course_libc_t* libc, int fd) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_CLOSE,
                                   (uint64_t)fd,
                                   0U,
                                   0U,
                                   0U);
}

int64_t course_libc_seek(course_libc_t* libc, int fd, size_t offset) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_SEEK,
                                   (uint64_t)fd,
                                   (uint64_t)offset,
                                   0U,
                                   0U);
}

int64_t course_libc_exit(course_libc_t* libc, int32_t code) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_EXIT,
                                   (uint64_t)code,
                                   0U,
                                   0U,
                                   0U);
}

int64_t course_libc_fork(course_libc_t* libc, const char* child_name) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_FORK,
                                   (uint64_t)(uintptr_t)child_name,
                                   0U,
                                   0U,
                                   0U);
}

int64_t course_libc_exec(course_libc_t* libc,
                         const char* program_name,
                         const char* argv) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_EXEC,
                                   (uint64_t)(uintptr_t)program_name,
                                   (uint64_t)(uintptr_t)argv,
                                   0U,
                                   0U);
}

int64_t course_libc_waitpid(course_libc_t* libc,
                            int32_t child_pid,
                            int32_t* out_status) {
    if (libc == 0 || libc->syscalls == 0) {
        return COURSE_SYSCALL_ERR_INVALID_SYSCALL;
    }
    return course_syscall_dispatch(libc->syscalls,
                                   COURSE_SYSCALL_WAITPID,
                                   (uint64_t)child_pid,
                                   (uint64_t)(uintptr_t)out_status,
                                   0U,
                                   0U);
}

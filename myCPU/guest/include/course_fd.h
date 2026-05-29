#pragma once

#include <stddef.h>
#include <stdint.h>

#include "course_fs.h"
#include "procfs.h"

#define COURSE_FD_MAX_OPEN 16U
#define COURSE_FD_MAX_PATH 96U
#define COURSE_FD_OPEN_READ (1U << 0)
#define COURSE_FD_OPEN_WRITE (1U << 1)
#define COURSE_FD_OPEN_CREATE (1U << 2)

typedef enum CourseFdError {
    COURSE_FD_OK = 0,
    COURSE_FD_ERR_BAD_FD = -1,
    COURSE_FD_ERR_NO_SLOT = -2,
    COURSE_FD_ERR_NO_SUCH_FILE = -3,
    COURSE_FD_ERR_PERMISSION_DENIED = -4,
    COURSE_FD_ERR_INVALID_PATH = -5,
} course_fd_error_t;

typedef enum CourseFdKind {
    COURSE_FD_KIND_UNUSED = 0,
    COURSE_FD_KIND_STDIO,
    COURSE_FD_KIND_FILE,
    COURSE_FD_KIND_PROC,
} course_fd_kind_t;

typedef struct CourseFdEntry {
    course_fd_kind_t kind;
    uint32_t flags;
    size_t offset;
    char path[COURSE_FD_MAX_PATH];
} course_fd_entry_t;

typedef struct CourseFdTable {
    course_fs_t* fs;
    procfs_t* procfs;
    char cwd[COURSE_FD_MAX_PATH];
    course_fd_entry_t entries[COURSE_FD_MAX_OPEN];
} course_fd_table_t;

void course_fd_table_init(course_fd_table_t* table,
                          course_fs_t* fs,
                          procfs_t* procfs);
int course_fd_open(course_fd_table_t* table, const char* path, uint32_t flags);
int course_fd_close(course_fd_table_t* table, int fd);
int course_fd_read(course_fd_table_t* table, int fd, char* out, size_t size);
int course_fd_write(course_fd_table_t* table,
                    int fd,
                    const char* data,
                    size_t size);
int course_fd_seek(course_fd_table_t* table, int fd, size_t offset);
int course_fd_set_cwd(course_fd_table_t* table, const char* cwd);
const char* course_fd_cwd(const course_fd_table_t* table);
int course_fd_resolve_path(const course_fd_table_t* table,
                           const char* path,
                           char* out,
                           size_t out_size);

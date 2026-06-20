#pragma once

#include <stddef.h>
#include <stdint.h>

#include "course_fs.h"
#include "procfs.h"

/* 课程 FD 层把 stdio、RAMFS 文件和只读 procfs 统一成一个小型打开文件表。
   它故意保持 fixed-size，便于课程单测直接验证坏 fd、权限和 seek 边界。 */
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
    /* offset 是每个 fd 的独立读写位置，procfs 读取也遵循同一规则。 */
    size_t offset;
    char path[COURSE_FD_MAX_PATH];
} course_fd_entry_t;

typedef struct CourseFdTable {
    /* fs/procfs 是引用，不拥有生命周期；多进程测试可为不同 pid 注入不同表。 */
    course_fs_t* fs;
    procfs_t* procfs;
    char cwd[COURSE_FD_MAX_PATH];
    course_fd_entry_t entries[COURSE_FD_MAX_OPEN];
} course_fd_table_t;

/* 初始化 FD 表：注入 fs/procfs，cwd 设为 "/"，0/1/2 固定为 stdin/stdout/stderr。 */
void course_fd_table_init(course_fd_table_t* table,
                          course_fs_t* fs,
                          procfs_t* procfs);
/* 打开路径：procfs 只读、RAMFS 按需创建，返回 fd 或负错误码。 */
int course_fd_open(course_fd_table_t* table, const char* path, uint32_t flags);
/* 关闭 fd（0/1/2 拒绝），释放槽位并记 close 计数。 */
int course_fd_close(course_fd_table_t* table, int fd);
/* 从 fd 读取最多 size 字节到 out，返回实际字节数或负错误码（不含 NUL）。 */
int course_fd_read(course_fd_table_t* table, int fd, char* out, size_t size);
/* 向 fd 写入 size 字节，更新 offset，返回写入字节数或负错误码。 */
int course_fd_write(course_fd_table_t* table,
                    int fd,
                    const char* data,
                    size_t size);
/* 设置普通文件 fd 的读写 offset（procfs/stdio 拒绝）。 */
int course_fd_seek(course_fd_table_t* table, int fd, size_t offset);
/* 设置工作目录，要求解析后路径存在。 */
int course_fd_set_cwd(course_fd_table_t* table, const char* cwd);
/* 返回当前工作目录。 */
const char* course_fd_cwd(const course_fd_table_t* table);
/* 把相对路径按 cwd 解析成绝对路径写入 out（不解析 ..）。 */
int course_fd_resolve_path(const course_fd_table_t* table,
                           const char* path,
                           char* out,
                           size_t out_size);

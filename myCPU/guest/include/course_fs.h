#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 课程 OS 文件系统：固定容量 RAMFS + 简化目录索引。
   数据和目录项全部驻留在 guest 内存里，目标是覆盖课程展示的 CRUD、路径解析、
   seek/mkfs 和 B 树索引证据，不承担真实磁盘一致性。 */
#define COURSE_FS_MAX_NODES 160U
#define COURSE_FS_MAX_NAME 32U
#define COURSE_FS_MAX_DATA 65536U
#define COURSE_FS_MAX_DIR_INDEX_ENTRIES COURSE_FS_MAX_NODES
#define COURSE_FS_BTREE_LEAF_CAPACITY 4U
#define COURSE_FS_BTREE_MAX_LEAVES \
    ((COURSE_FS_MAX_DIR_INDEX_ENTRIES + COURSE_FS_BTREE_LEAF_CAPACITY - 1U) / \
     COURSE_FS_BTREE_LEAF_CAPACITY)

typedef struct CourseFsStats {
    uint32_t file_creates;
    uint32_t dir_creates;
    uint32_t file_reads;
    uint32_t file_writes;
    uint32_t file_deletes;
    uint32_t dir_deletes;
    uint32_t path_resolves;
    uint32_t dir_index_lookups;
    uint32_t btree_compare_steps;
    uint32_t btree_internal_nodes;
    uint32_t btree_leaf_nodes;
    uint32_t open_calls;
    uint32_t close_calls;
    uint32_t seek_calls;
    uint32_t max_files;
    uint32_t max_file_size;
    uint32_t max_depth;
} course_fs_stats_t;

typedef struct CourseFsNode {
    /* 一个节点同时表示普通文件或目录；目录节点使用 dir_index 保存子节点。 */
    bool used;
    bool is_dir;
    int parent;
    char name[COURSE_FS_MAX_NAME];
    size_t data_offset;
    size_t data_capacity;
    size_t size;
    /* dir_index 始终按子节点名排序，B 树叶子信息在 rebuild 后由它派生。 */
    int dir_index[COURSE_FS_MAX_DIR_INDEX_ENTRIES];
    size_t dir_index_count;
    size_t btree_leaf_starts[COURSE_FS_BTREE_MAX_LEAVES];
    size_t btree_leaf_counts[COURSE_FS_BTREE_MAX_LEAVES];
    size_t btree_leaf_count;
    size_t btree_internal_count;
} course_fs_node_t;

typedef struct CourseFsStorage {
    unsigned char data[COURSE_FS_MAX_NODES][COURSE_FS_MAX_DATA];
} course_fs_storage_t;

typedef struct CourseFs {
    course_fs_node_t nodes[COURSE_FS_MAX_NODES];
    /* storage 可注入，便于测试多个 FS 实例互不串数据；NULL 时使用默认 backing。 */
    course_fs_storage_t* storage;
    course_fs_stats_t stats;
} course_fs_t;

/* 初始化文件系统：用默认 backing storage，清空节点表与统计，建好根目录。 */
void course_fs_init(course_fs_t* fs);
/* 同上，但允许传入自定义 backing storage，便于多 FS 实例隔离测试。 */
void course_fs_init_with_storage(course_fs_t* fs, course_fs_storage_t* storage);
/* 格式化文件系统：等价于 init，重建空根目录。 */
void course_fs_mkfs(course_fs_t* fs);
/* 同上，但使用传入的 backing storage。 */
void course_fs_mkfs_with_storage(course_fs_t* fs, course_fs_storage_t* storage);
/* 新建目录；路径已存在或父目录缺失则失败。 */
bool course_fs_mkdir(course_fs_t* fs, const char* path);
/* 删除空目录；目录非空或不是目录则失败。 */
bool course_fs_rmdir(course_fs_t* fs, const char* path);
/* 新建文件或目录节点；已存在同名、父目录缺失或名字过长则失败。 */
bool course_fs_create(course_fs_t* fs, const char* path, bool directory);
/* 删除普通文件；目录或根节点拒绝。 */
bool course_fs_unlink(course_fs_t* fs, const char* path);
/* 向文件 offset 处写入 size 字节；offset 超过当前末尾时以 0 填充空洞。 */
bool course_fs_write(course_fs_t* fs,
                     const char* path,
                     size_t offset,
                     const char* data,
                     size_t size);
/* 从文件 offset 处读取 size 字节到 out；越界或读到目录则失败。 */
bool course_fs_read(course_fs_t* fs,
                    const char* path,
                    size_t offset,
                    char* out,
                    size_t size);
/* 路径是否存在（文件或目录均可）。 */
bool course_fs_lookup(course_fs_t* fs, const char* path);
/* 取普通文件的当前字节大小。 */
bool course_fs_size(course_fs_t* fs, const char* path, size_t* out_size);
/* 列出目录的直接子节点名，空格分隔并以 '\n' 结尾；装不下或非目录则失败。 */
bool course_fs_listdir(course_fs_t* fs,
                       const char* path,
                       char* out,
                       size_t out_size);
/* 拷贝出文件系统累计统计（CRUD 计数、B 树索引步数等）。 */
bool course_fs_stats(const course_fs_t* fs, course_fs_stats_t* out_stats);
/* 记一次 open 计数，供 FD 层调用。 */
void course_fs_record_open(course_fs_t* fs);
/* 记一次 close 计数，供 FD 层调用。 */
void course_fs_record_close(course_fs_t* fs);
/* 记一次 seek 计数，供 FD 层调用。 */
void course_fs_record_seek(course_fs_t* fs);

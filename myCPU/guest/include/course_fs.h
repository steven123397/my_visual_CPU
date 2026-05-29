#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    bool used;
    bool is_dir;
    int parent;
    char name[COURSE_FS_MAX_NAME];
    size_t data_offset;
    size_t data_capacity;
    size_t size;
    int dir_index[COURSE_FS_MAX_DIR_INDEX_ENTRIES];
    size_t dir_index_count;
    size_t btree_leaf_starts[COURSE_FS_BTREE_MAX_LEAVES];
    size_t btree_leaf_counts[COURSE_FS_BTREE_MAX_LEAVES];
    size_t btree_leaf_count;
    size_t btree_internal_count;
} course_fs_node_t;

typedef struct CourseFs {
    course_fs_node_t nodes[COURSE_FS_MAX_NODES];
    course_fs_stats_t stats;
} course_fs_t;

void course_fs_init(course_fs_t* fs);
void course_fs_mkfs(course_fs_t* fs);
bool course_fs_mkdir(course_fs_t* fs, const char* path);
bool course_fs_rmdir(course_fs_t* fs, const char* path);
bool course_fs_create(course_fs_t* fs, const char* path, bool directory);
bool course_fs_unlink(course_fs_t* fs, const char* path);
bool course_fs_write(course_fs_t* fs,
                     const char* path,
                     size_t offset,
                     const char* data,
                     size_t size);
bool course_fs_read(course_fs_t* fs,
                    const char* path,
                    size_t offset,
                    char* out,
                    size_t size);
bool course_fs_lookup(course_fs_t* fs, const char* path);
bool course_fs_size(course_fs_t* fs, const char* path, size_t* out_size);
bool course_fs_stats(const course_fs_t* fs, course_fs_stats_t* out_stats);
void course_fs_record_open(course_fs_t* fs);
void course_fs_record_close(course_fs_t* fs);
void course_fs_record_seek(course_fs_t* fs);

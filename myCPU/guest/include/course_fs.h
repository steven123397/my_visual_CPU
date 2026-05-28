#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_FS_MAX_NODES 16U
#define COURSE_FS_MAX_NAME 24U
#define COURSE_FS_MAX_DATA 128U
#define COURSE_FS_MAX_DIR_INDEX_ENTRIES COURSE_FS_MAX_NODES

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
} course_fs_stats_t;

typedef struct CourseFsNode {
    bool used;
    bool is_dir;
    int parent;
    char name[COURSE_FS_MAX_NAME];
    unsigned char data[COURSE_FS_MAX_DATA];
    size_t size;
    int dir_index[COURSE_FS_MAX_DIR_INDEX_ENTRIES];
    size_t dir_index_count;
} course_fs_node_t;

typedef struct CourseFs {
    course_fs_node_t nodes[COURSE_FS_MAX_NODES];
    course_fs_stats_t stats;
} course_fs_t;

void course_fs_init(course_fs_t* fs);
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
bool course_fs_stats(const course_fs_t* fs, course_fs_stats_t* out_stats);

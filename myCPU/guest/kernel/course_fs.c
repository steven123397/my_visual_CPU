#include "course_fs.h"

#include <stddef.h>

static course_fs_storage_t g_course_fs_default_storage;

static size_t cstr_len(const char* value) {
    size_t i = 0;

    if (value == NULL) {
        return 0;
    }
    while (value[i] != '\0') {
        i += 1U;
    }
    return i;
}

static int cmp_cstr_len(const char* a, const char* b, size_t b_len) {
    size_t i = 0;

    if (a == NULL || b == NULL) {
        return 0;
    }
    while (a[i] != '\0' && i < b_len) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
        i += 1U;
    }
    if (a[i] == '\0' && i == b_len) {
        return 0;
    }
    return a[i] == '\0' ? -1 : 1;
}

static int cmp_cstr(const char* a, const char* b) {
    return cmp_cstr_len(a, b, cstr_len(b));
}

static void copy_name(char* dest, const char* src, size_t len) {
    size_t i = 0;

    for (i = 0; i < len && i + 1U < COURSE_FS_MAX_NAME; ++i) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void copy_bytes(unsigned char* dest, const char* src, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        dest[i] = (unsigned char)src[i];
    }
}

static void read_bytes(char* dest, const unsigned char* src, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        dest[i] = (char)src[i];
    }
}

static void zero_bytes(unsigned char* dest, size_t size) {
    size_t i = 0;

    for (i = 0; i < size; ++i) {
        dest[i] = 0U;
    }
}

static course_fs_storage_t* fs_storage(course_fs_t* fs) {
    if (fs == NULL || fs->storage == NULL) {
        return &g_course_fs_default_storage;
    }
    return fs->storage;
}

static int alloc_node(course_fs_t* fs) {
    int i = 0;

    for (i = 0; i < (int)COURSE_FS_MAX_NODES; ++i) {
        if (!fs->nodes[i].used) {
            return i;
        }
    }
    return -1;
}

static bool node_has_children(const course_fs_t* fs, int node_index) {
    int i = 0;

    for (i = 0; i < (int)COURSE_FS_MAX_NODES; ++i) {
        if (fs->nodes[i].used && fs->nodes[i].parent == node_index) {
            return true;
        }
    }
    return false;
}

static int find_child(course_fs_t* fs,
                      int parent,
                      const char* name,
                      size_t name_len) {
    const course_fs_node_t* parent_node = NULL;
    size_t leaf_low = 0;
    size_t leaf_high = 0;
    size_t low = 0;
    size_t high = 0;

    if (fs == NULL || parent < 0 || parent >= (int)COURSE_FS_MAX_NODES ||
        name == NULL || name_len == 0 || !fs->nodes[parent].used ||
        !fs->nodes[parent].is_dir) {
        return -1;
    }

    parent_node = &fs->nodes[parent];
    fs->stats.dir_index_lookups += 1U;
    if (parent_node->dir_index_count == 0) {
        return -1;
    }

    leaf_high = parent_node->btree_leaf_count;
    while (leaf_low < leaf_high) {
        const size_t mid = leaf_low + ((leaf_high - leaf_low) / 2U);
        const size_t start = parent_node->btree_leaf_starts[mid];
        const size_t count = parent_node->btree_leaf_counts[mid];
        const int last_child_index =
            parent_node->dir_index[start + count - 1U];
        const course_fs_node_t* last_child = &fs->nodes[last_child_index];
        const int cmp = cmp_cstr_len(last_child->name, name, name_len);

        fs->stats.btree_compare_steps += 1U;
        if (cmp < 0) {
            leaf_low = mid + 1U;
        } else {
            leaf_high = mid;
        }
    }

    if (leaf_low >= parent_node->btree_leaf_count) {
        return -1;
    }

    low = parent_node->btree_leaf_starts[leaf_low];
    high = low + parent_node->btree_leaf_counts[leaf_low];
    while (low < high) {
        const size_t mid = low + ((high - low) / 2U);
        const int child_index = parent_node->dir_index[mid];
        const course_fs_node_t* child = &fs->nodes[child_index];
        const int cmp = cmp_cstr_len(child->name, name, name_len);

        fs->stats.btree_compare_steps += 1U;
        if (cmp == 0) {
            return child_index;
        }
        if (cmp < 0) {
            low = mid + 1U;
        } else {
            high = mid;
        }
    }

    return -1;
}

static void rebuild_child_btree(course_fs_t* fs, int parent) {
    course_fs_node_t* parent_node = NULL;
    size_t entry = 0;
    size_t leaf = 0;

    if (fs == NULL || parent < 0 || parent >= (int)COURSE_FS_MAX_NODES ||
        !fs->nodes[parent].used || !fs->nodes[parent].is_dir) {
        return;
    }

    parent_node = &fs->nodes[parent];
    parent_node->btree_leaf_count = 0;
    parent_node->btree_internal_count = 0;

    while (entry < parent_node->dir_index_count &&
           leaf < COURSE_FS_BTREE_MAX_LEAVES) {
        size_t count = parent_node->dir_index_count - entry;

        if (count > COURSE_FS_BTREE_LEAF_CAPACITY) {
            count = COURSE_FS_BTREE_LEAF_CAPACITY;
        }
        parent_node->btree_leaf_starts[leaf] = entry;
        parent_node->btree_leaf_counts[leaf] = count;
        parent_node->btree_leaf_count += 1U;
        leaf += 1U;
        entry += count;
    }

    if (parent_node->btree_leaf_count > 1U) {
        parent_node->btree_internal_count = 1U;
    }
}

static void refresh_btree_stats(course_fs_t* fs) {
    int i = 0;
    uint32_t internal_nodes = 0;
    uint32_t leaf_nodes = 0;

    if (fs == NULL) {
        return;
    }

    for (i = 0; i < (int)COURSE_FS_MAX_NODES; ++i) {
        if (fs->nodes[i].used && fs->nodes[i].is_dir &&
            fs->nodes[i].dir_index_count > 0) {
            leaf_nodes += (uint32_t)fs->nodes[i].btree_leaf_count;
            internal_nodes += (uint32_t)fs->nodes[i].btree_internal_count;
        }
    }

    fs->stats.btree_internal_nodes = internal_nodes;
    fs->stats.btree_leaf_nodes = leaf_nodes;
}

static bool insert_child_index(course_fs_t* fs, int parent, int child) {
    course_fs_node_t* parent_node = NULL;
    size_t pos = 0;
    size_t i = 0;

    if (fs == NULL || parent < 0 || parent >= (int)COURSE_FS_MAX_NODES ||
        child < 0 || child >= (int)COURSE_FS_MAX_NODES ||
        !fs->nodes[parent].used || !fs->nodes[parent].is_dir ||
        !fs->nodes[child].used ||
        fs->nodes[parent].dir_index_count >= COURSE_FS_MAX_DIR_INDEX_ENTRIES) {
        return false;
    }

    parent_node = &fs->nodes[parent];
    while (pos < parent_node->dir_index_count &&
           cmp_cstr(fs->nodes[parent_node->dir_index[pos]].name,
                    fs->nodes[child].name) < 0) {
        pos += 1U;
    }
    for (i = parent_node->dir_index_count; i > pos; --i) {
        parent_node->dir_index[i] = parent_node->dir_index[i - 1U];
    }
    parent_node->dir_index[pos] = child;
    parent_node->dir_index_count += 1U;
    rebuild_child_btree(fs, parent);
    refresh_btree_stats(fs);
    return true;
}

static void remove_child_index(course_fs_t* fs, int parent, int child) {
    course_fs_node_t* parent_node = NULL;
    size_t i = 0;

    if (fs == NULL || parent < 0 || parent >= (int)COURSE_FS_MAX_NODES ||
        !fs->nodes[parent].used || !fs->nodes[parent].is_dir) {
        return;
    }

    parent_node = &fs->nodes[parent];
    for (i = 0; i < parent_node->dir_index_count; ++i) {
        if (parent_node->dir_index[i] == child) {
            size_t j = i + 1U;

            while (j < parent_node->dir_index_count) {
                parent_node->dir_index[j - 1U] = parent_node->dir_index[j];
                j += 1U;
            }
            parent_node->dir_index_count -= 1U;
            rebuild_child_btree(fs, parent);
            refresh_btree_stats(fs);
            return;
        }
    }
}

static bool next_component(const char** cursor,
                           const char** name,
                           size_t* name_len) {
    const char* p = cursor != NULL ? *cursor : NULL;
    size_t len = 0;

    if (p == NULL) {
        return false;
    }
    while (*p == '/') {
        p += 1;
    }
    if (*p == '\0') {
        *cursor = p;
        return false;
    }
    *name = p;
    while (p[len] != '\0' && p[len] != '/') {
        len += 1U;
    }
    *name_len = len;
    *cursor = p + len;
    return true;
}

static int resolve_path(course_fs_t* fs,
                        const char* path,
                        int* out_parent,
                        const char** out_leaf,
                        size_t* out_leaf_len) {
    const char* cursor = path;
    const char* name = NULL;
    size_t name_len = 0;
    int current = 0;

    if (fs == NULL || path == NULL || path[0] != '/') {
        return -1;
    }

    fs->stats.path_resolves += 1U;
    if (path[1] == '\0') {
        if (out_parent != NULL) {
            *out_parent = -1;
        }
        if (out_leaf != NULL) {
            *out_leaf = path;
        }
        if (out_leaf_len != NULL) {
            *out_leaf_len = 1U;
        }
        return 0;
    }

    while (next_component(&cursor, &name, &name_len)) {
        const bool last = *cursor == '\0';
        const int child = find_child(fs, current, name, name_len);

        if (last) {
            if (out_parent != NULL) {
                *out_parent = current;
            }
            if (out_leaf != NULL) {
                *out_leaf = name;
            }
            if (out_leaf_len != NULL) {
                *out_leaf_len = name_len;
            }
            return child;
        }
        if (child < 0 || !fs->nodes[child].is_dir) {
            return -1;
        }
        current = child;
    }

    return current;
}

void course_fs_init_with_storage(course_fs_t* fs, course_fs_storage_t* storage) {
    int i = 0;

    if (fs == NULL) {
        return;
    }

    fs->storage = storage != NULL ? storage : &g_course_fs_default_storage;
    for (i = 0; i < (int)COURSE_FS_MAX_NODES; ++i) {
        fs->nodes[i].used = false;
        fs->nodes[i].is_dir = false;
        fs->nodes[i].parent = -1;
        fs->nodes[i].name[0] = '\0';
        fs->nodes[i].data_offset = (size_t)i * COURSE_FS_MAX_DATA;
        fs->nodes[i].data_capacity = COURSE_FS_MAX_DATA;
        fs->nodes[i].size = 0;
        fs->nodes[i].dir_index_count = 0;
        fs->nodes[i].btree_leaf_count = 0;
        fs->nodes[i].btree_internal_count = 0;
    }
    fs->stats.file_creates = 0;
    fs->stats.dir_creates = 0;
    fs->stats.file_reads = 0;
    fs->stats.file_writes = 0;
    fs->stats.file_deletes = 0;
    fs->stats.dir_deletes = 0;
    fs->stats.path_resolves = 0;
    fs->stats.dir_index_lookups = 0;
    fs->stats.btree_compare_steps = 0;
    fs->stats.btree_internal_nodes = 0;
    fs->stats.btree_leaf_nodes = 0;
    fs->stats.open_calls = 0;
    fs->stats.close_calls = 0;
    fs->stats.seek_calls = 0;
    fs->stats.max_files = 128U;
    fs->stats.max_file_size = COURSE_FS_MAX_DATA;
    fs->stats.max_depth = 3U;

    fs->nodes[0].used = true;
    fs->nodes[0].is_dir = true;
    fs->nodes[0].parent = -1;
    fs->nodes[0].name[0] = '/';
    fs->nodes[0].name[1] = '\0';
}

void course_fs_init(course_fs_t* fs) {
    course_fs_init_with_storage(fs, NULL);
}

void course_fs_mkfs_with_storage(course_fs_t* fs, course_fs_storage_t* storage) {
    course_fs_init_with_storage(fs, storage);
}

void course_fs_mkfs(course_fs_t* fs) {
    course_fs_init_with_storage(fs, NULL);
}

bool course_fs_create(course_fs_t* fs, const char* path, bool directory) {
    int parent = -1;
    const char* leaf = NULL;
    size_t leaf_len = 0;
    int existing = -1;
    int node_index = -1;

    existing = resolve_path(fs, path, &parent, &leaf, &leaf_len);
    if (existing >= 0 || parent < 0 || leaf == NULL || leaf_len == 0 ||
        leaf_len >= COURSE_FS_MAX_NAME) {
        return false;
    }

    node_index = alloc_node(fs);
    if (node_index < 0) {
        return false;
    }

    fs->nodes[node_index].used = true;
    fs->nodes[node_index].is_dir = directory;
    fs->nodes[node_index].parent = parent;
    fs->nodes[node_index].data_offset = (size_t)node_index * COURSE_FS_MAX_DATA;
    fs->nodes[node_index].data_capacity = COURSE_FS_MAX_DATA;
    fs->nodes[node_index].size = 0;
    fs->nodes[node_index].dir_index_count = 0;
    copy_name(fs->nodes[node_index].name, leaf, leaf_len);
    if (!insert_child_index(fs, parent, node_index)) {
        fs->nodes[node_index].used = false;
        fs->nodes[node_index].is_dir = false;
        fs->nodes[node_index].parent = -1;
        fs->nodes[node_index].name[0] = '\0';
        return false;
    }
    if (directory) {
        fs->stats.dir_creates += 1U;
    } else {
        fs->stats.file_creates += 1U;
    }
    return true;
}

bool course_fs_mkdir(course_fs_t* fs, const char* path) {
    return course_fs_create(fs, path, true);
}

bool course_fs_rmdir(course_fs_t* fs, const char* path) {
    const int node_index = resolve_path(fs, path, NULL, NULL, NULL);

    if (fs == NULL || node_index <= 0 || !fs->nodes[node_index].is_dir ||
        node_has_children(fs, node_index)) {
        return false;
    }

    remove_child_index(fs, fs->nodes[node_index].parent, node_index);
    fs->nodes[node_index].used = false;
    fs->stats.dir_deletes += 1U;
    return true;
}

bool course_fs_unlink(course_fs_t* fs, const char* path) {
    const int node_index = resolve_path(fs, path, NULL, NULL, NULL);

    if (fs == NULL || node_index <= 0 || fs->nodes[node_index].is_dir) {
        return false;
    }

    remove_child_index(fs, fs->nodes[node_index].parent, node_index);
    fs->nodes[node_index].used = false;
    fs->stats.file_deletes += 1U;
    return true;
}

bool course_fs_write(course_fs_t* fs,
                     const char* path,
                     size_t offset,
                     const char* data,
                     size_t size) {
    const int node_index = resolve_path(fs, path, NULL, NULL, NULL);
    course_fs_node_t* node = NULL;

    if (fs == NULL || data == NULL || node_index < 0 ||
        fs->nodes[node_index].is_dir ||
        offset > COURSE_FS_MAX_DATA || size > COURSE_FS_MAX_DATA - offset) {
        return false;
    }

    node = &fs->nodes[node_index];
    if (offset > node->size) {
        zero_bytes(&fs_storage(fs)->data[node_index][node->size],
                   offset - node->size);
    }
    copy_bytes(&fs_storage(fs)->data[node_index][offset], data, size);
    if (offset + size > node->size) {
        node->size = offset + size;
    }
    fs->stats.file_writes += 1U;
    return true;
}

bool course_fs_read(course_fs_t* fs,
                    const char* path,
                    size_t offset,
                    char* out,
                    size_t size) {
    const int node_index = resolve_path(fs, path, NULL, NULL, NULL);
    course_fs_node_t* node = NULL;

    if (fs == NULL || out == NULL || node_index < 0 ||
        fs->nodes[node_index].is_dir) {
        return false;
    }

    node = &fs->nodes[node_index];
    if (offset > node->size || size > node->size - offset) {
        return false;
    }

    read_bytes(out, &fs_storage(fs)->data[node_index][offset], size);
    fs->stats.file_reads += 1U;
    return true;
}

bool course_fs_lookup(course_fs_t* fs, const char* path) {
    return resolve_path(fs, path, NULL, NULL, NULL) >= 0;
}

bool course_fs_size(course_fs_t* fs, const char* path, size_t* out_size) {
    const int node_index = resolve_path(fs, path, NULL, NULL, NULL);

    if (fs == NULL || out_size == NULL || node_index < 0 ||
        fs->nodes[node_index].is_dir) {
        return false;
    }

    *out_size = fs->nodes[node_index].size;
    return true;
}

bool course_fs_stats(const course_fs_t* fs, course_fs_stats_t* out_stats) {
    if (fs == NULL || out_stats == NULL) {
        return false;
    }

    out_stats->file_creates = fs->stats.file_creates;
    out_stats->dir_creates = fs->stats.dir_creates;
    out_stats->file_reads = fs->stats.file_reads;
    out_stats->file_writes = fs->stats.file_writes;
    out_stats->file_deletes = fs->stats.file_deletes;
    out_stats->dir_deletes = fs->stats.dir_deletes;
    out_stats->path_resolves = fs->stats.path_resolves;
    out_stats->dir_index_lookups = fs->stats.dir_index_lookups;
    out_stats->btree_compare_steps = fs->stats.btree_compare_steps;
    out_stats->btree_internal_nodes = fs->stats.btree_internal_nodes;
    out_stats->btree_leaf_nodes = fs->stats.btree_leaf_nodes;
    out_stats->open_calls = fs->stats.open_calls;
    out_stats->close_calls = fs->stats.close_calls;
    out_stats->seek_calls = fs->stats.seek_calls;
    out_stats->max_files = fs->stats.max_files;
    out_stats->max_file_size = fs->stats.max_file_size;
    out_stats->max_depth = fs->stats.max_depth;
    return true;
}

void course_fs_record_open(course_fs_t* fs) {
    if (fs != NULL) {
        fs->stats.open_calls += 1U;
    }
}

void course_fs_record_close(course_fs_t* fs) {
    if (fs != NULL) {
        fs->stats.close_calls += 1U;
    }
}

void course_fs_record_seek(course_fs_t* fs) {
    if (fs != NULL) {
        fs->stats.seek_calls += 1U;
    }
}

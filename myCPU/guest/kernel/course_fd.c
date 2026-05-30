#include "course_fd.h"

static size_t str_len(const char* value) {
    size_t i = 0;

    if (value == 0) {
        return 0;
    }
    while (value[i] != '\0') {
        i += 1U;
    }
    return i;
}

static bool str_eq_prefix(const char* value, const char* prefix) {
    size_t i = 0;

    if (value == 0 || prefix == 0) {
        return false;
    }
    while (prefix[i] != '\0') {
        if (value[i] != prefix[i]) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static void copy_str(char* out, size_t out_size, const char* value) {
    size_t i = 0;

    if (out == 0 || out_size == 0) {
        return;
    }
    if (value != 0) {
        while (value[i] != '\0' && i + 1U < out_size) {
            out[i] = value[i];
            i += 1U;
        }
    }
    out[i] = '\0';
}

static int find_free_fd(course_fd_table_t* table) {
    size_t i = 3U;

    if (table == 0) {
        return COURSE_FD_ERR_BAD_FD;
    }
    for (i = 3U; i < COURSE_FD_MAX_OPEN; ++i) {
        if (table->entries[i].kind == COURSE_FD_KIND_UNUSED) {
            return (int)i;
        }
    }
    return COURSE_FD_ERR_NO_SLOT;
}

static course_fd_entry_t* fd_entry(course_fd_table_t* table, int fd) {
    if (table == 0 || fd < 0 || fd >= (int)COURSE_FD_MAX_OPEN ||
        table->entries[fd].kind == COURSE_FD_KIND_UNUSED) {
        return 0;
    }
    return &table->entries[fd];
}

static bool is_proc_path(const char* path) {
    return str_eq_prefix(path, "/proc/");
}

void course_fd_table_init(course_fd_table_t* table,
                          course_fs_t* fs,
                          procfs_t* procfs) {
    size_t i = 0;

    if (table == 0) {
        return;
    }
    table->fs = fs;
    table->procfs = procfs;
    copy_str(table->cwd, sizeof(table->cwd), "/");
    for (i = 0; i < COURSE_FD_MAX_OPEN; ++i) {
        table->entries[i].kind = COURSE_FD_KIND_UNUSED;
        table->entries[i].flags = 0;
        table->entries[i].offset = 0;
        table->entries[i].path[0] = '\0';
    }
    table->entries[0].kind = COURSE_FD_KIND_STDIO;
    table->entries[0].flags = COURSE_FD_OPEN_READ;
    table->entries[1].kind = COURSE_FD_KIND_STDIO;
    table->entries[1].flags = COURSE_FD_OPEN_WRITE;
    table->entries[2].kind = COURSE_FD_KIND_STDIO;
    table->entries[2].flags = COURSE_FD_OPEN_WRITE;
}

int course_fd_resolve_path(const course_fd_table_t* table,
                           const char* path,
                           char* out,
                           size_t out_size) {
    size_t used = 0;
    size_t i = 0;

    if (table == 0 || path == 0 || out == 0 || out_size == 0) {
        return COURSE_FD_ERR_INVALID_PATH;
    }
    if (path[0] == '/') {
        copy_str(out, out_size, path);
        return COURSE_FD_OK;
    }

    copy_str(out, out_size, table->cwd);
    used = str_len(out);
    if (used == 0 || out[0] != '/') {
        return COURSE_FD_ERR_INVALID_PATH;
    }
    if (used > 1U && used + 1U < out_size) {
        out[used] = '/';
        used += 1U;
        out[used] = '\0';
    }
    while (path[i] != '\0' && used + 1U < out_size) {
        out[used] = path[i];
        used += 1U;
        i += 1U;
    }
    out[used] = '\0';
    return path[i] == '\0' ? COURSE_FD_OK : COURSE_FD_ERR_INVALID_PATH;
}

int course_fd_open(course_fd_table_t* table, const char* path, uint32_t flags) {
    int fd = 0;
    char resolved[COURSE_FD_MAX_PATH];

    if (course_fd_resolve_path(table, path, resolved, sizeof(resolved)) !=
        COURSE_FD_OK) {
        return COURSE_FD_ERR_INVALID_PATH;
    }

    fd = find_free_fd(table);
    if (fd < 0) {
        return fd;
    }

    if (is_proc_path(resolved)) {
        if ((flags & COURSE_FD_OPEN_WRITE) != 0) {
            return COURSE_FD_ERR_PERMISSION_DENIED;
        }
        table->entries[fd].kind = COURSE_FD_KIND_PROC;
        table->entries[fd].flags = COURSE_FD_OPEN_READ;
        table->entries[fd].offset = 0;
        copy_str(table->entries[fd].path, sizeof(table->entries[fd].path), resolved);
        if (table->fs != 0) {
            course_fs_record_open(table->fs);
        }
        return fd;
    }

    if (!course_fs_lookup(table->fs, resolved)) {
        if ((flags & COURSE_FD_OPEN_CREATE) == 0 ||
            !course_fs_create(table->fs, resolved, false)) {
            return COURSE_FD_ERR_NO_SUCH_FILE;
        }
    }

    table->entries[fd].kind = COURSE_FD_KIND_FILE;
    table->entries[fd].flags = flags;
    table->entries[fd].offset = 0;
    copy_str(table->entries[fd].path, sizeof(table->entries[fd].path), resolved);
    course_fs_record_open(table->fs);
    return fd;
}

int course_fd_close(course_fd_table_t* table, int fd) {
    course_fd_entry_t* entry = fd_entry(table, fd);

    if (entry == 0 || fd < 3) {
        return COURSE_FD_ERR_BAD_FD;
    }
    entry->kind = COURSE_FD_KIND_UNUSED;
    entry->flags = 0;
    entry->offset = 0;
    entry->path[0] = '\0';
    course_fs_record_close(table->fs);
    return COURSE_FD_OK;
}

int course_fd_read(course_fd_table_t* table, int fd, char* out, size_t size) {
    course_fd_entry_t* entry = fd_entry(table, fd);
    char proc_out[512];
    size_t available = 0;
    size_t i = 0;

    if (entry == 0 || out == 0) {
        return COURSE_FD_ERR_BAD_FD;
    }
    if ((entry->flags & COURSE_FD_OPEN_READ) == 0) {
        return COURSE_FD_ERR_PERMISSION_DENIED;
    }
    if (entry->kind == COURSE_FD_KIND_FILE) {
        size_t file_size = 0;

        if (!course_fs_size(table->fs, entry->path, &file_size)) {
            return COURSE_FD_ERR_NO_SUCH_FILE;
        }
        if (entry->offset >= file_size) {
            return 0;
        }
        if (size > file_size - entry->offset) {
            size = file_size - entry->offset;
        }
        if (!course_fs_read(table->fs, entry->path, entry->offset, out, size)) {
            return COURSE_FD_ERR_NO_SUCH_FILE;
        }
        out[size] = '\0';
        entry->offset += size;
        return (int)size;
    }
    if (entry->kind == COURSE_FD_KIND_PROC) {
        if (table->procfs == 0 ||
            !procfs_read(table->procfs, entry->path, proc_out, sizeof(proc_out))) {
            return COURSE_FD_ERR_NO_SUCH_FILE;
        }
        available = str_len(proc_out);
        if (entry->offset > available) {
            return 0;
        }
        available -= entry->offset;
        if (size > available) {
            size = available;
        }
        for (i = 0; i < size; ++i) {
            out[i] = proc_out[entry->offset + i];
        }
        if (i < COURSE_FD_MAX_PATH || size == 0) {
            out[i] = '\0';
        }
        entry->offset += size;
        return (int)size;
    }
    return COURSE_FD_ERR_BAD_FD;
}

int course_fd_write(course_fd_table_t* table,
                    int fd,
                    const char* data,
                    size_t size) {
    course_fd_entry_t* entry = fd_entry(table, fd);

    if (entry == 0 || data == 0) {
        return COURSE_FD_ERR_BAD_FD;
    }
    if ((entry->flags & COURSE_FD_OPEN_WRITE) == 0 ||
        entry->kind == COURSE_FD_KIND_PROC) {
        return COURSE_FD_ERR_PERMISSION_DENIED;
    }
    if (entry->kind != COURSE_FD_KIND_FILE ||
        !course_fs_write(table->fs, entry->path, entry->offset, data, size)) {
        return COURSE_FD_ERR_NO_SUCH_FILE;
    }
    entry->offset += size;
    return (int)size;
}

int course_fd_seek(course_fd_table_t* table, int fd, size_t offset) {
    course_fd_entry_t* entry = fd_entry(table, fd);

    if (entry == 0) {
        return COURSE_FD_ERR_BAD_FD;
    }
    if (entry->kind != COURSE_FD_KIND_FILE) {
        return COURSE_FD_ERR_PERMISSION_DENIED;
    }
    entry->offset = offset;
    if (table->fs != 0) {
        course_fs_record_seek(table->fs);
    }
    return COURSE_FD_OK;
}

int course_fd_set_cwd(course_fd_table_t* table, const char* cwd) {
    char resolved[COURSE_FD_MAX_PATH];

    if (course_fd_resolve_path(table, cwd, resolved, sizeof(resolved)) !=
        COURSE_FD_OK ||
        !course_fs_lookup(table->fs, resolved)) {
        return COURSE_FD_ERR_INVALID_PATH;
    }
    copy_str(table->cwd, sizeof(table->cwd), resolved);
    return COURSE_FD_OK;
}

const char* course_fd_cwd(const course_fd_table_t* table) {
    return table != 0 ? table->cwd : "";
}

/* Linux compat 旁路进程表：记录当前 pid/cwd/exec path 和 wait/clone 最小状态。 */
#include "linux_compat_process.h"

/* 取 C 字符串长度。 */
static size_t process_str_len(const char* value) {
    size_t length = 0;

    if (value == 0) {
        return 0;
    }
    while (value[length] != '\0') {
        length += 1U;
    }
    return length;
}

/* 安全拷贝字符串到定长缓冲并补 NUL。 */
static void process_copy_str(char* out, size_t out_size, const char* value) {
    size_t i = 0;

    if (out == 0 || out_size == 0U) {
        return;
    }
    if (value == 0) {
        value = "";
    }
    while (i + 1U < out_size && value[i] != '\0') {
        out[i] = value[i];
        i += 1U;
    }
    out[i] = '\0';
}

/* 校验 cwd 是否合法（非空且不太长）。 */
static bool valid_cwd(const char* cwd) {
    return cwd != 0 && cwd[0] == '/' &&
           process_str_len(cwd) < LINUX_COMPAT_MAX_PATH;
}

/* 把进程槽位复位为未使用。 */
static void clear_process(linux_compat_process_t* process) {
    if (process == 0) {
        return;
    }
    process->used = false;
    process->pid = 0;
    process->ppid = 0;
    process->exited = false;
    process->exit_code = 0;
    process->path[0] = '\0';
    process->cwd[0] = '\0';
}

void linux_compat_process_table_init(linux_compat_process_table_t* table,
                                     const char* cwd) {
    size_t i = 0;
    const char* initial_cwd = valid_cwd(cwd) ? cwd : "/";

    if (table == 0) {
        return;
    }
    table->current_pid = 1U;
    table->next_pid = 2U;
    table->clone_count = 0;
    table->last_clone_flags = 0;
    table->last_clone_stack = 0;
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        clear_process(&table->processes[i]);
    }
    table->processes[0].used = true;
    table->processes[0].pid = table->current_pid;
    table->processes[0].ppid = 0;
    process_copy_str(table->processes[0].cwd,
                     sizeof(table->processes[0].cwd),
                     initial_cwd);
}

linux_compat_process_t* linux_compat_process_table_find_mut(
    linux_compat_process_table_t* table,
    uint32_t pid) {
    size_t i = 0;

    if (table == 0 || pid == 0U) {
        return 0;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        if (table->processes[i].used && table->processes[i].pid == pid) {
            return &table->processes[i];
        }
    }
    return 0;
}

const linux_compat_process_t* linux_compat_process_table_find(
    const linux_compat_process_table_t* table,
    uint32_t pid) {
    size_t i = 0;

    if (table == 0 || pid == 0U) {
        return 0;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        if (table->processes[i].used && table->processes[i].pid == pid) {
            return &table->processes[i];
        }
    }
    return 0;
}

bool linux_compat_process_table_set_current_cwd(
    linux_compat_process_table_t* table,
    const char* cwd) {
    linux_compat_process_t* current = 0;

    if (table == 0 || !valid_cwd(cwd)) {
        return false;
    }
    current = linux_compat_process_table_find_mut(table, table->current_pid);
    if (current == 0) {
        return false;
    }
    process_copy_str(current->cwd, sizeof(current->cwd), cwd);
    return true;
}

bool linux_compat_process_table_set_current_exec_path(
    linux_compat_process_table_t* table,
    const char* path) {
    linux_compat_process_t* current = 0;

    if (table == 0 || path == 0 ||
        process_str_len(path) >= LINUX_COMPAT_MAX_PATH) {
        return false;
    }
    current = linux_compat_process_table_find_mut(table, table->current_pid);
    if (current == 0) {
        return false;
    }
    process_copy_str(current->path, sizeof(current->path), path);
    return true;
}

linux_compat_process_t* linux_compat_process_table_spawn_helper(
    linux_compat_process_table_t* table,
    uint32_t ppid,
    const char* path,
    const char* cwd,
    int32_t exit_code) {
    size_t i = 0;
    linux_compat_process_t* child = 0;
    const char* child_path = path != 0 && path[0] != '\0'
                                 ? path
                                 : "linux-compat-child";
    const char* child_cwd = valid_cwd(cwd) ? cwd : "/";

    if (table == 0 || table->next_pid == 0U || ppid == 0U) {
        return 0;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        if (!table->processes[i].used) {
            child = &table->processes[i];
            break;
        }
    }
    if (child == 0) {
        return 0;
    }
    child->used = true;
    child->pid = table->next_pid;
    table->next_pid += 1U;
    table->clone_count += 1U;
    child->ppid = ppid;
    child->exited = true;
    child->exit_code = exit_code;
    process_copy_str(child->path, sizeof(child->path), child_path);
    process_copy_str(child->cwd, sizeof(child->cwd), child_cwd);
    return child;
}

bool linux_compat_process_table_mark_exited(
    linux_compat_process_table_t* table,
    uint32_t pid,
    int32_t exit_code,
    uint32_t reaper_pid) {
    size_t i = 0;
    linux_compat_process_t* process = 0;

    if (table == 0 || pid == 0U || reaper_pid == 0U) {
        return false;
    }
    process = linux_compat_process_table_find_mut(table, pid);
    if (process == 0) {
        return false;
    }
    process->exited = true;
    process->exit_code = exit_code;
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        if (table->processes[i].used && table->processes[i].ppid == pid) {
            table->processes[i].ppid = reaper_pid;
        }
    }
    return true;
}

int64_t linux_compat_process_table_wait_exited(
    linux_compat_process_table_t* table,
    uint32_t parent_pid,
    int32_t pid,
    int32_t* out_status) {
    size_t i = 0;

    if (table == 0 || parent_pid == 0U) {
        return -22;
    }
    for (i = 0; i < LINUX_COMPAT_MAX_PROCESSES; ++i) {
        linux_compat_process_t* child = &table->processes[i];

        if (!child->used || child->ppid != parent_pid || !child->exited ||
            (pid > 0 && child->pid != (uint32_t)pid)) {
            continue;
        }
        if (out_status != 0) {
            *out_status = child->exit_code << 8;
        }
        pid = (int32_t)child->pid;
        clear_process(child);
        return pid;
    }
    return -10;
}

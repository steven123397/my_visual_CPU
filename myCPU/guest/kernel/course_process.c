#include "course_process.h"

#include "course_user_programs.h"

static void clear_user_page_ref(course_process_user_page_ref_t* ref) {
    if (ref == 0) {
        return;
    }

    ref->mapped = false;
    ref->cow_page_id = 0;
    ref->writable = false;
    ref->cow = false;
}

static void clear_process(course_process_t* process) {
    size_t i = 0;
    size_t j = 0;

    if (process == 0) {
        return;
    }

    process->used = false;
    process->pid = 0;
    process->ppid = 0;
    process->state = COURSE_PROCESS_UNUSED;
    process->exit_code = 0;
    process->crash_sepc = 0;
    process->crash_scause = 0;
    process->crash_stval = 0;
    process->address_space = 0;
    process->open_files = 0;
    process->entry_pc = 0;
    process->user_sp = 0;
    process->map_count = 0;
    for (i = 0; i < COURSE_PROCESS_MAX_MAPS; ++i) {
        process->maps[i].start = 0;
        process->maps[i].end = 0;
        process->maps[i].flags = 0;
        process->maps[i].cow = false;
        for (j = 0; j < COURSE_ELF_MAP_NAME_MAX; ++j) {
            process->maps[i].name[j] = '\0';
        }
    }
    for (i = 0; i < COURSE_PROCESS_MAX_USER_PAGES; ++i) {
        clear_user_page_ref(&process->user_pages[i]);
    }
    for (i = 0; i < COURSE_PROCESS_MAX_NAME; ++i) {
        process->name[i] = '\0';
        process->crash_reason[i] = '\0';
    }
    for (i = 0; i < COURSE_PROCESS_MAX_ARGV; ++i) {
        process->argv[i] = '\0';
    }
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

static void clear_cow_page(course_process_cow_page_t* page) {
    size_t i = 0;

    if (page == 0) {
        return;
    }

    page->used = false;
    page->id = 0;
    page->refcount = 0;
    for (i = 0; i < COURSE_PROCESS_USER_PAGE_SIZE; ++i) {
        page->data[i] = 0;
    }
}

static course_process_t* alloc_process(course_process_table_t* table) {
    size_t i = 0;

    if (table == 0) {
        return 0;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        if (!table->processes[i].used ||
            table->processes[i].state == COURSE_PROCESS_DEAD) {
            return &table->processes[i];
        }
    }
    return 0;
}

static course_process_cow_page_t* find_cow_page_by_id(
    course_process_table_t* table,
    uint32_t id) {
    size_t i = 0;

    if (table == 0 || id == 0) {
        return 0;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_COW_PAGES; ++i) {
        if (table->cow_pages[i].used && table->cow_pages[i].id == id) {
            return &table->cow_pages[i];
        }
    }
    return 0;
}

static const course_process_cow_page_t* find_const_cow_page_by_id(
    const course_process_table_t* table,
    uint32_t id) {
    size_t i = 0;

    if (table == 0 || id == 0) {
        return 0;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_COW_PAGES; ++i) {
        if (table->cow_pages[i].used && table->cow_pages[i].id == id) {
            return &table->cow_pages[i];
        }
    }
    return 0;
}

static course_process_cow_page_t* alloc_cow_page(course_process_table_t* table) {
    size_t i = 0;
    course_process_cow_page_t* page = 0;

    if (table == 0 || table->next_cow_page_id == 0) {
        return 0;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_COW_PAGES; ++i) {
        if (!table->cow_pages[i].used) {
            page = &table->cow_pages[i];
            clear_cow_page(page);
            page->used = true;
            page->id = table->next_cow_page_id;
            table->next_cow_page_id += 1U;
            page->refcount = 1U;
            if (table->cow_stats.refcount_peak < page->refcount) {
                table->cow_stats.refcount_peak = page->refcount;
            }
            return page;
        }
    }
    return 0;
}

static void release_cow_page_ref(course_process_table_t* table, uint32_t id) {
    course_process_cow_page_t* page = find_cow_page_by_id(table, id);

    if (page == 0 || page->refcount == 0) {
        return;
    }

    page->refcount -= 1U;
    if (page->refcount == 0) {
        table->cow_stats.released_pages += 1U;
        clear_cow_page(page);
    }
}

static void release_process_user_pages(course_process_table_t* table,
                                       course_process_t* process) {
    size_t i = 0;

    if (table == 0 || process == 0) {
        return;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_USER_PAGES; ++i) {
        if (process->user_pages[i].mapped) {
            release_cow_page_ref(table, process->user_pages[i].cow_page_id);
            clear_user_page_ref(&process->user_pages[i]);
        }
    }
}

static bool duplicate_cow_page(course_process_table_t* table,
                               course_process_t* process,
                               uint32_t page_index) {
    course_process_user_page_ref_t* ref = 0;
    course_process_cow_page_t* old_page = 0;
    course_process_cow_page_t* new_page = 0;
    size_t i = 0;

    if (table == 0 || process == 0 ||
        page_index >= COURSE_PROCESS_MAX_USER_PAGES) {
        return false;
    }

    ref = &process->user_pages[page_index];
    old_page = find_cow_page_by_id(table, ref->cow_page_id);
    if (old_page == 0 || old_page->refcount == 0) {
        return false;
    }

    if (!ref->cow && ref->writable) {
        return true;
    }
    if (old_page->refcount == 1U) {
        ref->cow = false;
        ref->writable = true;
        return true;
    }

    new_page = alloc_cow_page(table);
    if (new_page == 0) {
        return false;
    }
    for (i = 0; i < COURSE_PROCESS_USER_PAGE_SIZE; ++i) {
        new_page->data[i] = old_page->data[i];
    }
    old_page->refcount -= 1U;
    ref->cow_page_id = new_page->id;
    ref->cow = false;
    ref->writable = true;
    table->cow_stats.cow_faults += 1U;
    table->cow_stats.copied_pages += 1U;
    return true;
}

void course_process_table_init(course_process_table_t* table) {
    size_t i = 0;

    if (table == 0) {
        return;
    }
    table->next_pid = 1U;
    table->next_cow_page_id = 1U;
    table->cow_stats.mapped_pages = 0;
    table->cow_stats.shared_pages = 0;
    table->cow_stats.cow_faults = 0;
    table->cow_stats.copied_pages = 0;
    table->cow_stats.saved_pages = 0;
    table->cow_stats.refcount_peak = 0;
    table->cow_stats.released_pages = 0;
    table->cow_stats.leak_free = true;
    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        clear_process(&table->processes[i]);
    }
    for (i = 0; i < COURSE_PROCESS_MAX_COW_PAGES; ++i) {
        clear_cow_page(&table->cow_pages[i]);
    }
}

course_process_t* course_process_spawn(course_process_table_t* table,
                                       uint32_t ppid,
                                       const char* name) {
    course_process_t* process = alloc_process(table);

    if (table == 0 || process == 0 || table->next_pid == 0) {
        return 0;
    }

    clear_process(process);
    process->used = true;
    process->pid = table->next_pid;
    table->next_pid += 1U;
    process->ppid = ppid;
    process->state = COURSE_PROCESS_READY;
    copy_str(process->name, sizeof(process->name), name);
    return process;
}

course_process_t* course_process_fork(course_process_table_t* table,
                                      uint32_t parent_pid,
                                      const char* child_name) {
    course_process_t* parent = course_process_find(table, parent_pid);
    course_process_t* child = 0;

    if (parent == 0 || parent->state == COURSE_PROCESS_DEAD ||
        parent->state == COURSE_PROCESS_ZOMBIE) {
        return 0;
    }

    child = course_process_spawn(table,
                                 parent_pid,
                                 child_name != 0 ? child_name : parent->name);
    if (child == 0) {
        return 0;
    }
    child->entry_pc = parent->entry_pc;
    child->user_sp = parent->user_sp;
    child->address_space = parent->address_space;
    child->open_files = parent->open_files;
    child->map_count = parent->map_count;
    {
        size_t i = 0;

        for (i = 0; i < parent->map_count && i < COURSE_PROCESS_MAX_MAPS; ++i) {
            child->maps[i] = parent->maps[i];
        }
    }
    {
        size_t i = 0;

        for (i = 0; i < COURSE_PROCESS_MAX_USER_PAGES; ++i) {
            course_process_user_page_ref_t* parent_ref = &parent->user_pages[i];
            course_process_user_page_ref_t* child_ref = &child->user_pages[i];
            course_process_cow_page_t* page = 0;

            if (!parent_ref->mapped) {
                continue;
            }

            page = find_cow_page_by_id(table, parent_ref->cow_page_id);
            if (page == 0) {
                clear_process(child);
                return 0;
            }

            page->refcount += 1U;
            if (table->cow_stats.refcount_peak < page->refcount) {
                table->cow_stats.refcount_peak = page->refcount;
            }
            parent_ref->writable = false;
            parent_ref->cow = true;
            *child_ref = *parent_ref;
            child_ref->writable = false;
            child_ref->cow = true;
            table->cow_stats.mapped_pages += 1U;
            table->cow_stats.saved_pages += 1U;
        }
    }
    copy_str(child->argv, sizeof(child->argv), parent->argv);
    return child;
}

bool course_process_set_state(course_process_table_t* table,
                              uint32_t pid,
                              course_process_state_t state) {
    course_process_t* process = course_process_find(table, pid);

    if (process == 0 || state == COURSE_PROCESS_UNUSED) {
        return false;
    }

    process->state = state;
    return true;
}

bool course_process_exit(course_process_table_t* table,
                         uint32_t pid,
                         int32_t exit_code) {
    course_process_t* process = course_process_find(table, pid);

    if (process == 0 || process->state == COURSE_PROCESS_DEAD) {
        return false;
    }

    process->exit_code = exit_code;
    process->state = COURSE_PROCESS_ZOMBIE;
    return true;
}

int32_t course_process_waitpid(course_process_table_t* table,
                               uint32_t parent_pid,
                               uint32_t child_pid,
                               int32_t* out_status) {
    course_process_t* child = course_process_find(table, child_pid);

    if (child == 0 || child->ppid != parent_pid ||
        child->state == COURSE_PROCESS_DEAD) {
        return COURSE_PROCESS_ERR_NO_CHILD;
    }
    if (child->state != COURSE_PROCESS_ZOMBIE) {
        return COURSE_PROCESS_ERR_NO_CHILD;
    }

    if (out_status != 0) {
        *out_status = child->exit_code;
    }
    release_process_user_pages(table, child);
    child->state = COURSE_PROCESS_DEAD;
    return COURSE_PROCESS_OK;
}

int32_t course_process_wait(course_process_table_t* table,
                            uint32_t parent_pid,
                            int32_t* out_status) {
    size_t i = 0;

    if (table == 0) {
        return COURSE_PROCESS_ERR_NO_CHILD;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        course_process_t* child = &table->processes[i];

        if (child->used && child->ppid == parent_pid &&
            child->state == COURSE_PROCESS_ZOMBIE) {
            return course_process_waitpid(table,
                                          parent_pid,
                                          child->pid,
                                          out_status);
        }
    }
    return COURSE_PROCESS_ERR_NO_CHILD;
}

int32_t course_process_exec(course_process_table_t* table,
                            uint32_t pid,
                            const char* program_name,
                            const char* argv) {
    course_process_t* process = course_process_find(table, pid);
    course_user_program_t program;
    course_elf_load_result_t load;
    size_t i = 0;

    if (process == 0) {
        return COURSE_PROCESS_ERR_NO_PROCESS;
    }
    if (!course_user_program_lookup(program_name, &program)) {
        return COURSE_PROCESS_ERR_NO_SUCH_PROGRAM;
    }
    if (course_elf_loader_load(program.elf_image,
                               program.elf_size,
                               argv,
                               &load) != COURSE_ELF_OK) {
        return COURSE_PROCESS_ERR_BAD_ELF;
    }

    release_process_user_pages(table, process);
    copy_str(process->name, sizeof(process->name), program.name);
    copy_str(process->argv, sizeof(process->argv), load.argv);
    process->entry_pc = load.entry_pc;
    process->user_sp = load.user_sp;
    process->map_count = load.map_count;
    for (i = 0; i < COURSE_PROCESS_MAX_MAPS; ++i) {
        process->maps[i].start = 0;
        process->maps[i].end = 0;
        process->maps[i].flags = 0;
        process->maps[i].cow = false;
        process->maps[i].name[0] = '\0';
    }
    for (i = 0; i < load.map_count && i < COURSE_PROCESS_MAX_MAPS; ++i) {
        process->maps[i] = load.maps[i];
    }
    process->state = COURSE_PROCESS_READY;
    return COURSE_PROCESS_OK;
}

bool course_process_record_crash(course_process_table_t* table,
                                 uint32_t pid,
                                 uintptr_t sepc,
                                 uint64_t scause,
                                 uintptr_t stval,
                                 const char* reason) {
    course_process_t* process = course_process_find(table, pid);

    if (process == 0 || process->state == COURSE_PROCESS_DEAD) {
        return false;
    }

    process->crash_sepc = sepc;
    process->crash_scause = scause;
    process->crash_stval = stval;
    copy_str(process->crash_reason, sizeof(process->crash_reason), reason);
    return course_process_exit(table, pid, COURSE_PROCESS_EXIT_CRASH);
}

bool course_process_map_user_page(course_process_table_t* table,
                                  uint32_t pid,
                                  uint32_t page_index,
                                  uint8_t fill_value) {
    course_process_t* process = course_process_find(table, pid);
    course_process_cow_page_t* page = 0;
    size_t i = 0;

    if (process == 0 || page_index >= COURSE_PROCESS_MAX_USER_PAGES ||
        process->user_pages[page_index].mapped) {
        return false;
    }

    page = alloc_cow_page(table);
    if (page == 0) {
        return false;
    }
    for (i = 0; i < COURSE_PROCESS_USER_PAGE_SIZE; ++i) {
        page->data[i] = fill_value;
    }
    process->user_pages[page_index].mapped = true;
    process->user_pages[page_index].cow_page_id = page->id;
    process->user_pages[page_index].writable = true;
    process->user_pages[page_index].cow = false;
    table->cow_stats.mapped_pages += 1U;
    return true;
}

bool course_process_read_user_byte(course_process_table_t* table,
                                   uint32_t pid,
                                   uint32_t page_index,
                                   size_t offset,
                                   uint8_t* out_value) {
    course_process_t* process = course_process_find(table, pid);
    const course_process_cow_page_t* page = 0;

    if (process == 0 || out_value == 0 ||
        page_index >= COURSE_PROCESS_MAX_USER_PAGES ||
        offset >= COURSE_PROCESS_USER_PAGE_SIZE ||
        !process->user_pages[page_index].mapped) {
        return false;
    }

    page = find_const_cow_page_by_id(table,
                                     process->user_pages[page_index].cow_page_id);
    if (page == 0) {
        return false;
    }

    *out_value = page->data[offset];
    return true;
}

bool course_process_write_user_byte(course_process_table_t* table,
                                    uint32_t pid,
                                    uint32_t page_index,
                                    size_t offset,
                                    uint8_t value) {
    course_process_t* process = course_process_find(table, pid);
    course_process_cow_page_t* page = 0;

    if (process == 0 ||
        page_index >= COURSE_PROCESS_MAX_USER_PAGES ||
        offset >= COURSE_PROCESS_USER_PAGE_SIZE ||
        !process->user_pages[page_index].mapped ||
        !duplicate_cow_page(table, process, page_index)) {
        return false;
    }

    page = find_cow_page_by_id(table,
                               process->user_pages[page_index].cow_page_id);
    if (page == 0) {
        return false;
    }

    page->data[offset] = value;
    return true;
}

bool course_process_handle_cow_store_fault(course_process_table_t* table,
                                           uint32_t pid,
                                           uint32_t page_index,
                                           size_t offset) {
    course_process_t* process = course_process_find(table, pid);

    if (process == 0 ||
        page_index >= COURSE_PROCESS_MAX_USER_PAGES ||
        offset >= COURSE_PROCESS_USER_PAGE_SIZE ||
        !process->user_pages[page_index].mapped) {
        return false;
    }
    return duplicate_cow_page(table, process, page_index);
}

bool course_process_page_refcount(const course_process_table_t* table,
                                  uint32_t pid,
                                  uint32_t page_index,
                                  uint32_t* out_refcount) {
    const course_process_t* process = 0;
    const course_process_cow_page_t* page = 0;
    size_t i = 0;

    if (table == 0 || out_refcount == 0 ||
        page_index >= COURSE_PROCESS_MAX_USER_PAGES) {
        return false;
    }
    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        const course_process_t* candidate = &table->processes[i];

        if (candidate->used && candidate->pid == pid) {
            process = candidate;
            break;
        }
    }
    if (process == 0 || !process->user_pages[page_index].mapped) {
        return false;
    }
    page = find_const_cow_page_by_id(table,
                                     process->user_pages[page_index].cow_page_id);
    if (page == 0) {
        return false;
    }
    *out_refcount = page->refcount;
    return true;
}

bool course_process_cow_stats(const course_process_table_t* table,
                              course_process_cow_stats_t* out_stats) {
    size_t i = 0;
    uint32_t shared = 0;
    bool leak_free = true;

    if (table == 0 || out_stats == 0) {
        return false;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_COW_PAGES; ++i) {
        if (table->cow_pages[i].used && table->cow_pages[i].refcount > 1U) {
            shared += 1U;
        }
        if (table->cow_pages[i].used && table->cow_pages[i].refcount == 0U) {
            leak_free = false;
        }
    }

    out_stats->mapped_pages = table->cow_stats.mapped_pages;
    out_stats->shared_pages = shared;
    out_stats->cow_faults = table->cow_stats.cow_faults;
    out_stats->copied_pages = table->cow_stats.copied_pages;
    out_stats->saved_pages = table->cow_stats.saved_pages;
    out_stats->refcount_peak = table->cow_stats.refcount_peak;
    out_stats->released_pages = table->cow_stats.released_pages;
    out_stats->leak_free = leak_free;
    return true;
}

course_process_t* course_process_find(course_process_table_t* table,
                                      uint32_t pid) {
    size_t i = 0;

    if (table == 0 || pid == 0) {
        return 0;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        if (table->processes[i].used && table->processes[i].pid == pid) {
            return &table->processes[i];
        }
    }
    return 0;
}

const course_process_t* course_process_at(const course_process_table_t* table,
                                          size_t index) {
    if (table == 0 || index >= COURSE_PROCESS_MAX_PROCESSES ||
        !table->processes[index].used) {
        return 0;
    }
    return &table->processes[index];
}

const char* course_process_state_name(course_process_state_t state) {
    switch (state) {
    case COURSE_PROCESS_READY:
        return "ready";
    case COURSE_PROCESS_RUNNING:
        return "running";
    case COURSE_PROCESS_BLOCKED:
        return "blocked";
    case COURSE_PROCESS_ZOMBIE:
        return "zombie";
    case COURSE_PROCESS_DEAD:
        return "dead";
    case COURSE_PROCESS_UNUSED:
    default:
        return "unused";
    }
}

/* Stage3 VM/COW 单测：验证 fault-driven COW、引用计数和泄漏释放证据。 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_process.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_fault_driven_cow_stats_and_refcounts(void) {
    course_process_table_t table;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;
    course_process_cow_stats_t stats;
    uint32_t refcount = 0;
    uint8_t value = 0;

    course_process_table_init(&table);
    parent = course_process_spawn(&table, 0U, "parent");
    if (parent == NULL ||
        !course_process_map_user_page(&table, parent->pid, 0U, (uint8_t)'A')) {
        return fail("expected parent page mapping");
    }
    child = course_process_fork(&table, parent->pid, "child");
    if (child == NULL ||
        !course_process_page_refcount(&table, parent->pid, 0U, &refcount) ||
        refcount != 2U ||
        !course_process_cow_stats(&table, &stats) ||
        stats.saved_pages != 1U ||
        stats.refcount_peak != 2U) {
        return fail("expected fork to expose saved page and refcount evidence");
    }

    if (!course_process_handle_cow_store_fault(&table, child->pid, 0U, 0U) ||
        !course_process_cow_stats(&table, &stats) ||
        stats.cow_faults != 1U ||
        stats.copied_pages != 1U ||
        stats.shared_pages != 0U ||
        stats.refcount_peak != 2U ||
        !stats.leak_free ||
        !course_process_page_refcount(&table, child->pid, 0U, &refcount) ||
        refcount != 1U) {
        return fail("expected store fault adapter to perform COW copy");
    }

    if (!course_process_write_user_byte(&table, child->pid, 0U, 0U, (uint8_t)'B') ||
        !course_process_read_user_byte(&table, parent->pid, 0U, 0U, &value) ||
        value != (uint8_t)'A' ||
        !course_process_read_user_byte(&table, child->pid, 0U, 0U, &value) ||
        value != (uint8_t)'B') {
        return fail("expected post-fault COW pages to be isolated");
    }

    return 0;
}

static int test_cow_release_and_procfs_summary(void) {
    course_process_table_t table;
    course_process_t* parent = NULL;
    course_process_t* child = NULL;
    course_process_cow_stats_t stats;
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;
    procfs_t procfs;
    char out[512];
    int32_t status = 0;

    course_process_table_init(&table);
    parent = course_process_spawn(&table, 0U, "parent");
    if (parent == NULL ||
        !course_process_map_user_page(&table, parent->pid, 0U, (uint8_t)'P')) {
        return fail("expected parent COW setup");
    }
    child = course_process_fork(&table, parent->pid, "child");
    if (child == NULL ||
        !course_process_handle_cow_store_fault(&table, child->pid, 0U, 0U) ||
        !course_process_exit(&table, child->pid, 0) ||
        course_process_waitpid(&table, parent->pid, child->pid, &status) !=
            COURSE_PROCESS_OK ||
        !course_process_cow_stats(&table, &stats) ||
        stats.released_pages != 1U ||
        !stats.leak_free) {
        return fail("expected COW release and clean leak summary");
    }

    course_scheduler_init(&scheduler);
    course_memory_init(&memory, 2U);
    course_fs_mkfs(&fs);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    procfs_attach_processes(&procfs, &table);
    if (!procfs_read(&procfs, "/proc/cow", out, sizeof(out)) ||
        !contains(out, "cow_faults=1") ||
        !contains(out, "saved_pages=1") ||
        !contains(out, "copied_pages=1") ||
        !contains(out, "refcount_peak=2") ||
        !contains(out, "leak_free=yes")) {
        return fail("expected /proc/cow to expose Stage 3 COW evidence");
    }

    return 0;
}

static int test_exec_releases_old_user_pages(void) {
    course_process_table_t table;
    course_process_t* process = NULL;
    course_process_cow_stats_t stats;
    uint32_t refcount = 0;

    course_process_table_init(&table);
    process = course_process_spawn(&table, 0U, "exec-target");
    if (process == NULL ||
        !course_process_map_user_page(&table,
                                      process->pid,
                                      0U,
                                      (uint8_t)'E')) {
        return fail("expected old user page before exec");
    }

    if (course_process_exec(&table, process->pid, "hello", "") !=
            COURSE_PROCESS_OK ||
        course_process_page_refcount(&table, process->pid, 0U, &refcount) ||
        !course_process_cow_stats(&table, &stats) ||
        stats.released_pages != 1U ||
        !stats.leak_free) {
        return fail("expected exec to release old user pages");
    }

    return 0;
}

int main(void) {
    if (test_fault_driven_cow_stats_and_refcounts() != 0 ||
        test_cow_release_and_procfs_summary() != 0 ||
        test_exec_releases_old_user_pages() != 0) {
        return 1;
    }

    return 0;
}

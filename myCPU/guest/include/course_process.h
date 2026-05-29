#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_PROCESS_MAX_PROCESSES 16U
#define COURSE_PROCESS_MAX_NAME 24U
#define COURSE_PROCESS_MAX_ARGV 64U
#define COURSE_PROCESS_MAX_USER_PAGES 8U
#define COURSE_PROCESS_USER_PAGE_SIZE 4096U
#define COURSE_PROCESS_MAX_COW_PAGES 32U
#define COURSE_PROCESS_EXIT_CRASH (-128)

typedef enum CourseProcessState {
    COURSE_PROCESS_UNUSED = 0,
    COURSE_PROCESS_READY,
    COURSE_PROCESS_RUNNING,
    COURSE_PROCESS_BLOCKED,
    COURSE_PROCESS_ZOMBIE,
    COURSE_PROCESS_DEAD,
} course_process_state_t;

typedef enum CourseProcessResult {
    COURSE_PROCESS_OK = 0,
    COURSE_PROCESS_ERR_NO_SLOT = -1,
    COURSE_PROCESS_ERR_NO_PROCESS = -2,
    COURSE_PROCESS_ERR_NO_CHILD = -3,
    COURSE_PROCESS_ERR_NO_SUCH_PROGRAM = -4,
    COURSE_PROCESS_ERR_BAD_ADDRESS = -5,
} course_process_result_t;

typedef struct CourseProcessUserPageRef {
    bool mapped;
    uint32_t cow_page_id;
    bool writable;
    bool cow;
} course_process_user_page_ref_t;

typedef struct CourseProcessCowPage {
    bool used;
    uint32_t id;
    uint32_t refcount;
    uint8_t data[COURSE_PROCESS_USER_PAGE_SIZE];
} course_process_cow_page_t;

typedef struct CourseProcessCowStats {
    uint32_t mapped_pages;
    uint32_t shared_pages;
    uint32_t cow_faults;
    uint32_t copied_pages;
} course_process_cow_stats_t;

typedef struct CourseProcess {
    bool used;
    uint32_t pid;
    uint32_t ppid;
    course_process_state_t state;
    int32_t exit_code;
    char crash_reason[COURSE_PROCESS_MAX_NAME];
    uintptr_t crash_sepc;
    uint64_t crash_scause;
    uintptr_t crash_stval;
    void* address_space;
    void* open_files;
    char name[COURSE_PROCESS_MAX_NAME];
    char argv[COURSE_PROCESS_MAX_ARGV];
    uintptr_t entry_pc;
    uintptr_t user_sp;
    course_process_user_page_ref_t user_pages[COURSE_PROCESS_MAX_USER_PAGES];
} course_process_t;

typedef struct CourseProcessTable {
    course_process_t processes[COURSE_PROCESS_MAX_PROCESSES];
    course_process_cow_page_t cow_pages[COURSE_PROCESS_MAX_COW_PAGES];
    course_process_cow_stats_t cow_stats;
    uint32_t next_pid;
    uint32_t next_cow_page_id;
} course_process_table_t;

void course_process_table_init(course_process_table_t* table);
course_process_t* course_process_spawn(course_process_table_t* table,
                                       uint32_t ppid,
                                       const char* name);
course_process_t* course_process_fork(course_process_table_t* table,
                                      uint32_t parent_pid,
                                      const char* child_name);
bool course_process_set_state(course_process_table_t* table,
                              uint32_t pid,
                              course_process_state_t state);
bool course_process_exit(course_process_table_t* table,
                         uint32_t pid,
                         int32_t exit_code);
int32_t course_process_wait(course_process_table_t* table,
                            uint32_t parent_pid,
                            int32_t* out_status);
int32_t course_process_waitpid(course_process_table_t* table,
                               uint32_t parent_pid,
                               uint32_t child_pid,
                               int32_t* out_status);
int32_t course_process_exec(course_process_table_t* table,
                            uint32_t pid,
                            const char* program_name,
                            const char* argv);
bool course_process_record_crash(course_process_table_t* table,
                                 uint32_t pid,
                                 uintptr_t sepc,
                                 uint64_t scause,
                                 uintptr_t stval,
                                 const char* reason);
bool course_process_map_user_page(course_process_table_t* table,
                                  uint32_t pid,
                                  uint32_t page_index,
                                  uint8_t fill_value);
bool course_process_read_user_byte(course_process_table_t* table,
                                   uint32_t pid,
                                   uint32_t page_index,
                                   size_t offset,
                                   uint8_t* out_value);
bool course_process_write_user_byte(course_process_table_t* table,
                                    uint32_t pid,
                                    uint32_t page_index,
                                    size_t offset,
                                    uint8_t value);
bool course_process_cow_stats(const course_process_table_t* table,
                              course_process_cow_stats_t* out_stats);
course_process_t* course_process_find(course_process_table_t* table,
                                      uint32_t pid);
const course_process_t* course_process_at(const course_process_table_t* table,
                                          size_t index);
const char* course_process_state_name(course_process_state_t state);

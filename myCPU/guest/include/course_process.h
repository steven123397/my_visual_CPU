#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "course_elf_loader.h"

/* 课程 OS 进程表：固定槽位进程、教学级 ABI 标记、ELF 映射摘要和 COW 页引用。
   这里的进程模型负责课程 shell/syscall/procfs 的可观察状态，不直接等同 Linux task。 */
#define COURSE_PROCESS_MAX_PROCESSES 16U
#define COURSE_PROCESS_MAX_NAME 24U
#define COURSE_PROCESS_MAX_ARGV 64U
#define COURSE_PROCESS_MAX_USER_PAGES 8U
#define COURSE_PROCESS_USER_PAGE_SIZE 4096U
#define COURSE_PROCESS_MAX_COW_PAGES 32U
#define COURSE_PROCESS_MAX_MAPS COURSE_ELF_MAX_MAPS
#define COURSE_PROCESS_EXIT_CRASH (-128)

typedef enum CourseProcessState {
    COURSE_PROCESS_UNUSED = 0,
    COURSE_PROCESS_READY,
    COURSE_PROCESS_RUNNING,
    COURSE_PROCESS_BLOCKED,
    COURSE_PROCESS_ZOMBIE,
    COURSE_PROCESS_DEAD,
} course_process_state_t;

typedef enum CourseProcessAbi {
    COURSE_PROCESS_ABI_COURSE = 0,
    COURSE_PROCESS_ABI_LINUX_COMPAT,
} course_process_abi_t;

typedef enum CourseProcessResult {
    COURSE_PROCESS_OK = 0,
    COURSE_PROCESS_ERR_NO_SLOT = -1,
    COURSE_PROCESS_ERR_NO_PROCESS = -2,
    COURSE_PROCESS_ERR_NO_CHILD = -3,
    COURSE_PROCESS_ERR_NO_SUCH_PROGRAM = -4,
    COURSE_PROCESS_ERR_BAD_ADDRESS = -5,
    COURSE_PROCESS_ERR_BAD_ELF = -6,
} course_process_result_t;

typedef struct CourseProcessUserPageRef {
    bool mapped;
    uint32_t cow_page_id;
    /* writable/cow 描述课程级 COW 状态，写入时由 fault helper 决定是否复制。 */
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
    uint32_t saved_pages;
    uint32_t refcount_peak;
    uint32_t released_pages;
    bool leak_free;
} course_process_cow_stats_t;

typedef struct CourseProcess {
    bool used;
    uint32_t pid;
    uint32_t ppid;
    course_process_state_t state;
    /* abi 用来把课程 syscall 路径和 Linux compat 旁路分开，避免语义串线。 */
    course_process_abi_t abi;
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
    /* maps/user_pages 是 procfs 和 COW 证据面，不负责真实页表安装。 */
    size_t map_count;
    course_elf_map_t maps[COURSE_PROCESS_MAX_MAPS];
    course_process_user_page_ref_t user_pages[COURSE_PROCESS_MAX_USER_PAGES];
} course_process_t;

typedef struct CourseProcessTable {
    course_process_t processes[COURSE_PROCESS_MAX_PROCESSES];
    course_process_cow_page_t cow_pages[COURSE_PROCESS_MAX_COW_PAGES];
    course_process_cow_stats_t cow_stats;
    uint32_t next_pid;
    uint32_t next_cow_page_id;
} course_process_table_t;

/* 初始化进程表：清空槽位、COW 页与统计，next_pid 从 1 起。 */
void course_process_table_init(course_process_table_t* table);
/* 新建一个空进程（spawn），分配 pid 并登记 ppid/name。 */
course_process_t* course_process_spawn(course_process_table_t* table,
                                       uint32_t ppid,
                                       const char* name);
/* fork 当前进程：复制用户页为 COW 共享，子进程继承父映像摘要。 */
course_process_t* course_process_fork(course_process_table_t* table,
                                      uint32_t parent_pid,
                                      const char* child_name);
/* 设置进程状态（READY/RUNNING/BLOCKED 等）。 */
bool course_process_set_state(course_process_table_t* table,
                              uint32_t pid,
                              course_process_state_t state);
/* 设置进程 ABI（course / linux_compat）。 */
bool course_process_set_abi(course_process_table_t* table,
                            uint32_t pid,
                            course_process_abi_t abi);
/* 读取进程 ABI。 */
bool course_process_get_abi(const course_process_table_t* table,
                            uint32_t pid,
                            course_process_abi_t* out_abi);
/* 进程退出：记退出码并转 zombie，等父 wait 才释放。 */
bool course_process_exit(course_process_table_t* table,
                         uint32_t pid,
                         int32_t exit_code);
/* 等待任意子进程，回收 zombie 并输出状态。 */
int32_t course_process_wait(course_process_table_t* table,
                            uint32_t parent_pid,
                            int32_t* out_status);
/* 等待指定子进程，回收 zombie 并输出状态。 */
int32_t course_process_waitpid(course_process_table_t* table,
                               uint32_t parent_pid,
                               uint32_t child_pid,
                               int32_t* out_status);
/* exec：按程序名查找内置程序并替换进程映像摘要。 */
int32_t course_process_exec(course_process_table_t* table,
                            uint32_t pid,
                            const char* program_name,
                            const char* argv);
/* exec：直接用 ELF 镜像替换进程映像摘要（释放旧用户页，写入 loader 结果）。 */
int32_t course_process_exec_image(course_process_table_t* table,
                                  uint32_t pid,
                                  const char* image_name,
                                  const uint8_t* elf_image,
                                  size_t elf_size,
                                  const char* argv);
/* 记录一次用户态崩溃（异常上下文+原因），转异常退出但不停机。 */
bool course_process_record_crash(course_process_table_t* table,
                                 uint32_t pid,
                                 uintptr_t sepc,
                                 uint64_t scause,
                                 uintptr_t stval,
                                 const char* reason);
/* 给进程映射一个用户页（课程级 COW 页表），并用 fill_value 填充。 */
bool course_process_map_user_page(course_process_table_t* table,
                                  uint32_t pid,
                                  uint32_t page_index,
                                  uint8_t fill_value);
/* 读进程某用户页内一字节。 */
bool course_process_read_user_byte(course_process_table_t* table,
                                   uint32_t pid,
                                   uint32_t page_index,
                                   size_t offset,
                                   uint8_t* out_value);
/* 写进程某用户页内一字节，必要时触发 COW 复制。 */
bool course_process_write_user_byte(course_process_table_t* table,
                                    uint32_t pid,
                                    uint32_t page_index,
                                    size_t offset,
                                    uint8_t value);
/* 处理 store 缺页的 COW：复制共享页，恢复本进程可写副本。 */
bool course_process_handle_cow_store_fault(course_process_table_t* table,
                                           uint32_t pid,
                                           uint32_t page_index,
                                           size_t offset);
/* 取某进程某用户页的 COW 引用计数。 */
bool course_process_page_refcount(const course_process_table_t* table,
                                  uint32_t pid,
                                  uint32_t page_index,
                                  uint32_t* out_refcount);
/* kill：把目标进程标记为崩溃退出（仅演示级语义）。 */
bool course_process_kill(course_process_table_t* table,
                        uint32_t caller_pid,
                        uint32_t target_pid);
/* 拷贝出 COW 统计（映射/共享/复制/节省页数、泄漏判定）。 */
bool course_process_cow_stats(const course_process_table_t* table,
                              course_process_cow_stats_t* out_stats);
/* 按 pid 查活跃进程（可写指针）。 */
course_process_t* course_process_find(course_process_table_t* table,
                                      uint32_t pid);
/* 按槽位索引取只读进程指针（遍历用）。 */
const course_process_t* course_process_at(const course_process_table_t* table,
                                          size_t index);
/* 把进程状态枚举转成展示字符串。 */
const char* course_process_state_name(course_process_state_t state);

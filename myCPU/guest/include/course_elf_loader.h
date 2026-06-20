#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 教学级 ELF loader：只接受课程用户程序所需的保守 RV64 ELF 子集。
   loader 输出 entry、用户栈和映射摘要，实际进程替换由 course_process 负责。 */
#define COURSE_ELF_MAX_MAPS 8U
#define COURSE_ELF_MAP_NAME_MAX 12U
#define COURSE_ELF_ARGV_MAX 64U
#define COURSE_ELF_ENVP_MAX 64U
#define COURSE_ELF_USER_STACK_TOP UINT64_C(0x7FFFF000)
#define COURSE_ELF_USER_STACK_SIZE 4096U
#define COURSE_ELF_USER_HEAP_SIZE 4096U

typedef enum CourseElfResult {
    COURSE_ELF_OK = 0,
    COURSE_ELF_ERR_BAD_MAGIC = -1,
    COURSE_ELF_ERR_UNSUPPORTED = -2,
    COURSE_ELF_ERR_BAD_HEADER = -3,
    COURSE_ELF_ERR_BAD_PROGRAM_HEADER = -4,
    COURSE_ELF_ERR_NO_LOAD = -5,
} course_elf_result_t;

typedef struct CourseElfMap {
    char name[COURSE_ELF_MAP_NAME_MAX];
    /* start/end/flags 是展示和 procfs maps 使用的摘要，不直接存放段内容。 */
    uintptr_t start;
    uintptr_t end;
    uint32_t flags;
    bool cow;
} course_elf_map_t;

typedef struct CourseElfLoadResult {
    uintptr_t entry_pc;
    uintptr_t user_sp;
    /* argv/envp 是 loader 构造出的用户态启动上下文摘要，便于单测验证。 */
    size_t map_count;
    course_elf_map_t maps[COURSE_ELF_MAX_MAPS];
    uint32_t argc;
    char argv[COURSE_ELF_ARGV_MAX];
    char envp[COURSE_ELF_ENVP_MAX];
} course_elf_load_result_t;

/* 解析 RV64 ET_EXEC 镜像的 PT_LOAD 段，输出入口 PC、用户栈顶与映射摘要。 */
course_elf_result_t course_elf_loader_load(const uint8_t* image,
                                           size_t image_size,
                                           const char* argv,
                                           course_elf_load_result_t* out_load);

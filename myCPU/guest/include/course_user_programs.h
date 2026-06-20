#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "course_elf_loader.h"

/* 课程内置用户程序目录：提供受控最小 ELF 和程序类型，供 exec/shell smoke 使用。 */
typedef enum CourseUserProgramKind {
    COURSE_USER_PROGRAM_HELLO = 0,
    COURSE_USER_PROGRAM_ECHO,
    COURSE_USER_PROGRAM_CAT,
    COURSE_USER_PROGRAM_FORKTEST,
    COURSE_USER_PROGRAM_CRASH,
} course_user_program_kind_t;

typedef struct CourseUserProgram {
    const char* name;
    course_user_program_kind_t kind;
    uintptr_t entry_pc;
    uintptr_t user_sp;
    const uint8_t* elf_image;
    size_t elf_size;
    bool stage3_program;
} course_user_program_t;

/* 内置课程用户程序总数（含 crash/badelf 负向用例）。 */
size_t course_user_program_count(void);
/* Stage 3 正向课程程序数量。 */
size_t course_user_program_stage3_count(void);
/* 按名查找课程用户程序，输出其元数据与内嵌 ELF 镜像。 */
bool course_user_program_lookup(const char* name,
                                course_user_program_t* out_program);

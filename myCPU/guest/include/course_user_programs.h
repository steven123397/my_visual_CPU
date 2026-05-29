#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
} course_user_program_t;

size_t course_user_program_count(void);
bool course_user_program_lookup(const char* name,
                                course_user_program_t* out_program);

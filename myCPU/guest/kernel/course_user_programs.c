#include "course_user_programs.h"

static const course_user_program_t k_programs[] = {
    {"hello", COURSE_USER_PROGRAM_HELLO, 0x40000000U, 0x40001000U},
    {"echo", COURSE_USER_PROGRAM_ECHO, 0x40002000U, 0x40003000U},
    {"cat", COURSE_USER_PROGRAM_CAT, 0x40004000U, 0x40005000U},
    {"forktest", COURSE_USER_PROGRAM_FORKTEST, 0x40006000U, 0x40007000U},
    {"crash", COURSE_USER_PROGRAM_CRASH, 0x40008000U, 0x40009000U},
};

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;

    if (a == 0 || b == 0) {
        return false;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i += 1U;
    }
    return a[i] == b[i];
}

size_t course_user_program_count(void) {
    return sizeof(k_programs) / sizeof(k_programs[0]);
}

bool course_user_program_lookup(const char* name,
                                course_user_program_t* out_program) {
    size_t i = 0;

    if (name == 0 || out_program == 0) {
        return false;
    }

    for (i = 0; i < course_user_program_count(); ++i) {
        if (str_eq(name, k_programs[i].name)) {
            *out_program = k_programs[i];
            return true;
        }
    }
    return false;
}

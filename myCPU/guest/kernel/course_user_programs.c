#include "course_user_programs.h"

static const uint8_t k_valid_static_elf[] = {
    [0] = 0x7f, [1] = 'E', [2] = 'L', [3] = 'F',
    [4] = 2, [5] = 1, [6] = 1,
    [16] = 2, [17] = 0,
    [18] = 0xf3, [19] = 0x00,
    [20] = 1, [21] = 0, [22] = 0, [23] = 0,
    [24] = 0x00, [25] = 0x00, [26] = 0x00, [27] = 0x40,
    [32] = 0x40,
    [52] = 0x40, [53] = 0x00,
    [54] = 0x38, [55] = 0x00,
    [56] = 0x02, [57] = 0x00,

    [64] = 1, [68] = 5,
    [72] = 0xb0,
    [80] = 0x00, [81] = 0x00, [82] = 0x00, [83] = 0x40,
    [96] = 0x10,
    [104] = 0x10,
    [112] = 0x00, [113] = 0x10,

    [120] = 1, [124] = 6,
    [128] = 0xc0,
    [136] = 0x00, [137] = 0x10, [138] = 0x00, [139] = 0x40,
    [152] = 0x10,
    [160] = 0x20,
    [168] = 0x00, [169] = 0x10,

    [176] = 0x13, [177] = 0x00, [178] = 0x00, [179] = 0x00,
    [192] = 'd', [193] = 'a', [194] = 't', [195] = 'a',
    [207] = 0,
};

static const uint8_t k_bad_static_elf[] = {
    [0] = 0x00,
};

static const course_user_program_t k_programs[] = {
    {"hello", COURSE_USER_PROGRAM_HELLO, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), true},
    {"echo", COURSE_USER_PROGRAM_ECHO, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), true},
    {"cat", COURSE_USER_PROGRAM_CAT, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), true},
    {"forktest", COURSE_USER_PROGRAM_FORKTEST, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), true},
    {"crashdemo", COURSE_USER_PROGRAM_CRASH, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), true},
    {"crash", COURSE_USER_PROGRAM_CRASH, 0x40000000U, 0x7FFFF000U,
     k_valid_static_elf, sizeof(k_valid_static_elf), false},
    {"badelf", COURSE_USER_PROGRAM_HELLO, 0x40000000U, 0x7FFFF000U,
     k_bad_static_elf, sizeof(k_bad_static_elf), false},
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

size_t course_user_program_stage3_count(void) {
    size_t i = 0;
    size_t count = 0;

    for (i = 0; i < course_user_program_count(); ++i) {
        if (k_programs[i].stage3_program) {
            count += 1U;
        }
    }
    return count;
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

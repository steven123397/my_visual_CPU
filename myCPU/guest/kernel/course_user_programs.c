#include "course_user_programs.h"

/* 内嵌课程用户程序资产：每个数组都是受控最小 ELF，用于稳定测试 loader/libc/shell。
   这些不是完整应用镜像，程序行为由 shell 中的 libc-effect 模拟固定。 */

#define COURSE_ELF_IMAGE(NAME, BASE_LO, TAG0, TAG1, TAG2, TAG3) \
static const uint8_t NAME[] = { \
    [0] = 0x7f, [1] = 'E', [2] = 'L', [3] = 'F', \
    [4] = 2, [5] = 1, [6] = 1, \
    [16] = 2, [17] = 0, \
    [18] = 0xf3, [19] = 0x00, \
    [20] = 1, [21] = 0, [22] = 0, [23] = 0, \
    [24] = BASE_LO, [25] = 0x00, [26] = 0x00, [27] = 0x40, \
    [32] = 0x40, \
    [52] = 0x40, [53] = 0x00, \
    [54] = 0x38, [55] = 0x00, \
    [56] = 0x02, [57] = 0x00, \
    [64] = 1, [68] = 5, \
    [72] = 0xb0, \
    [80] = BASE_LO, [81] = 0x00, [82] = 0x00, [83] = 0x40, \
    [96] = 0x10, \
    [104] = 0x10, \
    [112] = 0x00, [113] = 0x10, \
    [120] = 1, [124] = 6, \
    [128] = 0xc0, \
    [136] = BASE_LO, [137] = 0x10, [138] = 0x00, [139] = 0x40, \
    [152] = 0x10, \
    [160] = 0x20, \
    [168] = 0x00, [169] = 0x10, \
    [176] = TAG0, [177] = TAG1, [178] = TAG2, [179] = TAG3, \
    [192] = TAG3, [193] = TAG2, [194] = TAG1, [195] = TAG0, \
    [207] = 0, \
}

COURSE_ELF_IMAGE(k_hello_elf, 0x00, 'h', 'e', 'l', 'o');
COURSE_ELF_IMAGE(k_echo_elf, 0x10, 'e', 'c', 'h', 'o');
COURSE_ELF_IMAGE(k_cat_elf, 0x20, 'c', 'a', 't', '0');
COURSE_ELF_IMAGE(k_forktest_elf, 0x30, 'f', 'o', 'r', 'k');
COURSE_ELF_IMAGE(k_crashdemo_elf, 0x40, 'c', 'r', 's', 'h');

static const uint8_t k_bad_static_elf[] = {
    [0] = 0x00,
};

static const course_user_program_t k_programs[] = {
    {"hello", COURSE_USER_PROGRAM_HELLO, 0x40000000U, 0x7FFFF000U,
     k_hello_elf, sizeof(k_hello_elf), true},
    {"echo", COURSE_USER_PROGRAM_ECHO, 0x40000010U, 0x7FFFF000U,
     k_echo_elf, sizeof(k_echo_elf), true},
    {"cat", COURSE_USER_PROGRAM_CAT, 0x40000020U, 0x7FFFF000U,
     k_cat_elf, sizeof(k_cat_elf), true},
    {"forktest", COURSE_USER_PROGRAM_FORKTEST, 0x40000030U, 0x7FFFF000U,
     k_forktest_elf, sizeof(k_forktest_elf), true},
    {"crashdemo", COURSE_USER_PROGRAM_CRASH, 0x40000040U, 0x7FFFF000U,
     k_crashdemo_elf, sizeof(k_crashdemo_elf), true},
    {"crash", COURSE_USER_PROGRAM_CRASH, 0x40000000U, 0x7FFFF000U,
     k_hello_elf, sizeof(k_hello_elf), false},
    {"badelf", COURSE_USER_PROGRAM_HELLO, 0x40000000U, 0x7FFFF000U,
     k_bad_static_elf, sizeof(k_bad_static_elf), false},
};

/* 判断两个 C 字符串是否完全相等。 */
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

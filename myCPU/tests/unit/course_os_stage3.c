#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_os_stage3.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_stage3_orchestration_summary_and_transcript(void) {
    static course_os_stage3_t stage;
    char summary[256];
    char transcript[2048];

    course_os_stage3_init(&stage);
    if (course_os_stage3_summary(&stage, summary, sizeof(summary))) {
        return fail("expected summary to fail before guardrails run");
    }
    if (!course_os_stage3_run(&stage) ||
        !course_os_stage3_summary(&stage, summary, sizeof(summary)) ||
        strcmp(summary, COURSE_OS_STAGE3_MARKER) != 0 ||
        !contains(summary, "elf=5") ||
        !contains(summary, "sync=sem/mutex") ||
        !contains(summary, "vm=sv39-cow") ||
        !contains(summary, "shell=script") ||
        !stage.elf_libc_ok ||
        !stage.sched_sync_ok ||
        !stage.vm_cow_ok ||
        !stage.fs_shell_ok ||
        !stage.proc_ok) {
        return fail("expected complete Stage 3 marker and guardrails");
    }
    if (!course_shell_transcript(&stage.shell, transcript, sizeof(transcript)) ||
        !contains(transcript, "$ exec hello") ||
        !contains(transcript, "$ exec echo") ||
        !contains(transcript, "$ exec cat /demo/input.txt") ||
        !contains(transcript, "$ sh /demo/stage3.sh") ||
        !contains(transcript, "$ cat /proc/cpuinfo") ||
        !contains(transcript, "$ cat /proc/1/maps")) {
        return fail("expected Stage 3 transcript to include key demo commands");
    }

    return 0;
}

int main(void) {
    if (test_stage3_orchestration_summary_and_transcript() != 0) {
        return 1;
    }

    return 0;
}

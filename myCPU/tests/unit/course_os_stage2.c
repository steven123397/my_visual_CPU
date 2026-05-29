#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_os_stage2.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_stage2_orchestration_summary_and_guardrails(void) {
    static course_os_stage2_t stage;
    char summary[256];
    char transcript[2048];

    course_os_stage2_init(&stage);
    if (!course_os_stage2_run(&stage)) {
        return fail("expected stage2 orchestration to run all positive and negative paths");
    }
    if (!course_os_stage2_summary(&stage, summary, sizeof(summary)) ||
        strcmp(summary, COURSE_OS_STAGE2_MARKER) != 0 ||
        !contains(summary, "syscall=ok") ||
        !contains(summary, "shell=ok") ||
        !contains(summary, "cow=ok") ||
        !contains(summary, "crash=isolated") ||
        !contains(summary, "proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog")) {
        return fail("expected stable stage2 marker");
    }

    if (!stage.bad_syscall_guarded ||
        !stage.bad_user_pointer_guarded ||
        !stage.bad_fd_guarded ||
        !stage.proc_write_guarded ||
        !stage.user_crash_guarded ||
        !stage.cow_write_guarded ||
        !stage.pipe_misuse_guarded) {
        return fail("expected all stage2 guardrails to be covered");
    }

    if (!course_shell_transcript(&stage.shell, transcript, sizeof(transcript)) ||
        !contains(transcript, "$ echo stage2") ||
        !contains(transcript, "$ ps") ||
        !contains(transcript, "$ forktest") ||
        !contains(transcript, "$ crash") ||
        !contains(transcript, "$ cat /proc/crashlog")) {
        return fail("expected stage2 shell transcript to include key demo commands");
    }

    return 0;
}

int main(void) {
    if (test_stage2_orchestration_summary_and_guardrails() != 0) {
        return 1;
    }

    return 0;
}

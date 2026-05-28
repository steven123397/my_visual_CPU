#include "course_os_stage1.h"

static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == NULL || used == NULL || *used + 1U >= out_size) {
        return false;
    }

    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

static bool append_str(char* out,
                       size_t out_size,
                       size_t* used,
                       const char* value) {
    size_t i = 0;

    if (value == NULL) {
        return false;
    }
    while (value[i] != '\0') {
        if (!append_char(out, out_size, used, value[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static bool append_u32(char* out,
                       size_t out_size,
                       size_t* used,
                       uint32_t value) {
    char digits[10];
    size_t count = 0;

    if (value == 0) {
        return append_char(out, out_size, used, '0');
    }
    while (value != 0 && count < sizeof(digits)) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count += 1U;
    }
    while (count > 0) {
        count -= 1U;
        if (!append_char(out, out_size, used, digits[count])) {
            return false;
        }
    }
    return true;
}

void course_os_stage1_init(course_os_stage1_t* stage) {
    if (stage == NULL) {
        return;
    }

    course_scheduler_init(&stage->scheduler);
    course_memory_init(&stage->memory, 3U);
    course_fs_init(&stage->fs);
    procfs_init(&stage->procfs, &stage->scheduler, &stage->memory, &stage->fs);
}

bool course_os_stage1_run(course_os_stage1_t* stage) {
    char readback[3];

    if (stage == NULL) {
        return false;
    }

    if (!course_scheduler_add_task(&stage->scheduler, 1U, 0U, 5U) ||
        !course_scheduler_add_task(&stage->scheduler, 2U, 1U, 3U) ||
        !course_scheduler_add_task(&stage->scheduler, 3U, 2U, 1U) ||
        !course_scheduler_run(&stage->scheduler, COURSE_SCHED_POLICY_FCFS, 2U) ||
        !course_scheduler_run(&stage->scheduler, COURSE_SCHED_POLICY_RR, 2U) ||
        !course_scheduler_run(&stage->scheduler,
                              COURSE_SCHED_POLICY_CFS_LITE,
                              1U)) {
        return false;
    }

    if (!course_memory_touch(&stage->memory, 0U, true) ||
        !course_memory_touch(&stage->memory, 1U, true) ||
        !course_memory_touch(&stage->memory, 2U, false) ||
        !course_memory_touch(&stage->memory, 3U, true)) {
        return false;
    }

    {
        void* first = course_kmalloc(&stage->memory, 24U);
        void* second = course_kmalloc(&stage->memory, 16U);
        void* reused = NULL;

        if (first == NULL || second == NULL) {
            return false;
        }
        course_kfree(&stage->memory, first);
        reused = course_kmalloc(&stage->memory, 12U);
        if (reused != first) {
            return false;
        }
    }

    if (!course_fs_mkdir(&stage->fs, "/home") ||
        !course_fs_mkdir(&stage->fs, "/home/course") ||
        !course_fs_create(&stage->fs, "/home/course/a.txt", false) ||
        !course_fs_create(&stage->fs, "/home/course/m.txt", false) ||
        !course_fs_create(&stage->fs, "/home/course/z.txt", false) ||
        !course_fs_write(&stage->fs, "/home/course/m.txt", 0U, "hello", 5U) ||
        !course_fs_write(&stage->fs, "/home/course/m.txt", 8U, "os", 2U) ||
        !course_fs_read(&stage->fs, "/home/course/m.txt", 8U, readback, 2U) ||
        readback[0] != 'o' || readback[1] != 's' ||
        !course_fs_lookup(&stage->fs, "/home/course/z.txt") ||
        !course_fs_unlink(&stage->fs, "/home/course/a.txt")) {
        return false;
    }

    return true;
}

bool course_os_stage1_summary(const course_os_stage1_t* stage,
                              char* out,
                              size_t out_size) {
    course_scheduler_summary_t sched;
    course_memory_stats_t mem;
    course_fs_stats_t fs;
    size_t used = 0;

    if (stage == NULL || out == NULL || out_size == 0 ||
        !course_scheduler_summary(&stage->scheduler, &sched) ||
        !course_memory_stats(&stage->memory, &mem) ||
        !course_fs_stats(&stage->fs, &fs)) {
        return false;
    }

    out[0] = '\0';
    return append_str(out, out_size, &used, "course-os-stage1 sched=") &&
           append_str(out,
                      out_size,
                      &used,
                      course_scheduler_policy_name(sched.policy)) &&
           append_str(out, out_size, &used, " ctx=") &&
           append_u32(out, out_size, &used, sched.context_switches) &&
           append_str(out, out_size, &used, " pf=") &&
           append_u32(out, out_size, &used, mem.page_faults) &&
           append_str(out, out_size, &used, " reclaim=") &&
           append_u32(out, out_size, &used, mem.page_reclaims) &&
           append_str(out, out_size, &used, " fs_create=") &&
           append_u32(out,
                      out_size,
                      &used,
                      fs.file_creates + fs.dir_creates) &&
           append_str(out, out_size, &used, " btree_steps=") &&
           append_u32(out, out_size, &used, fs.btree_compare_steps) &&
           append_str(out,
                      out_size,
                      &used,
                      " proc=ps/meminfo/schedstat/fsstat");
}

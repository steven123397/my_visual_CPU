#include <cstdio>

#include "../../src/exec/reorder_buffer.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    ReorderBuffer rob;
    const RobIndex first = rob.allocate({
        .sequence_id = 10,
        .pc = 0x80000000,
        .raw = 0x00100093U,
        .arch_rd = 1,
        .phys_rd = 33,
        .previous_phys_rd = 1,
    });
    const RobIndex second = rob.allocate({
        .sequence_id = 11,
        .pc = 0x80000004,
        .raw = 0x00208113U,
        .arch_rd = 2,
        .phys_rd = 34,
        .previous_phys_rd = 2,
    });

    const auto head_before_ready = rob.peek_head();
    if (!expect(head_before_ready.has_value() && head_before_ready->sequence_id == 10 &&
                    head_before_ready->previous_phys_rd == 1 && !head_before_ready->ready,
                "ROB head should retain the stale physical destination for the oldest entry")) {
        return 1;
    }

    rob.mark_ready(second, {
        .value_ready = true,
        .value = 42,
    });
    const auto head_after_younger_ready = rob.peek_head();
    if (!expect(head_after_younger_ready.has_value() && head_after_younger_ready->sequence_id == 10,
                "ROB should keep in-order commit even when a younger entry becomes ready first")) {
        return 1;
    }

    rob.mark_ready(first, {
        .value_ready = true,
        .value = 7,
    });
    const auto ready_head = rob.peek_head();
    if (!expect(ready_head.has_value() && ready_head->ready && ready_head->value_ready && ready_head->value == 7,
                "ROB should surface ready/value state on the oldest entry")) {
        return 1;
    }

    rob.commit_head();
    const auto head_after_commit = rob.peek_head();
    if (!expect(head_after_commit.has_value() && head_after_commit->sequence_id == 11,
                "commit_head should retire the oldest ready entry")) {
        return 1;
    }

    rob.allocate({
        .sequence_id = 12,
        .pc = 0x80000008,
        .raw = 0x003101b3U,
        .arch_rd = 3,
        .phys_rd = 35,
        .previous_phys_rd = 3,
    });
    rob.flush_younger_than(11);
    if (!expect(rob.size() == 1, "flush_younger_than should drop entries younger than the provided sequence")) {
        return 1;
    }
    const auto head_after_flush = rob.peek_head();
    if (!expect(head_after_flush.has_value() && head_after_flush->sequence_id == 11,
                "flush_younger_than should preserve the oldest non-flushed ROB head")) {
        return 1;
    }

    ReorderBuffer fp_rob;
    fp_rob.allocate({
        .sequence_id = 20,
        .pc = 0x80000020,
        .raw = 0x60b504c3U,  // fmadd.s fs1, fa0, fa1, fa2
        .arch_rd = 0,
        .phys_rd = 0,
        .previous_phys_rd = 0,
    });
    if (!expect(fp_rob.has_older_fp_pending(21, 9, 0xffU, 0xffU),
                "ROB should treat older fmadd.s as a pending FPR writer for younger consumers")) {
        return 1;
    }

    ReorderBuffer minmax_rob;
    minmax_rob.allocate({
        .sequence_id = 30,
        .pc = 0x80000030,
        .raw = 0x2ab504d3U,  // fmin.d fs1, fa0, fa1
        .arch_rd = 0,
        .phys_rd = 0,
        .previous_phys_rd = 0,
    });
    if (!expect(minmax_rob.has_older_fp_pending(31, 9, 0xffU, 0xffU),
                "ROB should treat older fmin.d as a pending FPR writer for younger consumers")) {
        return 1;
    }

    return 0;
}

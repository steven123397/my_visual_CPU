#include <cstdio>

#include "../../src/exec/rename_map.h"

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
    RenameMap map;
    if (!expect(map.map_source(5) == 5 && map.architectural_source(5) == 5,
                "rename map should start with identity architectural mappings")) {
        return 1;
    }

    const RenameCheckpoint checkpoint = map.checkpoint();
    const RenameDestResult first_rename = map.rename_dest(5);
    if (!expect(first_rename.phys != 5 && first_rename.previous_phys == 5 &&
                    map.map_source(5) == first_rename.phys &&
                    map.architectural_source(5) == 5,
                "rename_dest should report both the new physical register and the previous speculative mapping")) {
        return 1;
    }

    map.rollback(checkpoint);
    if (!expect(map.map_source(5) == 5 && map.architectural_source(5) == 5,
                "rollback should restore the checkpointed speculative mapping")) {
        return 1;
    }

    const RenameDestResult committed_dest = map.rename_dest(10);
    const uint32_t recycled_after_commit = map.commit_dest(10, committed_dest.phys);
    if (!expect(map.map_source(10) == committed_dest.phys &&
                    map.architectural_source(10) == committed_dest.phys,
                "commit_dest should advance the architectural rename map")) {
        return 1;
    }
    if (!expect(recycled_after_commit == 10,
                "commit_dest should return the stale committed physical register for later recycle")) {
        return 1;
    }

    const RenameDestResult zero_dest = map.rename_dest(0);
    if (!expect(zero_dest.phys == 0 && zero_dest.previous_phys == 0 && map.map_source(0) == 0,
                "x0 rename must stay a no-op and must not consume recycled physical registers")) {
        return 1;
    }

    const RenameCheckpoint recycled_checkpoint = map.checkpoint();
    const RenameDestResult reused_after_commit = map.rename_dest(11);
    if (!expect(reused_after_commit.phys == recycled_after_commit &&
                    reused_after_commit.previous_phys == 11,
                "a later rename should reuse the stale committed physical register before allocating a fresh tag")) {
        return 1;
    }
    map.rollback(recycled_checkpoint);
    const RenameDestResult reused_after_rollback = map.rename_dest(12);
    if (!expect(reused_after_rollback.phys == recycled_after_commit &&
                    reused_after_rollback.previous_phys == 12,
                "rollback should restore the recycled free-list state so the same phys tag can be reused again")) {
        return 1;
    }
    const uint32_t recycled_after_recommit = map.commit_dest(12, reused_after_rollback.phys);
    if (!expect(recycled_after_recommit == 12,
                "committing a reused phys tag should still recycle the stale committed mapping of the new destination")) {
        return 1;
    }
    map.rollback(map.committed_checkpoint());
    const RenameDestResult reused_stale_committed_phys = map.rename_dest(13);
    if (!expect(reused_stale_committed_phys.phys == recycled_after_recommit,
                "rollback to the committed baseline should still reuse the newly stale committed phys first")) {
        return 1;
    }
    const RenameDestResult must_not_reuse_live_committed_phys = map.rename_dest(14);
    if (!expect(must_not_reuse_live_committed_phys.phys != reused_after_rollback.phys,
                "rollback to the committed baseline must not keep a now-live committed phys tag in the free-list")) {
        return 1;
    }

    const RenameCheckpoint committed_checkpoint = map.checkpoint();
    const RenameDestResult renamed_again = map.rename_dest(10);
    if (!expect(renamed_again.previous_phys == committed_dest.phys,
                "rename_dest should surface the last committed mapping as the next stale physical register")) {
        return 1;
    }
    map.rollback(committed_checkpoint);
    if (!expect(map.map_source(10) == committed_dest.phys,
                "rollback should preserve the last committed mapping as the speculative baseline")) {
        return 1;
    }

    return 0;
}

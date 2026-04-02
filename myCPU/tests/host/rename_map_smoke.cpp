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
    map.commit_dest(10, committed_dest.phys);
    if (!expect(map.map_source(10) == committed_dest.phys &&
                    map.architectural_source(10) == committed_dest.phys,
                "commit_dest should advance the architectural rename map")) {
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

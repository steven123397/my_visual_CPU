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
    const uint16_t speculative_phys = map.rename_dest(5);
    if (!expect(speculative_phys != 5 && map.map_source(5) == speculative_phys &&
                    map.architectural_source(5) == 5,
                "rename_dest should update only the speculative mapping")) {
        return 1;
    }

    map.rollback(checkpoint);
    if (!expect(map.map_source(5) == 5 && map.architectural_source(5) == 5,
                "rollback should restore the checkpointed speculative mapping")) {
        return 1;
    }

    const uint16_t committed_phys = map.rename_dest(10);
    map.commit_dest(10, committed_phys);
    if (!expect(map.map_source(10) == committed_phys && map.architectural_source(10) == committed_phys,
                "commit_dest should advance the architectural rename map")) {
        return 1;
    }

    const RenameCheckpoint committed_checkpoint = map.checkpoint();
    (void) map.rename_dest(10);
    map.rollback(committed_checkpoint);
    if (!expect(map.map_source(10) == committed_phys,
                "rollback should preserve the last committed mapping as the speculative baseline")) {
        return 1;
    }

    return 0;
}

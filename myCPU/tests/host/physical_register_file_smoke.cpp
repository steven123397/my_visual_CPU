#include <cstdio>

#include "../../src/exec/physical_register_file.h"

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
    PhysicalRegisterFile file;

    if (!expect(file.read(0) == 0 && file.is_ready(0),
                "physical register file should expose architectural x0 as ready zero")) {
        return 1;
    }

    file.set_pending(33);
    if (!expect(!file.is_ready(33),
                "set_pending should mark a destination physical register as not ready")) {
        return 1;
    }

    file.write(33, 0x123456789abcdef0ULL);
    if (!expect(file.is_ready(33) && file.read(33) == 0x123456789abcdef0ULL,
                "write should publish a ready speculative value")) {
        return 1;
    }

    const PhysicalRegisterCheckpoint checkpoint = file.checkpoint();
    file.write(33, 0x55);
    file.set_pending(34);
    file.rollback(checkpoint);

    if (!expect(file.is_ready(33) && file.read(33) == 0x123456789abcdef0ULL,
                "rollback should restore the checkpointed value and ready state")) {
        return 1;
    }

    return 0;
}

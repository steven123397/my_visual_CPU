#include <cstdio>

#include "../../src/debug/debug_session.h"

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
    DebugSession session;
    session.load_elf("tests/asm/hello.elf", BackendKind::Pipeline, nullptr);

    const DebugSnapshot before = session.snapshot();
    session.step_cycle();
    const DebugSnapshot after = session.snapshot();
    if (!expect(after.summary.cycle == before.summary.cycle + 1, "step_cycle should advance cycle")) {
        return 1;
    }
    if (!expect(
            after.pipeline.if_stage.valid || after.pipeline.id_stage.valid || after.pipeline.ex_stage.valid,
            "pipeline snapshot should expose inflight stages after stepping")) {
        return 1;
    }

    for (int i = 0; i < 128 && !session.snapshot().summary.halted; ++i) {
        session.step_commit();
    }

    const DebugSnapshot halted = session.snapshot();
    if (!expect(halted.summary.halted, "step_commit loop should eventually halt the hello test")) {
        return 1;
    }
    if (!expect(!halted.events.empty(), "debug snapshot should preserve recent execution events")) {
        return 1;
    }
    if (!expect(halted.devices.uart.output_size > 0, "uart snapshot should report produced output")) {
        return 1;
    }

    return 0;
}

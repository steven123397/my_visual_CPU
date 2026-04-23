#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>

#include "../../src/devices/ai_scratchpad.h"

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
    try {
        AiScratchpad scratchpad;
        scratchpad.configure(64, 16, 8);

        const AiScratchpadLayout layout = scratchpad.layout();
        if (!expect(layout.scratchpad_base == 0, "expected scratchpad space to start at 0") ||
            !expect(layout.scratchpad_bytes == 64, "expected scratchpad bytes") ||
            !expect(layout.accumulator_base == 64, "expected accumulator base") ||
            !expect(layout.accumulator_bytes == 16, "expected accumulator bytes") ||
            !expect(layout.temporary_base == 80, "expected temporary base") ||
            !expect(layout.temporary_bytes == 8, "expected temporary bytes") ||
            !expect(layout.total_bytes == 88, "expected total scratchpad bytes")) {
            return 1;
        }

        const std::array<uint8_t, 4> payload{{0x10, 0x20, 0x30, 0x40}};
        std::array<uint8_t, 4> readback{{0, 0, 0, 0}};
        if (!expect(
                scratchpad.write(AiScratchpadSpace::Scratchpad, 4, payload.data(), payload.size()),
                "expected scratchpad write to succeed") ||
            !expect(
                scratchpad.read(AiScratchpadSpace::Scratchpad, 4, readback.data(), readback.size()),
                "expected scratchpad read to succeed") ||
            !expect(readback == payload, "expected scratchpad readback to match")) {
            return 1;
        }

        const std::array<uint8_t, 4> accum{{0x55, 0x66, 0x77, 0x88}};
        readback.fill(0);
        if (!expect(
                scratchpad.write(AiScratchpadSpace::Accumulator, 0, accum.data(), accum.size()),
                "expected accumulator write to succeed") ||
            !expect(
                scratchpad.read(AiScratchpadSpace::Accumulator, 0, readback.data(), readback.size()),
                "expected accumulator read to succeed") ||
            !expect(readback == accum, "expected accumulator readback to match")) {
            return 1;
        }

        readback.fill(0xEE);
        if (!expect(
                !scratchpad.write(AiScratchpadSpace::Temporary, 6, payload.data(), payload.size()),
                "expected temporary overflow write rejection") ||
            !expect(
                !scratchpad.read(AiScratchpadSpace::Scratchpad, 62, readback.data(), readback.size()),
                "expected scratchpad overflow read rejection")) {
            return 1;
        }

        scratchpad.reset();
        readback.fill(0xAA);
        if (!expect(
                scratchpad.read(AiScratchpadSpace::Scratchpad, 4, readback.data(), readback.size()),
                "expected post-reset scratchpad read to succeed") ||
            !expect(
                readback == std::array<uint8_t, 4>{{0, 0, 0, 0}},
                "expected scratchpad reset to zero the buffer")) {
            return 1;
        }

        std::puts("ai_scratchpad: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

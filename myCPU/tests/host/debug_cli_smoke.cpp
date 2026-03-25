#include <cstdio>
#include <sstream>
#include <string>

#include "../../src/debug/debug_protocol.h"

namespace {

bool expect_contains(const std::string& haystack, const char* needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

}  // namespace

int main() {
    std::istringstream in(
        "{\"cmd\":\"load\",\"image\":\"tests/asm/hello.elf\",\"backend\":\"pipeline\"}\n"
        "{\"cmd\":\"snapshot\"}\n"
        "{\"cmd\":\"step_cycle\"}\n"
        "{\"cmd\":\"step_commit\"}\n"
        "{\"cmd\":\"reset\"}\n"
        "{\"cmd\":\"quit\"}\n");
    std::ostringstream out;
    std::ostringstream err;

    const int status = run_debug_cli(in, out, err);
    if (status != 0) {
        std::fprintf(stderr, "debug cli exited with status %d\n", status);
        std::fprintf(stderr, "stderr:\n%s\n", err.str().c_str());
        return 1;
    }

    const std::string output = out.str();
    if (!expect_contains(output, "\"cmd\":\"load\"", "load response should be emitted")) {
        return 1;
    }
    if (!expect_contains(output, "\"type\":\"snapshot\"", "snapshot response should be emitted")) {
        return 1;
    }
    if (!expect_contains(output, "\"backend\":\"pipeline\"", "snapshot should report pipeline backend")) {
        return 1;
    }
    if (!expect_contains(output, "\"pipeline\"", "snapshot should include pipeline section")) {
        return 1;
    }
    if (!expect_contains(output, "\"gpr\"", "snapshot should include register state")) {
        return 1;
    }
    if (!expect_contains(output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    return 0;
}

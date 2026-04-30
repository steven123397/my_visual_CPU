#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/exec/dbt_executable_memory.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool test_executable_memory_allocates_writes_seals_and_releases() {
    DbtExecutableMemoryBlock block = allocate_dbt_executable_memory(16);
    const uint8_t bytes[] = {0x90, 0x90, 0xc3};
    const DbtExecutableMemoryResult write =
        write_dbt_executable_memory(block, 0, bytes, sizeof(bytes));
    const DbtExecutableMemoryResult seal = seal_dbt_executable_memory(block);
    const uint8_t after_seal[] = {0xcc};
    const DbtExecutableMemoryResult rejected_write =
        write_dbt_executable_memory(block, 0, after_seal, sizeof(after_seal));
    const DbtExecutableMemoryResult release = release_dbt_executable_memory(block);

    return expect(block.released,
                  "executable memory release should mark block released") &&
           expect(write.ok,
                  "executable memory write should succeed while writable") &&
           expect(seal.ok,
                  "executable memory seal should succeed") &&
           expect(!seal.writable && seal.executable,
                  "sealed executable memory should not remain writable") &&
           expect(!rejected_write.ok &&
                      rejected_write.error == "executable-memory-not-writable",
                  "sealed executable memory should reject writes") &&
           expect(release.ok && !block.allocated && !block.writable &&
                      !block.executable,
                  "executable memory release should clear live flags");
}

bool test_executable_memory_rejects_invalid_requests() {
    DbtExecutableMemoryBlock zero = allocate_dbt_executable_memory(0);
    DbtExecutableMemoryBlock block = allocate_dbt_executable_memory(4);
    const uint8_t bytes[] = {1, 2, 3, 4, 5};
    const DbtExecutableMemoryResult overflow =
        write_dbt_executable_memory(block, 0, bytes, sizeof(bytes));
    release_dbt_executable_memory(block);
    const DbtExecutableMemoryResult double_release = release_dbt_executable_memory(block);

    return expect(!zero.allocated && zero.error == "invalid-executable-memory-size",
                  "zero-sized executable memory allocation should be rejected") &&
           expect(block.released,
                  "block should be released before double release check") &&
           expect(!overflow.ok && overflow.error == "executable-memory-write-out-of-range",
                  "executable memory write should reject overflow") &&
           expect(!double_release.ok &&
                      double_release.error == "executable-memory-not-allocated",
                  "executable memory release should reject already released blocks");
}

bool test_executable_memory_formatter_is_stable() {
    DbtExecutableMemoryBlock block = allocate_dbt_executable_memory(8);
    const std::string line = format_dbt_executable_memory_block(block);
    release_dbt_executable_memory(block);

    return expect(line.find("executable-memory: allocated=true") != std::string::npos,
                  "executable memory formatter should expose allocation state") &&
           expect(line.find("writable=true") != std::string::npos,
                  "executable memory formatter should expose writable state") &&
           expect(line.find("executable=false") != std::string::npos,
                  "executable memory formatter should expose executable state");
}

}  // namespace

int main() {
    if (!test_executable_memory_allocates_writes_seals_and_releases()) {
        return 1;
    }
    if (!test_executable_memory_rejects_invalid_requests()) {
        return 1;
    }
    if (!test_executable_memory_formatter_is_stable()) {
        return 1;
    }
    std::puts("dbt_executable_memory_smoke: PASS");
    return 0;
}

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

#include <unistd.h>

#include "../../src/loader/binary_loader.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

constexpr uint64_t kLoadAddr = MEM_BASE + 0x2000;
constexpr uint64_t kProbeAddr = MEM_BASE + 0x3000;

std::string create_temp_path() {
    char path_template[] = "/tmp/mycpu_binary_loader_XXXXXX";
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp binary");
    }
    close(fd);
    return path_template;
}

std::string write_temp_binary(const std::array<uint8_t, 4>& bytes) {
    const std::string path = create_temp_path();
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp binary for writing");
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.close();
    return path;
}

bool expect_failure(const char* path, uint64_t addr, const char* expected_message) {
    BinaryLoader loader;
    Ram ram;
    ram.fill(kProbeAddr, 0xAA, 8);

    bool threw = false;
    try {
        loader.load(ram, path, addr);
    } catch (const std::runtime_error& ex) {
        threw = true;
        if (std::string(ex.what()).find(expected_message) == std::string::npos) {
            std::fprintf(stderr,
                         "unexpected error: expected '%s', got '%s'\n",
                         expected_message,
                         ex.what());
            return false;
        }
    }

    if (!threw) {
        std::fprintf(stderr, "expected BinaryLoader to reject input: %s\n", expected_message);
        return false;
    }

    for (uint64_t i = 0; i < 8; ++i) {
        if (ram.load(kProbeAddr + i, 1) != 0xAA) {
            std::fprintf(stderr, "BinaryLoader modified RAM despite rejection\n");
            return false;
        }
    }

    return true;
}

}  // namespace

int main() {
    try {
        const std::array<uint8_t, 4> bytes = {0x11, 0x22, 0x33, 0x44};
        const std::string path = write_temp_binary(bytes);

        {
            Ram ram;
            BinaryLoader loader;
            loader.load(ram, path.c_str(), kLoadAddr);
            for (size_t i = 0; i < bytes.size(); ++i) {
                if (ram.load(kLoadAddr + i, 1) != bytes[i]) {
                    std::fprintf(stderr, "binary byte %zu did not load at the requested address\n", i);
                    std::remove(path.c_str());
                    return 1;
                }
            }
        }

        if (!expect_failure("/tmp/mycpu_binary_loader_missing.bin",
                            kLoadAddr,
                            "failed to open binary")) {
            std::remove(path.c_str());
            return 1;
        }

        if (!expect_failure(path.c_str(),
                            MEM_BASE - 4,
                            "binary too large")) {
            std::remove(path.c_str());
            return 1;
        }

        if (!expect_failure(path.c_str(),
                            MEM_BASE + MEM_SIZE - 2,
                            "binary too large")) {
            std::remove(path.c_str());
            return 1;
        }

        std::remove(path.c_str());
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

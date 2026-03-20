#include "binary_loader.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../mem/ram.h"
#include "../platform/address_map.h"

void BinaryLoader::load(Ram& ram, const char* path, uint64_t addr) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("failed to open binary: ") + path);
    }

    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error(std::string("failed to size binary: ") + path);
    }

    const uint64_t memory_end = MEM_BASE + MEM_SIZE;
    const uint64_t byte_count = static_cast<uint64_t>(size);
    if (addr < MEM_BASE || addr > memory_end || byte_count > memory_end - addr) {
        throw std::runtime_error("binary too large");
    }

    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        throw std::runtime_error(std::string("short read while loading binary: ") + path);
    }

    ram.write_bytes(addr, bytes.data(), bytes.size());
}

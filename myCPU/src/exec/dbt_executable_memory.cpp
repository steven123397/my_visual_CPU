#include "dbt_executable_memory.h"

#include <cstring>
#include <sstream>

#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace {

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

size_t page_size() {
    const long value = sysconf(_SC_PAGESIZE);
    return value > 0 ? static_cast<size_t>(value) : 4096;
}

size_t round_up_to_page(size_t size) {
    const size_t page = page_size();
    return ((size + page - 1) / page) * page;
}

DbtExecutableMemoryResult error_result(const char* error) {
    return DbtExecutableMemoryResult{
        .ok = false,
        .error = error,
    };
}

}  // namespace

DbtExecutableMemoryBlock allocate_dbt_executable_memory(size_t size) {
    if (size == 0) {
        return DbtExecutableMemoryBlock{
            .error = "invalid-executable-memory-size",
        };
    }

    const size_t mapped = round_up_to_page(size);
    void* memory = mmap(nullptr,
                        mapped,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if (memory == MAP_FAILED) {
        return DbtExecutableMemoryBlock{
            .requested_size = size,
            .mapped_size = mapped,
            .error = "executable-memory-allocation-failed",
        };
    }

    return DbtExecutableMemoryBlock{
        .data = memory,
        .requested_size = size,
        .mapped_size = mapped,
        .allocated = true,
        .writable = true,
        .executable = false,
        .released = false,
    };
}

DbtExecutableMemoryResult write_dbt_executable_memory(DbtExecutableMemoryBlock& block,
                                                      size_t offset,
                                                      const void* data,
                                                      size_t size) {
    if (!block.allocated || block.data == nullptr) {
        return error_result("executable-memory-not-allocated");
    }
    if (!block.writable) {
        return error_result("executable-memory-not-writable");
    }
    if (data == nullptr && size != 0) {
        return error_result("executable-memory-null-write");
    }
    if (offset > block.requested_size || size > block.requested_size - offset) {
        return error_result("executable-memory-write-out-of-range");
    }

    std::memcpy(static_cast<uint8_t*>(block.data) + offset, data, size);
    return DbtExecutableMemoryResult{
        .ok = true,
        .writable = block.writable,
        .executable = block.executable,
    };
}

DbtExecutableMemoryResult seal_dbt_executable_memory(DbtExecutableMemoryBlock& block) {
    if (!block.allocated || block.data == nullptr) {
        return error_result("executable-memory-not-allocated");
    }
    if (!block.writable) {
        return error_result("executable-memory-not-writable");
    }

    if (mprotect(block.data, block.mapped_size, PROT_READ | PROT_EXEC) != 0) {
        return error_result("executable-memory-seal-failed");
    }
    block.writable = false;
    block.executable = true;
    return DbtExecutableMemoryResult{
        .ok = true,
        .writable = block.writable,
        .executable = block.executable,
    };
}

DbtExecutableMemoryResult release_dbt_executable_memory(DbtExecutableMemoryBlock& block) {
    if (!block.allocated || block.data == nullptr) {
        return error_result("executable-memory-not-allocated");
    }

    const int rc = munmap(block.data, block.mapped_size);
    if (rc != 0) {
        return error_result("executable-memory-release-failed");
    }

    block.data = nullptr;
    block.requested_size = 0;
    block.mapped_size = 0;
    block.allocated = false;
    block.writable = false;
    block.executable = false;
    block.released = true;
    return DbtExecutableMemoryResult{
        .ok = true,
        .writable = false,
        .executable = false,
    };
}

std::string format_dbt_executable_memory_block(const DbtExecutableMemoryBlock& block) {
    std::ostringstream out;
    out << "executable-memory:"
        << " allocated=" << bool_name(block.allocated)
        << " writable=" << bool_name(block.writable)
        << " executable=" << bool_name(block.executable)
        << " released=" << bool_name(block.released)
        << " requested=" << block.requested_size
        << " mapped=" << block.mapped_size
        << " error=" << (block.error.empty() ? "none" : block.error);
    return out.str();
}

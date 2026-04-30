#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct DbtExecutableMemoryResult {
    bool ok{false};
    std::string error{};
    bool writable{false};
    bool executable{false};
};

struct DbtExecutableMemoryBlock {
    void* data{nullptr};
    size_t requested_size{0};
    size_t mapped_size{0};
    bool allocated{false};
    bool writable{false};
    bool executable{false};
    bool released{false};
    std::string error{};
};

DbtExecutableMemoryBlock allocate_dbt_executable_memory(size_t size);
DbtExecutableMemoryResult write_dbt_executable_memory(DbtExecutableMemoryBlock& block,
                                                      size_t offset,
                                                      const void* data,
                                                      size_t size);
DbtExecutableMemoryResult seal_dbt_executable_memory(DbtExecutableMemoryBlock& block);
DbtExecutableMemoryResult release_dbt_executable_memory(DbtExecutableMemoryBlock& block);
std::string format_dbt_executable_memory_block(const DbtExecutableMemoryBlock& block);

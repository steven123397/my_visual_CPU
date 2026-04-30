#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "dbt_executable_memory.h"
#include "dbt_ir_lowering.h"

enum class DbtHostEmitterBackend : uint8_t {
    None,
    X86_64SysV,
};

struct DbtHostExecutableStats {
    uint64_t instructions_emitted{0};
    uint64_t bytes_emitted{0};
};

struct DbtHostExecutable {
    bool ok{false};
    DbtHostEmitterBackend backend{DbtHostEmitterBackend::None};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    DbtExecutableMemoryBlock memory{};
    size_t code_size{0};
    bool generated_host_code{false};
    bool requested_executable_memory{false};
    bool executed_guest_code{false};
    DbtHostExecutableStats stats{};
};

DbtHostExecutable emit_dbt_host_block(const DbtIrLoweringResult& lowered);
uint64_t execute_dbt_host_block(const DbtHostExecutable& executable,
                                uint64_t* gpr,
                                uint64_t pc);
void release_dbt_host_executable(DbtHostExecutable& executable);

const char* dbt_host_emitter_backend_name(DbtHostEmitterBackend backend);
std::string format_dbt_host_executable(const DbtHostExecutable& executable);

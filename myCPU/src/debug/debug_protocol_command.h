#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../platform/address_map.h"

enum class DebugCliCommandKind {
    Load,
    LoadPayload,
    SetGpr,
    Snapshot,
    StepCycle,
    StepCommit,
    RunUntilUartContains,
    RunUntilHalt,
    Reset,
    UartInput,
    UartOutput,
    Quit,
};

struct DebugCliCommand {
    DebugCliCommandKind kind{DebugCliCommandKind::Snapshot};
    std::string image{};
    std::string backend{};
    std::string block_transport{};
    std::string disk{};
    std::string text{};
    bool disk_ready{true};
    bool disk_magic_valid{true};
    bool flat{false};
    std::string reg_name{};
    uint64_t addr{MEM_BASE};
    uint64_t value{0};
    uint64_t count{1};
    uint64_t max_steps{0};
    size_t offset{0};
};

DebugCliCommand parse_debug_cli_command(const std::string& line);

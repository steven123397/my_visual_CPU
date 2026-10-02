#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "../arch/core_state.h"
#include "../isa/effects.h"

enum class MemoryOperation : uint8_t { Load, Store, LR, SC, AMO };
enum class MemoryOutcome : uint8_t { Success, Fault, NoAccess };
struct MemorySegment {
    uint64_t vaddr{0};
    std::optional<uint64_t> paddr{};
    uint32_t bytes{0};
    bool write{false};
    std::optional<uint64_t> value{};
    MemoryOutcome outcome{MemoryOutcome::Fault};
};
struct MemoryRecord {
    uint64_t capture_id{0}, machine_generation{0}, instruction_id{0};
    uint64_t pc{0}, satp{0}, start_cycle{0}, start_instret{0}, end_cycle{0}, end_instret{0};
    uint32_t raw{0};
    uint8_t instruction_bytes{0};
    PrivilegeMode privilege{PrivilegeMode::Machine}, effective_privilege{PrivilegeMode::Machine};
    MemoryOperation operation{MemoryOperation::Load};
    AtomicRequest::Kind atomic_kind{AtomicRequest::Kind::None};
    uint64_t vaddr{0};
    uint32_t bytes{0};
    std::optional<uint64_t> store_value{};
    std::array<MemorySegment, 4> segments{};
    size_t segment_count{0};
    bool representable{true}, retired{false};
    TrapRequest trap{};
    std::optional<uint64_t> sc_result{};
    void append(MemorySegment segment) {
        if (segment_count == segments.size()) { representable = false; return; }
        segments[segment_count++] = segment;
    }
};
class MemoryRecordReceiver {
public:
    virtual ~MemoryRecordReceiver() = default;
    virtual void receive(MemoryRecord record) = 0;
    virtual void unsupported() = 0;
};

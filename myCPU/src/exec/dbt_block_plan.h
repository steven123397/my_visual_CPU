#pragma once

#include <cstdint>
#include <string>
#include <vector>

class CPU;
class Bus;
struct ExecutionProfileSnapshot;

enum class DbtBoundaryKind : uint8_t {
    None,
    FetchFault,
    Unsupported,
    MemoryLoad,
    MemoryStore,
    Atomic,
    Vector,
    CsrWrite,
    Trap,
    Halt,
    TlbFlush,
    TrapReturn,
    ControlFlow,
    NotRetired,
    Fallback,
};

enum class DbtDryRunIrKind : uint8_t {
    ArchitectedEffect,
};

struct DbtDryRunIrOp {
    DbtDryRunIrKind kind{DbtDryRunIrKind::ArchitectedEffect};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t size{0};
    uint64_t next_pc{0};
    bool rd_write{false};
    uint8_t rd{0};
};

struct DbtBlockPlan {
    bool ok{false};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    uint64_t candidate_executions{0};
    uint64_t candidate_retired_instructions{0};
    uint64_t inlineable_instructions{0};
    uint64_t fallback_pc{0};
    uint32_t fallback_raw{0};
    std::string fallback_reason{};
    std::string boundary_kind{};
    DbtBoundaryKind boundary{DbtBoundaryKind::None};
    std::vector<DbtDryRunIrOp> dry_run_ir{};
};

enum class DbtInvalidationEventKind : uint8_t {
    PrimaryImageLoad,
    DebugReset,
    PayloadLoad,
    GuestStore,
    SatpWrite,
    SfenceVma,
    RegionAttributesChanged,
};

struct DbtInvalidationPlan {
    bool invalidates{false};
    std::string reason{};
};

DbtBlockPlan plan_dbt_block(CPU& cpu, Bus& bus, uint64_t start_pc, uint64_t end_pc);

DbtBlockPlan plan_dbt_hot_path(CPU& cpu,
                               Bus& bus,
                               const ExecutionProfileSnapshot& profile);

DbtInvalidationPlan plan_dbt_block_invalidation_event(DbtInvalidationEventKind kind,
                                                      uint64_t event_addr,
                                                      uint64_t event_size,
                                                      uint64_t block_start_pc,
                                                      uint64_t block_end_pc);

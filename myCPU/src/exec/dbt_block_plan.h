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

enum class DbtHelperKind : uint8_t {
    None,
    MemoryLoad,
    MemoryStore,
    CsrWrite,
    Atomic,
    Vector,
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

struct DbtHelperPlan {
    bool required{false};
    DbtHelperKind kind{DbtHelperKind::None};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t rd{0};
    uint64_t addr{0};
    uint8_t size{0};
    bool sign_extend{false};
    bool commit_at_boundary{false};
    bool non_speculative{false};
    uint32_t csr_addr{0};
    uint64_t value{0};
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
    DbtHelperPlan helper_plan{};
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

const char* dbt_helper_kind_name(DbtHelperKind kind);

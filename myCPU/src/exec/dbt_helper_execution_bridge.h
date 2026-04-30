#pragma once

#include <cstdint>
#include <string>

#include "../cpu.h"
#include "../mem/bus.h"
#include "dbt_helper_replay.h"

enum class DbtHelperExecutionKind : uint8_t {
    None,
    ScalarMemoryLoad,
    ScalarMemoryStore,
    CsrWrite,
    AtomicMemory,
    VectorConfig,
    VectorMemoryLoad,
    VectorMemoryStore,
    VectorAlu,
};

struct DbtHelperExecutionRequest {
    bool ok{false};
    std::string reject_reason{};
    DbtHelperExecutionKind kind{DbtHelperExecutionKind::None};
    DbtHelperReplayKind replay_kind{DbtHelperReplayKind::None};
    DbtHelperKind helper_kind{DbtHelperKind::None};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t rd{0};
    uint64_t addr{0};
    uint8_t size{0};
    bool sign_extend{false};
    uint32_t csr_addr{0};
    uint64_t value{0};
    DbtAtomicHelperOp atomic_op{DbtAtomicHelperOp::None};
    bool atomic_aq{false};
    bool atomic_rl{false};
    DbtVectorHelperOp vector_op{DbtVectorHelperOp::None};
    uint8_t vector_vs1{0};
    uint8_t vector_vs2{0};
    uint8_t vector_sew_bytes{0};
    uint8_t vector_vl{0};
    bool reads_memory{false};
    bool writes_memory{false};
    bool writes_gpr{false};
    bool writes_csr{false};
    bool writes_vector{false};
    bool changes_vector_config{false};
    bool may_trap{false};
    bool may_change_platform_state{false};
    bool requires_commit_boundary{false};
    bool non_speculative{false};
    bool serializing{false};
    bool fallback_to_reference_on_trap{false};
    bool dry_run_only{true};
    bool executed_helper{false};
    bool mutates_cpu_state{false};
};

struct DbtHelperExecutionResult {
    bool ok{false};
    std::string reject_reason{};
    DbtHelperExecutionKind kind{DbtHelperExecutionKind::None};
    DbtHelperReplayKind replay_kind{DbtHelperReplayKind::None};
    uint64_t pc{0};
    uint64_t next_pc{0};
    uint64_t addr{0};
    uint8_t size{0};
    uint8_t rd{0};
    uint64_t value{0};
    bool sign_extend{false};
    bool dry_run_only{false};
    bool executed_helper{false};
    bool mutated_cpu_state{false};
    bool retired{false};
    bool trap_taken{false};
    uint64_t trap_cause{0};
    uint64_t trap_tval{0};
    bool fallback_to_reference_on_trap{false};
    bool platform_state_changed{false};
    bool commit_boundary{false};
    bool serializing{false};
};

DbtHelperExecutionRequest plan_dbt_helper_execution_bridge(
    const DbtHelperReplayPlan& replay);
DbtHelperExecutionResult execute_dbt_helper_request(
    CPU& cpu,
    Bus& bus,
    const DbtHelperExecutionRequest& request);

const char* dbt_helper_execution_kind_name(DbtHelperExecutionKind kind);
std::string format_dbt_helper_execution_request(
    const DbtHelperExecutionRequest& request);
std::string format_dbt_helper_execution_result(
    const DbtHelperExecutionResult& result);

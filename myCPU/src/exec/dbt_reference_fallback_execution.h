#pragma once

#include <cstdint>
#include <string>

#include "dbt_reference_fallback.h"

enum class DbtReferenceFallbackExecutionKind : uint8_t {
    None,
    ReferenceStep,
    HelperBridgeReferenceStep,
    JitMissReferenceStep,
    TrapOrFaultReferenceStep,
};

struct DbtReferenceFallbackExecutionRequest {
    bool ok{false};
    DbtReferenceFallbackExecutionKind kind{DbtReferenceFallbackExecutionKind::None};
    DbtJitDryRunSource source{DbtJitDryRunSource::None};
    uint64_t pc{0};
    uint64_t end_pc{0};
    uint64_t reference_step_count{0};
    std::string reference_backend{};
    bool will_call_reference_backend{false};
    bool requires_helper_bridge{false};
    bool trap_or_fault_placeholder{false};
    bool dry_run_only{true};
    bool executed_helper{false};
    bool executed_reference_step{false};
    bool mutates_cpu_state{false};
    bool executed_guest_code{false};
    bool helper_may_trap{false};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    DbtHelperReplayKind helper_replay_kind{DbtHelperReplayKind::None};
};

DbtReferenceFallbackExecutionRequest plan_dbt_reference_fallback_execution(
    const DbtReferenceFallbackPlan& plan);

const char* dbt_reference_fallback_execution_kind_name(
    DbtReferenceFallbackExecutionKind kind);
std::string format_dbt_reference_fallback_execution_request(
    const DbtReferenceFallbackExecutionRequest& request);

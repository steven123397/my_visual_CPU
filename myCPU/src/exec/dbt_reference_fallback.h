#pragma once

#include <cstdint>
#include <string>

#include "dbt_runtime_dispatch.h"

enum class DbtReferenceFallbackKind : uint8_t {
    None,
    ReferenceStep,
    HelperBridgeReferenceStep,
};

struct DbtReferenceFallbackPlan {
    bool ok{false};
    DbtReferenceFallbackKind kind{DbtReferenceFallbackKind::None};
    DbtJitDryRunSource source{DbtJitDryRunSource::None};
    uint64_t pc{0};
    uint64_t end_pc{0};
    uint64_t reference_step_count{0};
    std::string reference_backend{};
    bool reference_step_required{false};
    bool requires_helper_bridge{false};
    bool dry_run_only{true};
    bool executed_helper{false};
    bool executed_reference_step{false};
    bool mutates_cpu_state{false};
    bool generated_host_code{false};
    bool requested_executable_memory{false};
    bool executed_guest_code{false};
    bool helper_commit_at_boundary{false};
    bool helper_serializing{false};
    bool helper_may_trap{false};
    bool helper_may_change_platform_state{false};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    DbtHelperReplayKind helper_replay_kind{DbtHelperReplayKind::None};
};

DbtReferenceFallbackPlan plan_dbt_reference_fallback_step(
    const DbtRuntimeDispatchContract& contract);

const char* dbt_reference_fallback_kind_name(DbtReferenceFallbackKind kind);
std::string format_dbt_reference_fallback_plan(const DbtReferenceFallbackPlan& plan);

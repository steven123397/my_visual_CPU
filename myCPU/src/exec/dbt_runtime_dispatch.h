#pragma once

#include <cstdint>
#include <string>

#include "dbt_helper_replay.h"
#include "dbt_ir.h"
#include "dbt_jit_engine.h"

enum class DbtRuntimeDispatchKind : uint8_t {
    None,
    LoweredBlock,
    HelperBridgeToReference,
    ReferenceStep,
};

struct DbtRuntimeDispatchContract {
    bool ok{false};
    DbtRuntimeDispatchKind kind{DbtRuntimeDispatchKind::None};
    DbtJitDryRunSource source{DbtJitDryRunSource::None};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    bool used_cache{false};
    bool inserted_cache{false};
    bool planned{false};
    bool translated{false};
    bool lowered{false};
    bool can_enter_lowered_block{false};
    bool requires_helper_bridge{false};
    bool reference_step_required{false};
    bool dry_run_only{true};
    bool mutates_cpu_state{false};
    bool generated_host_code{false};
    bool requested_executable_memory{false};
    bool executed_guest_code{false};
    bool helper_commit_at_boundary{false};
    bool helper_serializing{false};
    bool helper_may_trap{false};
    bool helper_may_change_platform_state{false};
    uint64_t lowered_instruction_count{0};
    DbtRejectKind reject_kind{DbtRejectKind::None};
    uint64_t reject_pc{0};
    uint32_t reject_raw{0};
    std::string reject_reason{};
    DbtHelperReplayKind helper_replay_kind{DbtHelperReplayKind::None};
};

DbtRuntimeDispatchContract plan_dbt_runtime_dispatch_contract(
    const DbtJitDryRunResult& result);

const char* dbt_runtime_dispatch_kind_name(DbtRuntimeDispatchKind kind);
std::string format_dbt_runtime_dispatch_contract(
    const DbtRuntimeDispatchContract& contract);

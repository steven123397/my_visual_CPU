#pragma once

#include <cstdint>
#include <string>

#include "../cpu.h"
#include "../mem/bus.h"
#include "dbt_block_cache.h"
#include "dbt_block_plan.h"
#include "dbt_helper_replay.h"
#include "dbt_ir.h"
#include "dbt_ir_lowering.h"

enum class DbtJitDryRunAction : uint8_t {
    None,
    LoweredReady,
    HelperBridgeRequired,
    ReferenceFallback,
};

enum class DbtJitDryRunSource : uint8_t {
    None,
    ExplicitBlock,
    HotPathProfile,
};

struct DbtJitEngineDryRunStats {
    uint64_t requests{0};
    uint64_t profile_requests{0};
    uint64_t profile_no_candidates{0};
    uint64_t cache_hits{0};
    uint64_t cache_misses{0};
    uint64_t planned_blocks{0};
    uint64_t translations{0};
    uint64_t lowerings{0};
    uint64_t lowered_ready{0};
    uint64_t helper_bridges{0};
    uint64_t reference_fallbacks{0};
    uint64_t cache_insertions{0};
};

struct DbtJitDryRunResult {
    bool ok{false};
    DbtJitDryRunAction action{DbtJitDryRunAction::None};
    DbtJitDryRunSource source{DbtJitDryRunSource::None};
    uint64_t start_pc{0};
    uint64_t end_pc{0};
    bool used_cache{false};
    bool inserted_cache{false};
    bool planned{false};
    bool translated{false};
    bool lowered{false};
    bool fallback_to_reference{false};
    bool generated_host_code{false};
    bool requested_executable_memory{false};
    bool executed_guest_code{false};
    DbtBlockPlan block_plan{};
    DbtTranslationUnit translation{};
    DbtIrLoweringResult lowering{};
    DbtHelperReplayPlan helper_replay{};
};

struct DbtJitDryRunSummary {
    bool ok{false};
    std::string source{};
    std::string action{};
    std::string start_pc{};
    std::string end_pc{};
    std::string cache_state{};
    bool planned{false};
    bool translated{false};
    bool lowered{false};
    bool fallback_to_reference{false};
    bool generated_host_code{false};
    bool requested_executable_memory{false};
    bool executed_guest_code{false};
    uint64_t lowered_instruction_count{0};
    uint64_t candidate_executions{0};
    uint64_t candidate_retired_instructions{0};
    std::string reject_kind{};
    std::string reject_reason{};
    std::string helper_replay_kind{};
};

class DbtJitEngineDryRun {
public:
    DbtJitDryRunResult dry_run_block(CPU& cpu, Bus& bus, uint64_t start_pc, uint64_t end_pc);
    DbtJitDryRunResult dry_run_hot_path(CPU& cpu,
                                        Bus& bus,
                                        const ExecutionProfileSnapshot& profile);

    DbtBlockCache& cache();
    const DbtBlockCache& cache() const;
    DbtJitEngineDryRunStats stats() const;
    void clear();

private:
    DbtJitDryRunResult lower_translation_unit(const DbtTranslationUnit& unit,
                                              uint64_t start_pc,
                                              uint64_t end_pc,
                                              bool used_cache,
                                              DbtJitDryRunSource source);
    DbtJitDryRunResult bridge_rejected_unit(const DbtTranslationUnit& unit,
                                            const DbtBlockPlan& plan,
                                            uint64_t start_pc,
                                            uint64_t end_pc,
                                            DbtJitDryRunSource source);

    DbtBlockCache cache_{};
    DbtJitEngineDryRunStats stats_{};
};

const char* dbt_jit_dry_run_action_name(DbtJitDryRunAction action);
const char* dbt_jit_dry_run_source_name(DbtJitDryRunSource source);
DbtJitDryRunSummary summarize_dbt_jit_dry_run(const DbtJitDryRunResult& result);
std::string format_dbt_jit_dry_run_summary(const DbtJitDryRunSummary& summary);

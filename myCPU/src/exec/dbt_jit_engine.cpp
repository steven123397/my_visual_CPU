#include "dbt_jit_engine.h"

#include <cstdio>
#include <sstream>

#include "dbt_translator.h"

namespace {

DbtJitDryRunResult base_result(uint64_t start_pc,
                                uint64_t end_pc,
                                DbtJitDryRunSource source) {
    return DbtJitDryRunResult{
        .source = source,
        .start_pc = start_pc,
        .end_pc = end_pc,
    };
}

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

std::string cache_state_name(const DbtJitDryRunResult& result) {
    if (result.used_cache) {
        return "hit";
    }
    if (result.inserted_cache) {
        return "miss-inserted";
    }
    return "miss";
}

}  // namespace

DbtJitDryRunResult DbtJitEngineDryRun::dry_run_block(CPU& cpu,
                                                     Bus& bus,
                                                     uint64_t start_pc,
                                                     uint64_t end_pc) {
    stats_.requests += 1;

    if (const DbtTranslationUnit* cached = cache_.lookup(start_pc, end_pc)) {
        stats_.cache_hits += 1;
        return lower_translation_unit(*cached,
                                      start_pc,
                                      end_pc,
                                      true,
                                      DbtJitDryRunSource::ExplicitBlock);
    }

    stats_.cache_misses += 1;
    stats_.planned_blocks += 1;
    DbtBlockPlan plan = plan_dbt_block(cpu, bus, start_pc, end_pc);

    stats_.translations += 1;
    DbtTranslationUnit unit = translate_dbt_block(plan);
    if (!unit.ok) {
        return bridge_rejected_unit(unit,
                                    plan,
                                    start_pc,
                                    end_pc,
                                    DbtJitDryRunSource::ExplicitBlock);
    }

    DbtJitDryRunResult result = lower_translation_unit(unit,
                                                       start_pc,
                                                       end_pc,
                                                       false,
                                                       DbtJitDryRunSource::ExplicitBlock);
    result.block_plan = plan;
    result.translation = unit;
    result.planned = true;
    result.translated = true;
    if (result.ok && cache_.insert(unit)) {
        result.inserted_cache = true;
        stats_.cache_insertions += 1;
    }
    return result;
}

DbtJitDryRunResult DbtJitEngineDryRun::dry_run_hot_path(
    CPU& cpu,
    Bus& bus,
    const ExecutionProfileSnapshot& profile) {
    stats_.requests += 1;
    stats_.profile_requests += 1;
    stats_.planned_blocks += 1;

    DbtBlockPlan plan = plan_dbt_hot_path(cpu, bus, profile);
    if (plan.fallback_reason == "no-hot-paths") {
        stats_.profile_no_candidates += 1;
    }

    if (plan.ok) {
        if (const DbtTranslationUnit* cached = cache_.lookup(plan.start_pc, plan.end_pc)) {
            stats_.cache_hits += 1;
            DbtJitDryRunResult result = lower_translation_unit(*cached,
                                                              plan.start_pc,
                                                              plan.end_pc,
                                                              true,
                                                              DbtJitDryRunSource::HotPathProfile);
            result.block_plan = plan;
            result.planned = true;
            return result;
        }
        stats_.cache_misses += 1;
    }

    stats_.translations += 1;
    DbtTranslationUnit unit = translate_dbt_block(plan);
    if (!unit.ok) {
        return bridge_rejected_unit(unit,
                                    plan,
                                    plan.start_pc,
                                    plan.end_pc,
                                    DbtJitDryRunSource::HotPathProfile);
    }

    DbtJitDryRunResult result = lower_translation_unit(unit,
                                                       plan.start_pc,
                                                       plan.end_pc,
                                                       false,
                                                       DbtJitDryRunSource::HotPathProfile);
    result.block_plan = plan;
    result.translation = unit;
    result.planned = true;
    result.translated = true;
    if (result.ok && cache_.insert(unit)) {
        result.inserted_cache = true;
        stats_.cache_insertions += 1;
    }
    return result;
}

DbtBlockCache& DbtJitEngineDryRun::cache() {
    return cache_;
}

const DbtBlockCache& DbtJitEngineDryRun::cache() const {
    return cache_;
}

DbtJitEngineDryRunStats DbtJitEngineDryRun::stats() const {
    return stats_;
}

void DbtJitEngineDryRun::clear() {
    cache_ = {};
    stats_ = {};
}

DbtJitDryRunResult DbtJitEngineDryRun::lower_translation_unit(const DbtTranslationUnit& unit,
                                                              uint64_t start_pc,
                                                              uint64_t end_pc,
                                                              bool used_cache,
                                                              DbtJitDryRunSource source) {
    stats_.lowerings += 1;
    DbtJitDryRunResult result = base_result(start_pc, end_pc, source);
    result.used_cache = used_cache;
    result.translation = unit;
    result.lowering = lower_dbt_ir_unit(unit);
    result.lowered = true;

    if (result.lowering.ok) {
        result.ok = true;
        result.action = DbtJitDryRunAction::LoweredReady;
        stats_.lowered_ready += 1;
        return result;
    }

    result.ok = false;
    result.action = DbtJitDryRunAction::ReferenceFallback;
    result.fallback_to_reference = true;
    stats_.reference_fallbacks += 1;
    return result;
}

DbtJitDryRunResult DbtJitEngineDryRun::bridge_rejected_unit(const DbtTranslationUnit& unit,
                                                            const DbtBlockPlan& plan,
                                                            uint64_t start_pc,
                                                            uint64_t end_pc,
                                                            DbtJitDryRunSource source) {
    DbtJitDryRunResult result = base_result(start_pc, end_pc, source);
    result.block_plan = plan;
    result.translation = unit;
    result.planned = true;
    result.translated = true;
    result.fallback_to_reference = true;

    result.helper_replay = plan_dbt_helper_replay(unit);
    if (result.helper_replay.ok) {
        result.action = DbtJitDryRunAction::HelperBridgeRequired;
        stats_.helper_bridges += 1;
        return result;
    }

    result.action = DbtJitDryRunAction::ReferenceFallback;
    stats_.reference_fallbacks += 1;
    return result;
}

const char* dbt_jit_dry_run_action_name(DbtJitDryRunAction action) {
    switch (action) {
    case DbtJitDryRunAction::None:
        return "none";
    case DbtJitDryRunAction::LoweredReady:
        return "lowered-ready";
    case DbtJitDryRunAction::HelperBridgeRequired:
        return "helper-bridge-required";
    case DbtJitDryRunAction::ReferenceFallback:
        return "reference-fallback";
    }
    return "unknown";
}

const char* dbt_jit_dry_run_source_name(DbtJitDryRunSource source) {
    switch (source) {
    case DbtJitDryRunSource::None:
        return "none";
    case DbtJitDryRunSource::ExplicitBlock:
        return "explicit-block";
    case DbtJitDryRunSource::HotPathProfile:
        return "hot-path-profile";
    }
    return "unknown";
}

DbtJitDryRunSummary summarize_dbt_jit_dry_run(const DbtJitDryRunResult& result) {
    return DbtJitDryRunSummary{
        .ok = result.ok,
        .source = dbt_jit_dry_run_source_name(result.source),
        .action = dbt_jit_dry_run_action_name(result.action),
        .start_pc = hex_u64(result.start_pc),
        .end_pc = hex_u64(result.end_pc),
        .cache_state = cache_state_name(result),
        .planned = result.planned,
        .translated = result.translated,
        .lowered = result.lowered,
        .fallback_to_reference = result.fallback_to_reference,
        .generated_host_code = result.generated_host_code,
        .requested_executable_memory = result.requested_executable_memory,
        .executed_guest_code = result.executed_guest_code,
        .lowered_instruction_count = result.lowering.instructions.size(),
        .candidate_executions = result.block_plan.candidate_executions,
        .candidate_retired_instructions = result.block_plan.candidate_retired_instructions,
        .reject_kind = dbt_reject_kind_name(result.translation.reject_kind),
        .reject_reason = result.translation.reject_reason.empty() ? "none" : result.translation.reject_reason,
        .helper_replay_kind = dbt_helper_replay_kind_name(result.helper_replay.kind),
    };
}

std::string format_dbt_jit_dry_run_summary(const DbtJitDryRunSummary& summary) {
    std::ostringstream out;
    out << "jit-dispatch:"
        << " source=" << summary.source
        << " action=" << summary.action
        << " ok=" << bool_name(summary.ok)
        << " start=" << summary.start_pc
        << " end=" << summary.end_pc
        << " cache=" << summary.cache_state
        << " planned=" << bool_name(summary.planned)
        << " translated=" << bool_name(summary.translated)
        << " lowered=" << bool_name(summary.lowered)
        << " fallback=" << bool_name(summary.fallback_to_reference)
        << " lowered-ops=" << summary.lowered_instruction_count
        << " candidate-executions=" << summary.candidate_executions
        << " candidate-retired=" << summary.candidate_retired_instructions
        << " reject=" << summary.reject_kind
        << " reason=" << summary.reject_reason
        << " helper=" << summary.helper_replay_kind
        << " host-code=" << bool_name(summary.generated_host_code)
        << " exec-mem=" << bool_name(summary.requested_executable_memory)
        << " guest-exec=" << bool_name(summary.executed_guest_code);
    return out.str();
}

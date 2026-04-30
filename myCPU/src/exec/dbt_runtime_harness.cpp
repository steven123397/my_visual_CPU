#include "dbt_runtime_harness.h"

#include <array>
#include <cstdio>
#include <sstream>

#include "dbt_block_plan.h"
#include "dbt_host_emitter.h"
#include "dbt_ir_eval.h"
#include "dbt_ir_lowering.h"
#include "dbt_jit_engine.h"
#include "dbt_runtime_dispatch.h"
#include "dbt_translator.h"

namespace {

const char* bool_name(bool value) {
    return value ? "true" : "false";
}

std::array<uint64_t, 32> snapshot_gprs(const CPU& cpu) {
    std::array<uint64_t, 32> gpr{};
    for (uint32_t i = 0; i < gpr.size(); ++i) {
        gpr[i] = cpu.core().read_gpr(i);
    }
    return gpr;
}

bool gprs_equal(const std::array<uint64_t, 32>& lhs, const std::array<uint64_t, 32>& rhs) {
    for (uint32_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

void commit_gprs(CPU& cpu, const std::array<uint64_t, 32>& gpr) {
    for (uint32_t i = 1; i < gpr.size(); ++i) {
        cpu.core().write_gpr(i, gpr[i]);
    }
}

std::string reject_reason_or_default(const std::string& reason, const char* fallback) {
    return reason.empty() ? fallback : reason;
}

DbtRuntimeHarnessResult reject_from_translation(const DbtTranslationUnit& unit,
                                                uint64_t start_pc,
                                                uint64_t end_pc) {
    return DbtRuntimeHarnessResult{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .reject_kind = unit.reject_kind,
        .reject_pc = unit.reject_pc,
        .reject_raw = unit.reject_raw,
        .reject_reason = reject_reason_or_default(unit.reject_reason, "translation-rejected"),
        .fallback_required = true,
    };
}

DbtRuntimeHarnessResult reject_from_lowering(const DbtIrLoweringResult& lowering,
                                             uint64_t start_pc,
                                             uint64_t end_pc) {
    return DbtRuntimeHarnessResult{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .reject_kind = lowering.reject_kind,
        .reject_pc = lowering.reject_pc,
        .reject_raw = lowering.reject_raw,
        .reject_reason = reject_reason_or_default(lowering.reject_reason, "lowering-rejected"),
        .fallback_required = true,
    };
}

DbtRuntimeHarnessResult reject_from_emitter(const DbtHostExecutable& executable,
                                            uint64_t start_pc,
                                            uint64_t end_pc) {
    return DbtRuntimeHarnessResult{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .reject_kind = executable.reject_kind,
        .reject_pc = executable.reject_pc,
        .reject_raw = executable.reject_raw,
        .reject_reason = reject_reason_or_default(executable.reject_reason, "host-emission-rejected"),
        .fallback_required = true,
    };
}

DbtRuntimeHarnessResult reject_from_cache_contract(const DbtRuntimeDispatchContract& contract,
                                                   uint64_t start_pc,
                                                   uint64_t end_pc) {
    return DbtRuntimeHarnessResult{
        .ok = false,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .reject_kind = contract.reject_kind,
        .reject_pc = contract.reject_pc,
        .reject_raw = contract.reject_raw,
        .reject_reason = reject_reason_or_default(contract.reject_reason,
                                                  "executable-cache-contract-rejected"),
        .fallback_required = true,
    };
}

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

DbtRuntimeHarnessResult execute_with_guardrail(CPU& cpu,
                                               const DbtTranslationUnit& unit,
                                               const DbtHostExecutable& executable,
                                               const std::array<uint64_t, 32>& input_gpr,
                                               uint64_t input_pc,
                                               uint64_t start_pc,
                                               uint64_t end_pc,
                                               bool used_cache,
                                               bool inserted_cache,
                                               bool emitted_on_miss = false,
                                               bool executed_on_hit = false) {
    const DbtIrEvaluationResult expected =
        evaluate_dbt_ir_unit(unit, DbtIrEvaluationInput{
                                       .gpr = input_gpr,
                                       .pc = input_pc,
                                   });
    if (!expected.ok) {
        return DbtRuntimeHarnessResult{
            .ok = false,
            .start_pc = start_pc,
            .end_pc = end_pc,
            .reject_kind = expected.reject_kind,
            .reject_reason = reject_reason_or_default(expected.reject_reason, "ir-eval-rejected"),
            .fallback_required = true,
            .used_executable_cache = used_cache,
            .inserted_executable_cache = inserted_cache,
            .cache_lookup = false,
            .cache_hit = used_cache,
            .cache_miss = emitted_on_miss,
            .emitted_on_miss = emitted_on_miss,
            .executed_on_hit = executed_on_hit,
        };
    }

    std::array<uint64_t, 32> output_gpr = input_gpr;
    const uint64_t next_pc = execute_dbt_host_block(executable, output_gpr.data(), input_pc);
    output_gpr[0] = 0;

    const bool differential_matched =
        expected.next_pc == next_pc &&
        expected.retired_instructions + 1 == unit.instructions.size() &&
        gprs_equal(expected.gpr, output_gpr);

    DbtRuntimeHarnessResult result{
        .ok = differential_matched,
        .start_pc = start_pc,
        .end_pc = end_pc,
        .fallback_required = !differential_matched,
        .executed_host_code = true,
        .used_executable_memory = executable.requested_executable_memory,
        .used_executable_cache = used_cache,
        .inserted_executable_cache = inserted_cache,
        .cache_lookup = false,
        .cache_hit = used_cache,
        .cache_miss = emitted_on_miss,
        .emitted_on_miss = emitted_on_miss,
        .executed_on_hit = executed_on_hit,
        .executed_guest_code = true,
        .mutated_cpu_state = differential_matched,
        .differential_checked = true,
        .differential_matched = differential_matched,
        .next_pc = next_pc,
        .retired_instructions = expected.retired_instructions,
    };

    if (differential_matched) {
        commit_gprs(cpu, output_gpr);
        cpu.core().set_pc(next_pc);
        cpu.core().advance_instret(expected.retired_instructions);
    } else {
        result.reject_kind = DbtRejectKind::UnsupportedIr;
        result.reject_reason = "differential-mismatch";
    }

    return result;
}

}  // namespace

DbtRuntimeHarnessResult run_dbt_runtime_harness_block(CPU& cpu,
                                                      Bus& bus,
                                                      uint64_t start_pc,
                                                      uint64_t end_pc) {
    const std::array<uint64_t, 32> input_gpr = snapshot_gprs(cpu);
    const uint64_t input_pc = cpu.core().pc();

    DbtBlockPlan plan = plan_dbt_block(cpu, bus, start_pc, end_pc);
    DbtTranslationUnit unit = translate_dbt_block(plan);
    if (!unit.ok) {
        return reject_from_translation(unit, start_pc, end_pc);
    }

    const DbtIrLoweringResult lowering = lower_dbt_ir_unit(unit);
    if (!lowering.ok) {
        return reject_from_lowering(lowering, start_pc, end_pc);
    }

    DbtHostExecutable executable = emit_dbt_host_block(lowering);
    if (!executable.ok) {
        DbtRuntimeHarnessResult rejected = reject_from_emitter(executable, start_pc, end_pc);
        release_dbt_host_executable(executable);
        return rejected;
    }

    DbtRuntimeHarnessResult result = execute_with_guardrail(cpu,
                                                            unit,
                                                            executable,
                                                            input_gpr,
                                                            input_pc,
                                                            start_pc,
                                                            end_pc,
                                                            false,
                                                            false);

    release_dbt_host_executable(executable);
    return result;
}

DbtRuntimeHarnessResult run_dbt_runtime_harness_block_with_cache(
    CPU& cpu,
    Bus& bus,
    DbtExecutableCacheRuntime& cache,
    uint64_t start_pc,
    uint64_t end_pc) {
    const std::array<uint64_t, 32> input_gpr = snapshot_gprs(cpu);
    const uint64_t input_pc = cpu.core().pc();

    DbtExecutableCacheLookup cached = cache.lookup(start_pc, end_pc);
    if (cached.hit && cached.has_host_executable && cached.executable != nullptr) {
        DbtJitEngineDryRun engine;
        const DbtJitDryRunResult dry_run = engine.dry_run_block(cpu, bus, start_pc, end_pc);
        const DbtRuntimeDispatchContract contract =
            plan_dbt_runtime_dispatch_contract(dry_run);
        if (!contract.ok || contract.kind != DbtRuntimeDispatchKind::LoweredBlock) {
            DbtRuntimeHarnessResult rejected =
                reject_from_cache_contract(contract, start_pc, end_pc);
            rejected.cache_lookup = true;
            rejected.cache_hit = true;
            return rejected;
        }
        DbtRuntimeHarnessResult result =
            execute_with_guardrail(cpu,
                                   dry_run.translation,
                                   *cached.executable,
                                   input_gpr,
                                   input_pc,
                                   start_pc,
                                   end_pc,
                                   true,
                                   false,
                                   false,
                                   true);
        result.cache_lookup = true;
        result.cache_hit = true;
        return result;
    }

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult dry_run = engine.dry_run_block(cpu, bus, start_pc, end_pc);
    const DbtRuntimeDispatchContract contract =
        plan_dbt_runtime_dispatch_contract(dry_run);
    if (!contract.ok || contract.kind != DbtRuntimeDispatchKind::LoweredBlock) {
        DbtRuntimeHarnessResult rejected =
            reject_from_cache_contract(contract, start_pc, end_pc);
        rejected.cache_lookup = true;
        rejected.cache_miss = true;
        return rejected;
    }

    DbtHostExecutable executable = emit_dbt_host_block(dry_run.lowering);
    if (!executable.ok) {
        DbtRuntimeHarnessResult rejected = reject_from_emitter(executable, start_pc, end_pc);
        release_dbt_host_executable(executable);
        rejected.cache_lookup = true;
        rejected.cache_miss = true;
        rejected.emitted_on_miss = true;
        return rejected;
    }

    DbtRuntimeHarnessResult result = execute_with_guardrail(cpu,
                                                            dry_run.translation,
                                                            executable,
                                                            input_gpr,
                                                            input_pc,
                                                            start_pc,
                                                            end_pc,
                                                            false,
                                                            false,
                                                            true,
                                                            false);
    result.cache_lookup = true;
    result.cache_miss = true;
    if (!result.ok) {
        release_dbt_host_executable(executable);
        return result;
    }

    const bool inserted = cache.insert(contract, executable);
    result.inserted_executable_cache = inserted;
    if (!inserted) {
        release_dbt_host_executable(executable);
    }

    return result;
}

bool dbt_runtime_harness_is_default_enabled() {
    return false;
}

std::string format_dbt_runtime_harness_result(const DbtRuntimeHarnessResult& result) {
    std::ostringstream out;
    out << "runtime-harness:"
        << " ok=" << bool_name(result.ok)
        << " start=" << hex_u64(result.start_pc)
        << " end=" << hex_u64(result.end_pc)
        << " next-pc=" << hex_u64(result.next_pc)
        << " retired=" << result.retired_instructions
        << " fallback=" << bool_name(result.fallback_required)
        << " differential-checked=" << bool_name(result.differential_checked)
        << " differential-matched=" << bool_name(result.differential_matched)
        << " mutates-state=" << bool_name(result.mutated_cpu_state)
        << " host-code=" << bool_name(result.executed_host_code)
        << " exec-mem=" << bool_name(result.used_executable_memory)
        << " guest-exec=" << bool_name(result.executed_guest_code)
        << " exec-cache=" << (result.used_executable_cache ? "hit" : (result.inserted_executable_cache ? "inserted" : "none"))
        << " reject=" << dbt_reject_kind_name(result.reject_kind)
        << " reason=" << (result.reject_reason.empty() ? "none" : result.reject_reason);
    return out.str();
}

void record_dbt_runtime_harness_result(DbtRuntimeHarnessStats& stats,
                                       const DbtRuntimeHarnessResult& result) {
    stats.dispatches += 1;
    if (result.cache_lookup) {
        stats.cache_lookups += 1;
    }
    if (result.cache_hit) {
        stats.cache_hits += 1;
    }
    if (result.cache_miss) {
        stats.cache_misses += 1;
    }
    if (result.emitted_on_miss) {
        stats.host_emits += 1;
    }
    if (result.executed_host_code || result.executed_on_hit) {
        stats.host_executes += 1;
    }
    if (result.fallback_required) {
        stats.fallbacks += 1;
    }
    if (result.differential_checked) {
        stats.differential_checks += 1;
        if (!result.differential_matched) {
            stats.differential_mismatches += 1;
        }
    }
}

void record_dbt_runtime_invalidation_result(DbtRuntimeHarnessStats& stats,
                                            const DbtRuntimeInvalidationHookResult& result) {
    if (result.invalidated) {
        stats.invalidations += 1;
    }
    if (result.stale_dispatch_prevented) {
        stats.stale_dispatches_prevented += 1;
    }
}

std::string format_dbt_runtime_harness_stats(const DbtRuntimeHarnessStats& stats) {
    std::ostringstream out;
    out << "runtime-harness-stats:"
        << " dispatches=" << stats.dispatches
        << " lookups=" << stats.cache_lookups
        << " hits=" << stats.cache_hits
        << " misses=" << stats.cache_misses
        << " emits=" << stats.host_emits
        << " exec=" << stats.host_executes
        << " fallback=" << stats.fallbacks
        << " invalidate=" << stats.invalidations
        << " stale-prevented=" << stats.stale_dispatches_prevented
        << " differential-checks=" << stats.differential_checks
        << " differential-mismatch=" << stats.differential_mismatches;
    return out.str();
}

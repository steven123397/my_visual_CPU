#include "dbt_runtime_harness.h"

#include <array>
#include <cstdio>
#include <sstream>

#include "dbt_block_plan.h"
#include "dbt_host_emitter.h"
#include "dbt_helper_execution_bridge.h"
#include "dbt_helper_replay.h"
#include "dbt_ir_eval.h"
#include "dbt_ir_lowering.h"
#include "dbt_jit_engine.h"
#include "dbt_reference_fallback.h"
#include "dbt_reference_fallback_execution.h"
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

DbtRuntimeLoopStepResult make_loop_error(uint64_t pc, const std::string& reason) {
    return DbtRuntimeLoopStepResult{
        .ok = false,
        .pc = pc,
        .next_pc = pc,
        .reason = reason,
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
        .ir_expected_retired_count = expected.retired_instructions,
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

DbtRuntimeLoopResult run_dbt_runtime_harness_loop(
    CPU& cpu,
    Bus& bus,
    DbtExecutableCacheRuntime& cache,
    const DbtRuntimeLoopRequest& request) {
    DbtRuntimeLoopResult loop{};
    loop.steps_requested = request.max_steps;
    loop.default_backend_enabled = dbt_runtime_harness_is_default_enabled();

    for (uint64_t step = 0; step < request.max_steps; ++step) {
        const uint64_t pc = cpu.core().pc();
        const uint64_t block_end = pc;
        DbtJitEngineDryRun engine;
        const DbtJitDryRunResult dry_run = engine.dry_run_block(cpu, bus, pc, block_end);
        const DbtRuntimeDispatchContract contract =
            plan_dbt_runtime_dispatch_contract(dry_run);

        if (contract.ok && contract.kind == DbtRuntimeDispatchKind::LoweredBlock &&
            request.enable_executable_cache) {
            const DbtRuntimeHarnessResult dispatch =
                run_dbt_runtime_harness_block_with_cache(cpu, bus, cache, pc, block_end);
            record_dbt_runtime_harness_result(loop.stats, dispatch);
            loop.steps.push_back(DbtRuntimeLoopStepResult{
                .ok = dispatch.ok,
                .kind = DbtRuntimeLoopStepKind::HostExecutable,
                .pc = pc,
                .next_pc = dispatch.next_pc,
                .cache_hit = dispatch.cache_hit,
                .cache_miss = dispatch.cache_miss,
                .emitted_on_miss = dispatch.emitted_on_miss,
                .reason = dispatch.reject_reason.empty() ? "none" : dispatch.reject_reason,
            });
            if (!dispatch.ok) {
                loop.stopped_on_error = true;
                loop.stop_reason = dispatch.reject_reason.empty()
                                       ? "host-dispatch-failed"
                                       : dispatch.reject_reason;
                break;
            }
            loop.steps_executed += 1;
            loop.host_executions += 1;
            continue;
        }

        if (contract.ok &&
            contract.kind == DbtRuntimeDispatchKind::HelperBridgeToReference &&
            request.enable_helper_execution) {
            DbtHelperExecutionRequest helper_request =
                plan_dbt_helper_execution_bridge(dry_run.helper_replay);
            const DbtHelperExecutionResult helper =
                execute_dbt_helper_request(cpu, bus, helper_request);

            DbtRuntimeLoopStepResult step_result{
                .ok = helper.ok,
                .kind = DbtRuntimeLoopStepKind::HelperExecution,
                .pc = pc,
                .next_pc = helper.next_pc,
                .reason = helper.reject_reason.empty() ? "none" : helper.reject_reason,
            };

            if (helper.ok) {
                loop.steps_executed += 1;
                loop.helper_executions += 1;
                loop.stats.helper_executions += 1;
                if (request.apply_guest_store_invalidation &&
                    helper.kind == DbtHelperExecutionKind::ScalarMemoryStore) {
                    const DbtRuntimeInvalidationHookResult invalidation =
                        apply_dbt_runtime_invalidation_hook(
                            cache,
                            DbtRuntimeInvalidationEvent{
                                .kind = DbtInvalidationEventKind::GuestStore,
                                .addr = helper.addr,
                                .size = helper.size,
                            });
                    record_dbt_runtime_invalidation_result(loop.stats, invalidation);
                    step_result.invalidated_after_store = invalidation.invalidated;
                    step_result.stale_dispatch_prevented =
                        invalidation.stale_dispatch_prevented;
                    if (invalidation.invalidated) {
                        loop.invalidations += 1;
                    }
                    if (invalidation.stale_dispatch_prevented) {
                        loop.stale_dispatches_prevented += 1;
                    }
                }
                loop.steps.push_back(step_result);
                continue;
            }

            if (helper.trap_taken || helper.fallback_to_reference_on_trap) {
                if (!request.enable_reference_fallback) {
                    loop.steps.push_back(step_result);
                    loop.stopped_on_error = true;
                    loop.stop_reason = "helper-reference-fallback-disabled";
                    break;
                }
            } else {
                loop.steps.push_back(step_result);
                loop.stopped_on_error = true;
                loop.stop_reason = helper.reject_reason.empty()
                                       ? "helper-execution-failed"
                                       : helper.reject_reason;
                break;
            }
        }

        if (request.enable_reference_fallback &&
            (contract.ok &&
             (contract.kind == DbtRuntimeDispatchKind::ReferenceStep ||
              contract.kind == DbtRuntimeDispatchKind::HelperBridgeToReference))) {
            const DbtReferenceFallbackPlan fallback_plan =
                plan_dbt_reference_fallback_step(contract);
            const DbtReferenceFallbackExecutionRequest fallback_request =
                plan_dbt_reference_fallback_execution(fallback_plan);
            const DbtReferenceFallbackExecutionResult fallback =
                execute_dbt_reference_fallback(cpu, bus, fallback_request);
            loop.steps.push_back(DbtRuntimeLoopStepResult{
                .ok = fallback.ok,
                .kind = DbtRuntimeLoopStepKind::ReferenceFallback,
                .pc = pc,
                .next_pc = fallback.next_pc,
                .reason = fallback.reject_reason.empty() ? "none" : fallback.reject_reason,
            });
            if (!fallback.ok) {
                loop.stopped_on_error = true;
                loop.stop_reason = fallback.reject_reason.empty()
                                       ? "reference-fallback-failed"
                                       : fallback.reject_reason;
                break;
            }
            loop.steps_executed += fallback.reference_steps_executed;
            loop.reference_fallbacks += 1;
            loop.stats.reference_fallback_executions += 1;
            loop.stats.fallbacks += 1;
            continue;
        }

        loop.steps.push_back(make_loop_error(pc, "unsupported-runtime-loop-dispatch"));
        loop.stopped_on_error = true;
        loop.stop_reason = "unsupported-runtime-loop-dispatch";
        break;
    }

    loop.ok = !loop.stopped_on_error && loop.steps_executed == request.max_steps;
    if (loop.stop_reason.empty()) {
        loop.stop_reason = loop.ok ? "completed" : "max-steps-not-reached";
    }
    return loop;
}

std::string format_dbt_runtime_harness_result(const DbtRuntimeHarnessResult& result) {
    std::ostringstream out;
    out << "runtime-harness:"
        << " ok=" << bool_name(result.ok)
        << " start=" << hex_u64(result.start_pc)
        << " end=" << hex_u64(result.end_pc)
        << " next-pc=" << hex_u64(result.next_pc)
        << " ir-expected-retired=" << result.ir_expected_retired_count
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
        << " differential-mismatch=" << stats.differential_mismatches
        << " helper-exec=" << stats.helper_executions
        << " reference-fallback-exec=" << stats.reference_fallback_executions;
    return out.str();
}

const char* dbt_runtime_loop_step_kind_name(DbtRuntimeLoopStepKind kind) {
    switch (kind) {
    case DbtRuntimeLoopStepKind::None:
        return "none";
    case DbtRuntimeLoopStepKind::HostExecutable:
        return "host-executable";
    case DbtRuntimeLoopStepKind::HelperExecution:
        return "helper-execution";
    case DbtRuntimeLoopStepKind::ReferenceFallback:
        return "reference-fallback";
    }
    return "unknown";
}

std::string format_dbt_runtime_loop_result(const DbtRuntimeLoopResult& result) {
    std::ostringstream out;
    out << "runtime-loop:"
        << " ok=" << bool_name(result.ok)
        << " requested=" << result.steps_requested
        << " executed=" << result.steps_executed
        << " host=" << result.host_executions
        << " helper=" << result.helper_executions
        << " fallback=" << result.reference_fallbacks
        << " invalidate=" << result.invalidations
        << " stale-prevented=" << result.stale_dispatches_prevented
        << " stopped=" << bool_name(result.stopped_on_error)
        << " reason=" << (result.stop_reason.empty() ? "none" : result.stop_reason)
        << " backend-default=" << bool_name(result.default_backend_enabled)
        << " stats={" << format_dbt_runtime_harness_stats(result.stats) << "}";
    return out.str();
}

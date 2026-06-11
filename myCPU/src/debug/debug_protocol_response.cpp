#include "debug_protocol_response.h"

#include <cstdio>
#include <sstream>
#include <string>

#include "debug_protocol.h"

namespace {

const char* privilege_name(PrivilegeMode mode) {
    switch (mode) {
    case PrivilegeMode::User:
        return "U";
    case PrivilegeMode::Supervisor:
        return "S";
    case PrivilegeMode::Machine:
        return "M";
    default:
        return "?";
    }
}

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(ch)));
                escaped += buffer;
            } else {
                escaped.push_back(ch);
            }
            break;
        }
    }
    return escaped;
}

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

template <size_t N>
std::string hex_bytes(const std::array<uint8_t, N>& bytes) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(2 + N * 2);
    out += "0x";
    for (uint8_t byte : bytes) {
      out.push_back(kHexDigits[(byte >> 4) & 0x0fU]);
      out.push_back(kHexDigits[byte & 0x0fU]);
    }
    return out;
}

void append_json_string(std::ostringstream& out, const std::string& value) {
    out << '"' << json_escape(value) << '"';
}

void append_jit_dispatch_observation_event(std::ostringstream& out,
                                           const DbtJitDryRunSummary& summary) {
    out << "{"
        << "\"schema_version\":1"
        << ",\"event_id\":";
    append_json_string(out,
                       "jit-dbt-dispatch:" + summary.source + ":" + summary.action + ":" +
                           summary.start_pc + ":" + summary.end_pc);
    out << ",\"source\":\"jit-dbt-dispatch\""
        << ",\"phase\":\"dry-run\""
        << ",\"subject\":{"
        << "\"start_pc\":";
    append_json_string(out, summary.start_pc);
    out << ",\"end_pc\":";
    append_json_string(out, summary.end_pc);
    out << ",\"dispatch_source\":";
    append_json_string(out, summary.source);
    out << "}"
        << ",\"timestamp_or_step\":{"
        << "\"candidate_executions\":" << summary.candidate_executions
        << ",\"candidate_retired_instructions\":" << summary.candidate_retired_instructions
        << "}"
        << ",\"effect\":";
    append_json_string(out, summary.action);
    out << ",\"payload\":{"
        << "\"ok\":" << (summary.ok ? "true" : "false")
        << ",\"cache_state\":";
    append_json_string(out, summary.cache_state);
    out << ",\"planned\":" << (summary.planned ? "true" : "false")
        << ",\"translated\":" << (summary.translated ? "true" : "false")
        << ",\"lowered\":" << (summary.lowered ? "true" : "false")
        << ",\"fallback_to_reference\":" << (summary.fallback_to_reference ? "true" : "false")
        << ",\"lowered_instruction_count\":" << summary.lowered_instruction_count
        << ",\"reject\":{"
        << "\"kind\":";
    append_json_string(out, summary.reject_kind);
    out << ",\"reason\":";
    append_json_string(out, summary.reject_reason);
    out << "}"
        << ",\"helper_replay_kind\":";
    append_json_string(out, summary.helper_replay_kind);
    out << ",\"no_execution\":{"
        << "\"generated_host_code\":" << (summary.generated_host_code ? "true" : "false")
        << ",\"requested_executable_memory\":"
        << (summary.requested_executable_memory ? "true" : "false")
        << ",\"executed_guest_code\":" << (summary.executed_guest_code ? "true" : "false")
        << "}"
        << "}"
        << ",\"evidence_ref\":{"
        << "\"debug_json\":\"jit_dispatch\""
        << ",\"text_line\":\"jit-dispatch\""
        << "}"
        << "}";
}

void append_stage(std::ostringstream& out, const DebugStageSnapshot& stage) {
    out << "{"
        << "\"valid\":" << (stage.valid ? "true" : "false")
        << ",\"sequence_id\":" << stage.sequence_id
        << ",\"pc\":";
    append_json_string(out, hex_u64(stage.pc));
    out << ",\"raw\":";
    append_json_string(out, hex_u64(stage.raw));
    out << ",\"text\":";
    append_json_string(out, stage.text);
    out << "}";
}

void append_retire_trace(std::ostringstream& out, const std::vector<RetireTraceEntry>& trace) {
    out << "[";
    for (size_t i = 0; i < trace.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"sequence_id\":" << trace[i].sequence_id
            << ",\"pc\":";
        append_json_string(out, hex_u64(trace[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(trace[i].raw));
        out << ",\"trap\":" << (trace[i].trap ? "true" : "false")
            << ",\"redirect\":" << (trace[i].redirect ? "true" : "false")
            << "}";
    }
    out << "]";
}

void append_execution_profile_observation_event(std::ostringstream& out,
                                                const ExecutionProfileSnapshot& profile,
                                                const DebugSummarySnapshot& summary) {
    out << "{"
        << "\"schema_version\":1"
        << ",\"event_id\":";
    append_json_string(out,
                       "execution-profile:" + summary.backend + ":" +
                           std::to_string(summary.cycle) + ":" + hex_u64(summary.pc));
    out << ",\"source\":\"execution-profile\""
        << ",\"phase\":\"snapshot-summary\""
        << ",\"subject\":{"
        << "\"backend\":";
    append_json_string(out, summary.backend);
    out << ",\"pc\":";
    append_json_string(out, hex_u64(summary.pc));
    out << ",\"privilege\":";
    append_json_string(out, privilege_name(summary.privilege));
    out << "}"
        << ",\"timestamp_or_step\":{"
        << "\"cycle\":" << summary.cycle
        << ",\"instret\":" << summary.instret
        << "}"
        << ",\"effect\":\"observed\""
        << ",\"payload\":{"
        << "\"total_retirements\":" << profile.total_retirements
        << ",\"total_traps\":" << profile.total_traps
        << ",\"total_memory_observations\":" << profile.total_memory_observations
        << ",\"hot_path_count\":" << profile.hot_paths.size()
        << ",\"branch_count\":" << profile.branches.size()
        << ",\"branch_target_count\":" << profile.branch_targets.size()
        << ",\"syscall_count\":" << profile.syscalls.size()
        << ",\"trap_count\":" << profile.traps.size()
        << ",\"memory_region_count\":" << profile.memory_regions.size()
        << ",\"pc_cost_count\":" << profile.pc_costs.size()
        << ",\"top_hot_path\":";
    if (profile.hot_paths.empty()) {
        out << "null";
    } else {
        const ExecutionHotPathEntry& hot_path = profile.hot_paths.front();
        out << "{"
            << "\"start_pc\":";
        append_json_string(out, hex_u64(hot_path.start_pc));
        out << ",\"end_pc\":";
        append_json_string(out, hex_u64(hot_path.end_pc));
        out << ",\"executions\":" << hot_path.executions
            << ",\"retired_instructions\":" << hot_path.retired_instructions
            << "}";
    }
    out << ",\"top_pc_cost\":";
    if (profile.pc_costs.empty()) {
        out << "null";
    } else {
        const ExecutionPcCostEntry& pc_cost = profile.pc_costs.front();
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(pc_cost.pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(pc_cost.raw));
        out << ",\"retirements\":" << pc_cost.retirements
            << ",\"cycles\":" << pc_cost.cycles
            << ",\"memory_observations\":" << pc_cost.memory_observations
            << ",\"memory_reads\":" << pc_cost.memory_reads
            << ",\"memory_writes\":" << pc_cost.memory_writes
            << ",\"memory_faults\":" << pc_cost.memory_faults
            << ",\"memory_bytes\":" << pc_cost.memory_bytes
            << "}";
    }
    out << "}"
        << ",\"evidence_ref\":{"
        << "\"debug_json\":\"snapshot.profile\""
        << "}"
        << "}";
}

void append_execution_profile(std::ostringstream& out, const ExecutionProfileSnapshot& profile) {
    out << "{"
        << "\"total_retirements\":" << profile.total_retirements
        << ",\"total_traps\":" << profile.total_traps
        << ",\"total_memory_observations\":" << profile.total_memory_observations
        << ",\"hot_paths\":[";
    for (size_t i = 0; i < profile.hot_paths.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"start_pc\":";
        append_json_string(out, hex_u64(profile.hot_paths[i].start_pc));
        out << ",\"end_pc\":";
        append_json_string(out, hex_u64(profile.hot_paths[i].end_pc));
        out << ",\"executions\":" << profile.hot_paths[i].executions
            << ",\"retired_instructions\":" << profile.hot_paths[i].retired_instructions
            << "}";
    }
    out << "],\"branches\":[";
    for (size_t i = 0; i < profile.branches.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(profile.branches[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(profile.branches[i].raw));
        out << ",\"executions\":" << profile.branches[i].executions
            << ",\"redirects\":" << profile.branches[i].redirects
            << "}";
    }
    out << "],\"branch_targets\":[";
    for (size_t i = 0; i < profile.branch_targets.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(profile.branch_targets[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(profile.branch_targets[i].raw));
        out << ",\"target_pc\":";
        append_json_string(out, hex_u64(profile.branch_targets[i].target_pc));
        out << ",\"executions\":" << profile.branch_targets[i].executions
            << ",\"redirects\":" << profile.branch_targets[i].redirects
            << "}";
    }
    out << "],\"syscalls\":[";
    for (size_t i = 0; i < profile.syscalls.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(profile.syscalls[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(profile.syscalls[i].raw));
        out << ",\"count\":" << profile.syscalls[i].count
            << "}";
    }
    out << "],\"traps\":[";
    for (size_t i = 0; i < profile.traps.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(profile.traps[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(profile.traps[i].raw));
        out << ",\"cause\":";
        append_json_string(out, hex_u64(profile.traps[i].cause));
        out << ",\"privilege\":";
        append_json_string(out, privilege_name(profile.traps[i].privilege));
        out << ",\"interrupt\":" << (profile.traps[i].interrupt ? "true" : "false")
            << ",\"count\":" << profile.traps[i].count
            << "}";
    }
    out << "],\"shadow_cache\":{"
        << "\"line_size_bytes\":" << profile.shadow_cache.line_size_bytes
        << ",\"capacity_lines\":" << profile.shadow_cache.capacity_lines
        << ",\"resident_lines\":" << profile.shadow_cache.resident_lines
        << ",\"line_accesses\":" << profile.shadow_cache.line_accesses
        << ",\"hits\":" << profile.shadow_cache.hits
        << ",\"misses\":" << profile.shadow_cache.misses
        << ",\"evictions\":" << profile.shadow_cache.evictions
        << ",\"bypasses\":" << profile.shadow_cache.bypasses
        << "},\"memory_regions\":[";
    for (size_t i = 0; i < profile.memory_regions.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"label\":";
        append_json_string(out, profile.memory_regions[i].label);
        out << ",\"kind\":";
        append_json_string(out, profile.memory_regions[i].kind);
        out << ",\"cacheable\":" << (profile.memory_regions[i].cacheable ? "true" : "false")
            << ",\"dma_visible\":" << (profile.memory_regions[i].dma_visible ? "true" : "false")
            << ",\"has_side_effect\":" << (profile.memory_regions[i].has_side_effect ? "true" : "false")
            << ",\"supports_burst\":" << (profile.memory_regions[i].supports_burst ? "true" : "false")
            << ",\"accesses\":" << profile.memory_regions[i].accesses
            << ",\"reads\":" << profile.memory_regions[i].reads
            << ",\"writes\":" << profile.memory_regions[i].writes
            << ",\"faults\":" << profile.memory_regions[i].faults
            << ",\"bytes\":" << profile.memory_regions[i].bytes
            << ",\"shadow_cache_line_accesses\":" << profile.memory_regions[i].shadow_cache_line_accesses
            << ",\"shadow_cache_hits\":" << profile.memory_regions[i].shadow_cache_hits
            << ",\"shadow_cache_misses\":" << profile.memory_regions[i].shadow_cache_misses
            << ",\"shadow_cache_evictions\":" << profile.memory_regions[i].shadow_cache_evictions
            << ",\"shadow_cache_bypasses\":" << profile.memory_regions[i].shadow_cache_bypasses
            << "}";
    }
    out << "],\"pc_costs\":[";
    for (size_t i = 0; i < profile.pc_costs.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"pc\":";
        append_json_string(out, hex_u64(profile.pc_costs[i].pc));
        out << ",\"raw\":";
        append_json_string(out, hex_u64(profile.pc_costs[i].raw));
        out << ",\"retirements\":" << profile.pc_costs[i].retirements
            << ",\"cycles\":" << profile.pc_costs[i].cycles
            << ",\"memory_observations\":" << profile.pc_costs[i].memory_observations
            << ",\"memory_reads\":" << profile.pc_costs[i].memory_reads
            << ",\"memory_writes\":" << profile.pc_costs[i].memory_writes
            << ",\"memory_faults\":" << profile.pc_costs[i].memory_faults
            << ",\"memory_bytes\":" << profile.pc_costs[i].memory_bytes
            << "}";
    }
    out << "]}";
}

void append_l1_data_cache(std::ostringstream& out, const DebugL1DataCacheSnapshot& cache) {
    out << "{"
        << "\"enabled\":" << (cache.enabled ? "true" : "false")
        << ",\"line_size_bytes\":" << cache.line_size_bytes
        << ",\"capacity_lines\":" << cache.capacity_lines
        << ",\"loads\":" << cache.loads
        << ",\"stores\":" << cache.stores
        << ",\"hits\":" << cache.hits
        << ",\"misses\":" << cache.misses
        << ",\"evictions\":" << cache.evictions
        << ",\"bypasses\":" << cache.bypasses
        << ",\"write_through_stores\":" << cache.write_through_stores
        << "}";
}

void append_bus_access(std::ostringstream& out, const DebugBusAccess& access) {
    out << "{"
        << "\"valid\":" << (access.valid ? "true" : "false")
        << ",\"success\":" << (access.success ? "true" : "false")
        << ",\"write\":" << (access.write ? "true" : "false")
        << ",\"mmio\":" << (access.mmio ? "true" : "false")
        << ",\"source\":";
    append_json_string(out, access.source);
    out << ",\"kind\":";
    append_json_string(out, access.kind);
    out << ",\"addr\":";
    append_json_string(out, hex_u64(access.addr));
    out << ",\"value\":";
    append_json_string(out, hex_u64(access.value));
    out << ",\"size\":" << access.size
        << ",\"device\":";
    append_json_string(out, access.device);
    out << ",\"detail\":";
    append_json_string(out, access.detail);
    out << "}";
}

std::string serialize_snapshot_json(const DebugSnapshot& snapshot) {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"snapshot\""
        << ",\"summary\":{"
        << "\"cycle\":" << snapshot.summary.cycle
        << ",\"instret\":" << snapshot.summary.instret
        << ",\"pc\":";
    append_json_string(out, hex_u64(snapshot.summary.pc));
    out << ",\"halted\":" << (snapshot.summary.halted ? "true" : "false")
        << ",\"privilege\":";
    append_json_string(out, privilege_name(snapshot.summary.privilege));
    out << ",\"backend\":";
    append_json_string(out, snapshot.summary.backend);
    out << "},\"pipeline\":{"
        << "\"if\":";
    append_stage(out, snapshot.pipeline.if_stage);
    out << ",\"id\":";
    append_stage(out, snapshot.pipeline.id_stage);
    out << ",\"ex\":";
    append_stage(out, snapshot.pipeline.ex_stage);
    out << ",\"mem\":";
    append_stage(out, snapshot.pipeline.mem_stage);
    out << ",\"wb\":";
    append_stage(out, snapshot.pipeline.wb_stage);
    out << ",\"last_sequence_id\":" << snapshot.pipeline.last_sequence_id
        << ",\"retire_trace\":";
    append_retire_trace(out, snapshot.pipeline.retire_trace);
    out << ",\"flags\":{"
        << "\"stalled\":" << (snapshot.pipeline.stalled ? "true" : "false")
        << ",\"stall_reason\":";
    append_json_string(out, snapshot.pipeline.stall_reason);
    out
        << ",\"redirected\":" << (snapshot.pipeline.redirected ? "true" : "false")
        << ",\"pending_fetch_fault\":" << (snapshot.pipeline.pending_fetch_fault ? "true" : "false")
        << ",\"trap_flush\":" << (snapshot.pipeline.trap_flush ? "true" : "false")
        << ",\"replay_flush\":" << (snapshot.pipeline.replay_flush ? "true" : "false")
        << ",\"committed\":" << (snapshot.pipeline.committed ? "true" : "false")
        << "}"
        << ",\"redirect_target\":";
    append_json_string(out, hex_u64(snapshot.pipeline.redirect_target));
    out << ",\"ooo\":{"
        << "\"rob_depth\":" << snapshot.pipeline.ooo.rob_depth
        << ",\"rob_head_sequence_id\":" << snapshot.pipeline.ooo.rob_head_sequence_id
        << ",\"lsq_depth\":" << snapshot.pipeline.ooo.lsq_depth
        << ",\"lsq_head_sequence_id\":" << snapshot.pipeline.ooo.lsq_head_sequence_id
        << ",\"lsq_load_state\":";
    append_json_string(out, snapshot.pipeline.ooo.lsq_load_state);
    out << ",\"lsq_load_sequence_id\":" << snapshot.pipeline.ooo.lsq_load_sequence_id
        << ",\"lsq_store_sequence_id\":" << snapshot.pipeline.ooo.lsq_store_sequence_id
        << "}"
        << ",\"predictor\":{"
        << "\"mode\":";
    append_json_string(out, snapshot.pipeline.predictor.mode);
    out << ",\"last_prediction_valid\":" << (snapshot.pipeline.predictor.last_prediction_valid ? "true" : "false")
        << ",\"last_prediction_taken\":" << (snapshot.pipeline.predictor.last_prediction_taken ? "true" : "false")
        << ",\"last_prediction_correct\":" << (snapshot.pipeline.predictor.last_prediction_correct ? "true" : "false")
        << ",\"last_prediction_pc\":";
    append_json_string(out, hex_u64(snapshot.pipeline.predictor.last_prediction_pc));
    out << ",\"last_prediction_target\":";
    append_json_string(out, hex_u64(snapshot.pipeline.predictor.last_prediction_target));
    out << ",\"last_mispredict_valid\":" << (snapshot.pipeline.predictor.last_mispredict_valid ? "true" : "false")
        << ",\"last_mispredict_pc\":";
    append_json_string(out, hex_u64(snapshot.pipeline.predictor.last_mispredict_pc));
    out << ",\"last_mispredict_target\":";
    append_json_string(out, hex_u64(snapshot.pipeline.predictor.last_mispredict_target));
    out << ",\"total_predictions\":" << snapshot.pipeline.predictor.total_predictions
        << ",\"correct_predictions\":" << snapshot.pipeline.predictor.correct_predictions
        << ",\"mispredictions\":" << snapshot.pipeline.predictor.mispredictions
        << "}";
    out << "},\"profile\":";
    append_execution_profile(out, snapshot.profile);
    out << ",\"observation_event\":";
    append_execution_profile_observation_event(out, snapshot.profile, snapshot.summary);
    out << ",\"l1_data_cache\":";
    append_l1_data_cache(out, snapshot.l1_data_cache);
    out << ",\"gpr\":[";
    for (size_t i = 0; i < snapshot.gpr.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        append_json_string(out, hex_u64(snapshot.gpr[i]));
    }
    out << "],\"vector\":{"
        << "\"sew_bytes\":" << static_cast<unsigned>(snapshot.vector.sew_bytes)
        << ",\"vl\":" << static_cast<unsigned>(snapshot.vector.vl)
        << ",\"registers\":[";
    for (size_t i = 0; i < snapshot.vector.registers.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        append_json_string(out, hex_bytes(snapshot.vector.registers[i]));
    }
    out << "]},\"csrs\":{"
        << "\"mstatus\":";
    append_json_string(out, hex_u64(snapshot.csrs.mstatus));
    out << ",\"sstatus\":";
    append_json_string(out, hex_u64(snapshot.csrs.sstatus));
    out << ",\"mepc\":";
    append_json_string(out, hex_u64(snapshot.csrs.mepc));
    out << ",\"sepc\":";
    append_json_string(out, hex_u64(snapshot.csrs.sepc));
    out << ",\"mcause\":";
    append_json_string(out, hex_u64(snapshot.csrs.mcause));
    out << ",\"scause\":";
    append_json_string(out, hex_u64(snapshot.csrs.scause));
    out << ",\"mtval\":";
    append_json_string(out, hex_u64(snapshot.csrs.mtval));
    out << ",\"stval\":";
    append_json_string(out, hex_u64(snapshot.csrs.stval));
    out << ",\"mie\":";
    append_json_string(out, hex_u64(snapshot.csrs.mie));
    out << ",\"mip\":";
    append_json_string(out, hex_u64(snapshot.csrs.mip));
    out << ",\"sie\":";
    append_json_string(out, hex_u64(snapshot.csrs.sie));
    out << ",\"sip\":";
    append_json_string(out, hex_u64(snapshot.csrs.sip));
    out << ",\"mtvec\":";
    append_json_string(out, hex_u64(snapshot.csrs.mtvec));
    out << ",\"stvec\":";
    append_json_string(out, hex_u64(snapshot.csrs.stvec));
    out << ",\"satp\":";
    append_json_string(out, hex_u64(snapshot.csrs.satp));
    out << "},\"bus\":";
    append_bus_access(out, snapshot.bus);
    out << ",\"guest_bus\":";
    append_bus_access(out, snapshot.guest_bus);
    out << ",\"devices\":{"
        << "\"uart\":{"
        << "\"ier\":" << static_cast<unsigned>(snapshot.devices.uart.ier)
        << ",\"thre_interrupt_asserted\":" << (snapshot.devices.uart.thre_interrupt_asserted ? "true" : "false")
        << ",\"output_size\":" << snapshot.devices.uart.output_size
        << ",\"recent_output\":";
    append_json_string(out, snapshot.devices.uart.recent_output);
    out << "},\"clint\":{"
        << "\"mtime\":" << snapshot.devices.clint.mtime
        << ",\"mtimecmp\":" << snapshot.devices.clint.mtimecmp
        << ",\"timer_interrupt_pending\":" << (snapshot.devices.clint.timer_interrupt_pending ? "true" : "false")
        << "},\"plic\":{"
        << "\"priority\":" << snapshot.devices.plic.priority
        << ",\"level\":" << (snapshot.devices.plic.level ? "true" : "false")
        << ",\"pending\":" << (snapshot.devices.plic.pending ? "true" : "false")
        << ",\"claimed\":" << (snapshot.devices.plic.claimed ? "true" : "false")
        << ",\"machine_enables\":" << snapshot.devices.plic.machine_enables
        << ",\"supervisor_enables\":" << snapshot.devices.plic.supervisor_enables
        << ",\"machine_threshold\":" << snapshot.devices.plic.machine_threshold
        << ",\"supervisor_threshold\":" << snapshot.devices.plic.supervisor_threshold
        << ",\"machine_has_pending\":" << (snapshot.devices.plic.machine_has_pending ? "true" : "false")
        << ",\"supervisor_has_pending\":" << (snapshot.devices.plic.supervisor_has_pending ? "true" : "false")
        << "},\"storage\":{"
        << "\"attached\":" << (snapshot.devices.storage.attached ? "true" : "false")
        << ",\"status\":" << snapshot.devices.storage.status
        << ",\"capacity_blocks\":" << snapshot.devices.storage.capacity_blocks
        << ",\"lba\":" << snapshot.devices.storage.lba
        << ",\"block_count\":" << snapshot.devices.storage.block_count
        << ",\"error_code\":" << snapshot.devices.storage.error_code
        << "},\"ai_accelerator\":{"
        << "\"present\":" << (snapshot.devices.ai_accelerator.present ? "true" : "false")
        << ",\"queue_depth\":" << snapshot.devices.ai_accelerator.queue_depth
        << ",\"doorbell_count\":" << snapshot.devices.ai_accelerator.doorbell_count
        << ",\"last_fault\":" << snapshot.devices.ai_accelerator.last_fault
        << ",\"completion_count\":" << snapshot.devices.ai_accelerator.completion_count
        << ",\"engine_busy\":" << (snapshot.devices.ai_accelerator.engine_busy ? "true" : "false")
        << ",\"scratchpad_occupancy_bytes\":"
        << snapshot.devices.ai_accelerator.scratchpad_occupancy_bytes
        << ",\"dma_load_bytes\":" << snapshot.devices.ai_accelerator.dma_load_bytes
        << ",\"dma_store_bytes\":" << snapshot.devices.ai_accelerator.dma_store_bytes
        << ",\"device_cycles\":" << snapshot.devices.ai_accelerator.device_cycles
        << ",\"dma_cycles\":" << snapshot.devices.ai_accelerator.dma_cycles
        << ",\"compute_cycles\":" << snapshot.devices.ai_accelerator.compute_cycles
        << ",\"stall_cycles\":" << snapshot.devices.ai_accelerator.stall_cycles
        << ",\"busy_cycles\":" << snapshot.devices.ai_accelerator.busy_cycles
        << ",\"queue_cycles\":" << snapshot.devices.ai_accelerator.queue_cycles
        << ",\"completion_cycles\":" << snapshot.devices.ai_accelerator.completion_cycles
        << ",\"effective_ops_per_cycle\":"
        << snapshot.devices.ai_accelerator.effective_ops_per_cycle
        << ",\"utilization\":" << snapshot.devices.ai_accelerator.utilization
        << "}},\"events\":[";
    for (size_t i = 0; i < snapshot.events.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << "{"
            << "\"cycle\":" << snapshot.events[i].cycle
            << ",\"kind\":";
        append_json_string(out, snapshot.events[i].kind);
        out << ",\"detail\":";
        append_json_string(out, snapshot.events[i].detail);
        out << "}";
    }
    out << "]}";
    return out.str();
}

}  // namespace

std::string debug_snapshot_json(const DebugSnapshot& snapshot) {
    return serialize_snapshot_json(snapshot);
}

std::string debug_protocol_ok_json(const char* command) {
    return std::string("{\"type\":\"ok\",\"cmd\":\"") + command + "\"}";
}

std::string debug_protocol_uart_output_json(const DebugSession::UartOutputChunk& chunk) {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"uart_output\""
        << ",\"offset\":" << chunk.offset
        << ",\"next_offset\":" << chunk.next_offset
        << ",\"text\":";
    append_json_string(out, chunk.text);
    out << "}";
    return out.str();
}

std::string debug_protocol_translation_plan_json(const DebugSession::TranslationPlanSnapshot& plan) {
    std::ostringstream out;
    out << "{\"type\":\"translation_plan\"";
    if (!plan.candidate) {
        out << ",\"status\":\"none\""
            << ",\"reason\":";
        append_json_string(out, plan.reason.empty() ? "no-hot-paths" : plan.reason);
        out << "}";
        return out.str();
    }

    out << ",\"status\":\"" << (plan.inlineable ? "inlineable" : "fallback") << "\""
        << ",\"start_pc\":";
    append_json_string(out, hex_u64(plan.start_pc));
    out << ",\"end_pc\":";
    append_json_string(out, hex_u64(plan.end_pc));
    out << ",\"executions\":" << plan.executions
        << ",\"retired_instructions\":" << plan.retired_instructions
        << ",\"inlineable_instructions\":" << plan.inlineable_instructions;
    if (!plan.inlineable) {
        out << ",\"fallback_pc\":";
        append_json_string(out, hex_u64(plan.fallback_pc));
        out << ",\"reason\":";
        append_json_string(out, plan.reason);
        if (!plan.boundary_kind.empty()) {
            out << ",\"boundary_kind\":";
            append_json_string(out, plan.boundary_kind);
        }
    }
    out << "}";
    return out.str();
}

std::string debug_protocol_jit_dispatch_json(const DbtJitDryRunSummary& summary) {
    std::ostringstream out;
    out << "{"
        << "\"type\":\"jit_dispatch\""
        << ",\"ok\":" << (summary.ok ? "true" : "false")
        << ",\"source\":";
    append_json_string(out, summary.source);
    out << ",\"action\":";
    append_json_string(out, summary.action);
    out << ",\"start_pc\":";
    append_json_string(out, summary.start_pc);
    out << ",\"end_pc\":";
    append_json_string(out, summary.end_pc);
    out << ",\"cache_state\":";
    append_json_string(out, summary.cache_state);
    out << ",\"planned\":" << (summary.planned ? "true" : "false")
        << ",\"translated\":" << (summary.translated ? "true" : "false")
        << ",\"lowered\":" << (summary.lowered ? "true" : "false")
        << ",\"fallback_to_reference\":" << (summary.fallback_to_reference ? "true" : "false")
        << ",\"lowered_instruction_count\":" << summary.lowered_instruction_count
        << ",\"candidate_executions\":" << summary.candidate_executions
        << ",\"candidate_retired_instructions\":" << summary.candidate_retired_instructions;
    out << ",\"reject_kind\":";
    append_json_string(out, summary.reject_kind);
    out << ",\"reject_reason\":";
    append_json_string(out, summary.reject_reason);
    out << ",\"helper_replay_kind\":";
    append_json_string(out, summary.helper_replay_kind);
    out << ",\"host_code\":" << (summary.generated_host_code ? "true" : "false")
        << ",\"executable_memory\":" << (summary.requested_executable_memory ? "true" : "false")
        << ",\"guest_execution\":" << (summary.executed_guest_code ? "true" : "false")
        << ",\"observation_event\":";
    append_jit_dispatch_observation_event(out, summary);
    out << "}";
    return out.str();
}

std::string debug_protocol_error_json(const std::string& message) {
    std::ostringstream out;
    out << "{\"type\":\"error\",\"message\":";
    append_json_string(out, message);
    out << "}";
    return out.str();
}

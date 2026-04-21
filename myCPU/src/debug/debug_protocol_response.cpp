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
    out << "],\"memory_regions\":[";
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
            << "}";
    }
    out << "]}";
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
    out << "},\"bus\":{"
        << "\"valid\":" << (snapshot.bus.valid ? "true" : "false")
        << ",\"success\":" << (snapshot.bus.success ? "true" : "false")
        << ",\"write\":" << (snapshot.bus.write ? "true" : "false")
        << ",\"mmio\":" << (snapshot.bus.mmio ? "true" : "false")
        << ",\"addr\":";
    append_json_string(out, hex_u64(snapshot.bus.addr));
    out << ",\"value\":";
    append_json_string(out, hex_u64(snapshot.bus.value));
    out << ",\"size\":" << snapshot.bus.size
        << ",\"device\":";
    append_json_string(out, snapshot.bus.device);
    out << ",\"detail\":";
    append_json_string(out, snapshot.bus.detail);
    out << "},\"devices\":{"
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

std::string debug_protocol_error_json(const std::string& message) {
    std::ostringstream out;
    out << "{\"type\":\"error\",\"message\":";
    append_json_string(out, message);
    out << "}";
    return out.str();
}

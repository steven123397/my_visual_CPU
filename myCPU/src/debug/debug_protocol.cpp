#include "debug_protocol.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

#include "debug_session.h"

namespace {

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

size_t find_key_offset(const std::string& line, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t key_pos = line.find(needle);
    if (key_pos == std::string::npos) {
        throw std::runtime_error(std::string("missing key: ") + key);
    }
    const size_t colon = line.find(':', key_pos + needle.size());
    if (colon == std::string::npos) {
        throw std::runtime_error(std::string("missing ':' for key: ") + key);
    }
    return colon + 1;
}

std::string extract_string(const std::string& line, const char* key) {
    size_t value_pos = find_key_offset(line, key);
    while (value_pos < line.size() && std::isspace(static_cast<unsigned char>(line[value_pos])) != 0) {
        ++value_pos;
    }
    if (value_pos >= line.size() || line[value_pos] != '"') {
        throw std::runtime_error(std::string("expected string for key: ") + key);
    }
    ++value_pos;
    const size_t end = line.find('"', value_pos);
    if (end == std::string::npos) {
        throw std::runtime_error(std::string("unterminated string for key: ") + key);
    }
    return line.substr(value_pos, end - value_pos);
}

std::string try_extract_string(const std::string& line, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    if (line.find(needle) == std::string::npos) {
        return {};
    }
    return extract_string(line, key);
}

bool try_extract_bool(const std::string& line, const char* key, bool default_value) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t key_pos = line.find(needle);
    if (key_pos == std::string::npos) {
        return default_value;
    }
    size_t value_pos = find_key_offset(line, key);
    while (value_pos < line.size() && std::isspace(static_cast<unsigned char>(line[value_pos])) != 0) {
        ++value_pos;
    }
    if (line.compare(value_pos, 4, "true") == 0) {
        return true;
    }
    if (line.compare(value_pos, 5, "false") == 0) {
        return false;
    }
    throw std::runtime_error(std::string("expected boolean for key: ") + key);
}

uint64_t try_extract_u64(const std::string& line, const char* key, uint64_t default_value) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t key_pos = line.find(needle);
    if (key_pos == std::string::npos) {
        return default_value;
    }
    size_t value_pos = find_key_offset(line, key);
    while (value_pos < line.size() && std::isspace(static_cast<unsigned char>(line[value_pos])) != 0) {
        ++value_pos;
    }

    bool quoted = false;
    if (value_pos < line.size() && line[value_pos] == '"') {
        quoted = true;
        ++value_pos;
    }

    size_t end = value_pos;
    while (end < line.size()) {
        const char ch = line[end];
        if (quoted) {
            if (ch == '"') {
                break;
            }
        } else if (ch == ',' || ch == '}' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
            break;
        }
        ++end;
    }

    const std::string token = line.substr(value_pos, end - value_pos);
    if (token.empty()) {
        return default_value;
    }
    return std::strtoull(token.c_str(), nullptr, 0);
}

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

BackendKind parse_backend_kind(const std::string& name) {
    if (name == "functional") {
        return BackendKind::Functional;
    }
    if (name == "pipeline") {
        return BackendKind::Pipeline;
    }
    throw std::runtime_error("unknown backend: " + name);
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
            escaped.push_back(ch);
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

void append_json_string(std::ostringstream& out, const std::string& value) {
    out << '"' << json_escape(value) << '"';
}

void append_stage(std::ostringstream& out, const DebugStageSnapshot& stage) {
    out << "{"
        << "\"valid\":" << (stage.valid ? "true" : "false")
        << ",\"pc\":";
    append_json_string(out, hex_u64(stage.pc));
    out << ",\"raw\":";
    append_json_string(out, hex_u64(stage.raw));
    out << ",\"text\":";
    append_json_string(out, stage.text);
    out << "}";
}

std::string snapshot_json(const DebugSnapshot& snapshot) {
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
    out << ",\"flags\":{"
        << "\"stalled\":" << (snapshot.pipeline.stalled ? "true" : "false")
        << ",\"redirected\":" << (snapshot.pipeline.redirected ? "true" : "false")
        << ",\"pending_fetch_fault\":" << (snapshot.pipeline.pending_fetch_fault ? "true" : "false")
        << ",\"trap_flush\":" << (snapshot.pipeline.trap_flush ? "true" : "false")
        << ",\"committed\":" << (snapshot.pipeline.committed ? "true" : "false")
        << "}"
        << ",\"redirect_target\":";
    append_json_string(out, hex_u64(snapshot.pipeline.redirect_target));
    out << "},\"gpr\":[";
    for (size_t i = 0; i < snapshot.gpr.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        append_json_string(out, hex_u64(snapshot.gpr[i]));
    }
    out << "],\"csrs\":{"
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
        << ",\"write\":" << (snapshot.bus.write ? "true" : "false")
        << ",\"mmio\":" << (snapshot.bus.mmio ? "true" : "false")
        << ",\"addr\":";
    append_json_string(out, hex_u64(snapshot.bus.addr));
    out << ",\"value\":";
    append_json_string(out, hex_u64(snapshot.bus.value));
    out << ",\"size\":" << snapshot.bus.size
        << ",\"device\":";
    append_json_string(out, snapshot.bus.device);
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

std::string ok_json(const char* command) {
    return std::string("{\"type\":\"ok\",\"cmd\":\"") + command + "\"}";
}

std::string error_json(const std::string& message) {
    std::ostringstream out;
    out << "{\"type\":\"error\",\"message\":";
    append_json_string(out, message);
    out << "}";
    return out.str();
}

}  // namespace

int run_debug_cli(std::istream& in, std::ostream& out, std::ostream& err) {
    DebugSession session;
    std::string line;

    try {
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty()) {
                continue;
            }

            const std::string command = extract_string(line, "cmd");
            if (command == "load") {
                const std::string image = extract_string(line, "image");
                const std::string backend_name = try_extract_string(line, "backend");
                const std::string disk = try_extract_string(line, "disk");
                const bool disk_ready = try_extract_bool(line, "disk_ready", true);
                const bool disk_magic_valid = try_extract_bool(line, "disk_magic_valid", true);
                const bool flat = try_extract_bool(line, "flat", false);
                const uint64_t addr = try_extract_u64(line, "addr", MEM_BASE);
                const BackendKind backend_kind =
                    parse_backend_kind(backend_name.empty() ? std::string("pipeline") : backend_name);
                if (flat) {
                    session.load_binary(
                        image,
                        addr,
                        backend_kind,
                        disk.empty() ? nullptr : disk.c_str(),
                        disk_ready,
                        disk_magic_valid);
                } else {
                    session.load_elf(
                        image,
                        backend_kind,
                        disk.empty() ? nullptr : disk.c_str(),
                        disk_ready,
                        disk_magic_valid);
                }
                out << ok_json("load") << '\n';
                continue;
            }
            if (command == "snapshot") {
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "step_cycle") {
                session.step_cycle();
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "step_commit") {
                session.step_commit();
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "reset") {
                session.reset();
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "quit") {
                out << ok_json("quit") << '\n';
                return 0;
            }

            throw std::runtime_error("unknown command: " + command);
        }
    } catch (const std::exception& ex) {
        out << error_json(ex.what()) << '\n';
        err << ex.what() << '\n';
        return 1;
    }

    return 0;
}

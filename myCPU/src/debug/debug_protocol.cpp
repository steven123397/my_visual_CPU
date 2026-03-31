#include "debug_protocol.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
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

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
    };

    Type type = Type::Null;
    bool bool_value = false;
    uint64_t number_value = 0;
    std::string string_value;
};

using JsonObject = std::map<std::string, JsonValue>;

void skip_whitespace(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

void expect_char(const std::string& text, size_t& pos, char expected) {
    skip_whitespace(text, pos);
    if (pos >= text.size() || text[pos] != expected) {
        throw std::runtime_error(std::string("expected '") + expected + "'");
    }
    ++pos;
}

int hex_digit_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint <= 0x7fU) {
        out.push_back(static_cast<char>(codepoint));
        return;
    }
    if (codepoint <= 0x7ffU) {
        out.push_back(static_cast<char>(0xc0U | ((codepoint >> 6U) & 0x1fU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        return;
    }
    if (codepoint <= 0xffffU) {
        out.push_back(static_cast<char>(0xe0U | ((codepoint >> 12U) & 0x0fU)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        return;
    }
    out.push_back(static_cast<char>(0xf0U | ((codepoint >> 18U) & 0x07U)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
}

std::string parse_json_string(const std::string& text, size_t& pos) {
    expect_char(text, pos, '"');
    std::string value;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') {
            return value;
        }
        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }

        if (pos >= text.size()) {
            throw std::runtime_error("unterminated escape in string");
        }

        switch (text[pos++]) {
        case '\\':
            value.push_back('\\');
            break;
        case '"':
            value.push_back('"');
            break;
        case '/':
            value.push_back('/');
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u': {
            uint32_t codepoint = 0;
            for (int i = 0; i < 4; ++i) {
                if (pos >= text.size()) {
                    throw std::runtime_error("unterminated unicode escape in string");
                }
                const int digit = hex_digit_value(text[pos++]);
                if (digit < 0) {
                    throw std::runtime_error("invalid unicode escape in string");
                }
                codepoint = (codepoint << 4U) | static_cast<uint32_t>(digit);
            }
            append_utf8(value, codepoint);
            break;
        }
        default:
            throw std::runtime_error("unsupported escape in string");
        }
    }

    throw std::runtime_error("unterminated string");
}

JsonValue parse_json_value(const std::string& text, size_t& pos) {
    skip_whitespace(text, pos);
    if (pos >= text.size()) {
        throw std::runtime_error("expected JSON value");
    }

    if (text[pos] == '"') {
        JsonValue value;
        value.type = JsonValue::Type::String;
        value.string_value = parse_json_string(text, pos);
        return value;
    }
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        JsonValue value;
        value.type = JsonValue::Type::Bool;
        value.bool_value = true;
        return value;
    }
    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        JsonValue value;
        value.type = JsonValue::Type::Bool;
        value.bool_value = false;
        return value;
    }
    if (text.compare(pos, 4, "null") == 0) {
        pos += 4;
        return JsonValue{};
    }

    if (text[pos] == '-') {
        throw std::runtime_error("negative numbers are not supported");
    }
    if (!std::isdigit(static_cast<unsigned char>(text[pos]))) {
        throw std::runtime_error("unsupported JSON value");
    }

    const size_t begin = pos;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    JsonValue value;
    value.type = JsonValue::Type::Number;
    value.number_value = std::strtoull(text.substr(begin, pos - begin).c_str(), nullptr, 10);
    return value;
}

JsonObject parse_json_object(const std::string& text) {
    size_t pos = 0;
    expect_char(text, pos, '{');

    JsonObject object;
    skip_whitespace(text, pos);
    if (pos < text.size() && text[pos] == '}') {
        ++pos;
        skip_whitespace(text, pos);
        if (pos != text.size()) {
            throw std::runtime_error("unexpected trailing tokens after JSON object");
        }
        return object;
    }

    while (pos < text.size()) {
        const std::string key = parse_json_string(text, pos);
        expect_char(text, pos, ':');
        object[key] = parse_json_value(text, pos);
        skip_whitespace(text, pos);
        if (pos >= text.size()) {
            break;
        }
        if (text[pos] == '}') {
            ++pos;
            skip_whitespace(text, pos);
            if (pos != text.size()) {
                throw std::runtime_error("unexpected trailing tokens after JSON object");
            }
            return object;
        }
        expect_char(text, pos, ',');
    }

    throw std::runtime_error("unterminated JSON object");
}

const JsonValue* try_find_value(const JsonObject& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

const JsonValue& require_value(const JsonObject& object, const char* key) {
    const JsonValue* value = try_find_value(object, key);
    if (!value) {
        throw std::runtime_error(std::string("missing key: ") + key);
    }
    return *value;
}

std::string extract_string(const JsonObject& object, const char* key) {
    const JsonValue& value = require_value(object, key);
    if (value.type != JsonValue::Type::String) {
        throw std::runtime_error(std::string("expected string for key: ") + key);
    }
    return value.string_value;
}

std::string try_extract_string(const JsonObject& object, const char* key) {
    const JsonValue* value = try_find_value(object, key);
    if (!value) {
        return {};
    }
    if (value->type != JsonValue::Type::String) {
        throw std::runtime_error(std::string("expected string for key: ") + key);
    }
    return value->string_value;
}

bool try_extract_bool(const JsonObject& object, const char* key, bool default_value) {
    const JsonValue* value = try_find_value(object, key);
    if (!value) {
        return default_value;
    }
    if (value->type != JsonValue::Type::Bool) {
        throw std::runtime_error(std::string("expected boolean for key: ") + key);
    }
    return value->bool_value;
}

uint64_t try_extract_u64(const JsonObject& object, const char* key, uint64_t default_value) {
    const JsonValue* value = try_find_value(object, key);
    if (!value || value->type == JsonValue::Type::Null) {
        return default_value;
    }
    if (value->type == JsonValue::Type::Number) {
        return value->number_value;
    }
    if (value->type == JsonValue::Type::String) {
        return std::strtoull(value->string_value.c_str(), nullptr, 0);
    }
    throw std::runtime_error(std::string("expected number for key: ") + key);
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
    out << ",\"predictor\":{"
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

std::string ok_json(const char* command) {
    return std::string("{\"type\":\"ok\",\"cmd\":\"") + command + "\"}";
}

std::string uart_output_json(const DebugSession::UartOutputChunk& chunk) {
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

            const JsonObject object = parse_json_object(line);
            const std::string command = extract_string(object, "cmd");
            if (command == "load") {
                const std::string image = extract_string(object, "image");
                const std::string backend_name = try_extract_string(object, "backend");
                const std::string disk = try_extract_string(object, "disk");
                const bool disk_ready = try_extract_bool(object, "disk_ready", true);
                const bool disk_magic_valid = try_extract_bool(object, "disk_magic_valid", true);
                const bool flat = try_extract_bool(object, "flat", false);
                const uint64_t addr = try_extract_u64(object, "addr", MEM_BASE);
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
                const uint64_t count = try_extract_u64(object, "count", 1);
                for (uint64_t i = 0; i < count; ++i) {
                    session.step_cycle();
                }
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "step_commit") {
                const uint64_t count = try_extract_u64(object, "count", 1);
                for (uint64_t i = 0; i < count; ++i) {
                    session.step_commit();
                }
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "run_until_uart_contains") {
                session.run_until_uart_contains(extract_string(object, "text"),
                                                try_extract_u64(object, "max_steps", 0));
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "run_until_halt") {
                session.run_until_halt(try_extract_u64(object, "max_steps", 0));
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "reset") {
                session.reset();
                out << snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command == "uart_input") {
                session.uart_input(extract_string(object, "text"));
                out << ok_json("uart_input") << '\n';
                continue;
            }
            if (command == "uart_output") {
                out << uart_output_json(
                           session.uart_output(static_cast<size_t>(try_extract_u64(object, "offset", 0))))
                    << '\n';
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

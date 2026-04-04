#include "debug_protocol_command.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>

namespace {

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

}  // namespace

DebugCliCommand parse_debug_cli_command(const std::string& line) {
    const JsonObject object = parse_json_object(line);
    const std::string command = extract_string(object, "cmd");

    DebugCliCommand parsed;
    if (command == "load") {
        parsed.kind = DebugCliCommandKind::Load;
        parsed.image = extract_string(object, "image");
        parsed.backend = try_extract_string(object, "backend");
        parsed.disk = try_extract_string(object, "disk");
        parsed.disk_ready = try_extract_bool(object, "disk_ready", true);
        parsed.disk_magic_valid = try_extract_bool(object, "disk_magic_valid", true);
        parsed.flat = try_extract_bool(object, "flat", false);
        parsed.addr = try_extract_u64(object, "addr", MEM_BASE);
        return parsed;
    }
    if (command == "snapshot") {
        parsed.kind = DebugCliCommandKind::Snapshot;
        return parsed;
    }
    if (command == "step_cycle") {
        parsed.kind = DebugCliCommandKind::StepCycle;
        parsed.count = try_extract_u64(object, "count", 1);
        return parsed;
    }
    if (command == "step_commit") {
        parsed.kind = DebugCliCommandKind::StepCommit;
        parsed.count = try_extract_u64(object, "count", 1);
        return parsed;
    }
    if (command == "run_until_uart_contains") {
        parsed.kind = DebugCliCommandKind::RunUntilUartContains;
        parsed.text = extract_string(object, "text");
        parsed.max_steps = try_extract_u64(object, "max_steps", 0);
        return parsed;
    }
    if (command == "run_until_halt") {
        parsed.kind = DebugCliCommandKind::RunUntilHalt;
        parsed.max_steps = try_extract_u64(object, "max_steps", 0);
        return parsed;
    }
    if (command == "reset") {
        parsed.kind = DebugCliCommandKind::Reset;
        return parsed;
    }
    if (command == "uart_input") {
        parsed.kind = DebugCliCommandKind::UartInput;
        parsed.text = extract_string(object, "text");
        return parsed;
    }
    if (command == "uart_output") {
        parsed.kind = DebugCliCommandKind::UartOutput;
        parsed.offset = static_cast<size_t>(try_extract_u64(object, "offset", 0));
        return parsed;
    }
    if (command == "quit") {
        parsed.kind = DebugCliCommandKind::Quit;
        return parsed;
    }

    throw std::runtime_error("unknown command: " + command);
}

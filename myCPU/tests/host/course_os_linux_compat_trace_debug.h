// Host smoke trace helper：从 guest 符号中读取 linux_compat trace，辅助定位卡点。
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../src/debug/debug_session.h"

extern "C" {
#include "../../guest/include/course_os_stage3.h"
}

namespace course_os_linux_compat_trace_debug {

struct GuestLinuxTraceRecordSnapshot {
    bool valid{false};
    uint64_t number{0};
    int64_t return_value{0};
    int32_t errno_value{0};
    uint64_t pc{0};
    char message[LINUX_COMPAT_MAX_MESSAGE]{};
};

struct GuestLinuxTraceSnapshot {
    bool available{false};
    uint64_t runtime_addr{0};
    uint64_t trace_count{0};
    bool trace_truncated{false};
    GuestLinuxTraceRecordSnapshot records[LINUX_COMPAT_MAX_TRACE_RECORDS]{};
    GuestLinuxTraceRecordSnapshot latest{};
    GuestLinuxTraceRecordSnapshot latest_error{};
};

class GuestLinuxTraceReader {
public:
    explicit GuestLinuxTraceReader(const char* elf_path)
        : runtime_addr_(linux_runtime_addr(elf_path)) {}

    GuestLinuxTraceSnapshot read(DebugSession& session) const {
        GuestLinuxTraceSnapshot out{};
        if (runtime_addr_ == 0) {
            return out;
        }
        out.runtime_addr = runtime_addr_;
        if (!read_u64(session,
                      runtime_addr_ +
                          offsetof(linux_compat_runtime_t, trace_count),
                      out.trace_count) ||
            !read_bool(session,
                       runtime_addr_ +
                           offsetof(linux_compat_runtime_t, trace_truncated),
                       out.trace_truncated) ||
            !read_bool(session,
                       runtime_addr_ +
                           offsetof(linux_compat_runtime_t,
                                    latest_trace_valid),
                       out.latest.valid) ||
            !read_bool(session,
                       runtime_addr_ +
                           offsetof(linux_compat_runtime_t,
                                    latest_error_trace_valid),
                       out.latest_error.valid)) {
            return out;
        }

        if (out.latest.valid &&
            !read_record(session,
                         runtime_addr_ +
                             offsetof(linux_compat_runtime_t,
                                      latest_trace_record),
                         out.latest)) {
            return GuestLinuxTraceSnapshot{};
        }
        if (out.latest_error.valid &&
            !read_record(session,
                         runtime_addr_ +
                             offsetof(linux_compat_runtime_t,
                                      latest_error_trace_record),
                         out.latest_error)) {
            return GuestLinuxTraceSnapshot{};
        }
        for (uint64_t i = 0;
             i < out.trace_count && i < LINUX_COMPAT_MAX_TRACE_RECORDS;
             ++i) {
            out.records[i].valid = read_record(
                session,
                runtime_addr_ +
                    offsetof(linux_compat_runtime_t, trace_records) +
                    i * sizeof(linux_compat_syscall_trace_record_t),
                out.records[i]);
            if (!out.records[i].valid) {
                return GuestLinuxTraceSnapshot{};
            }
        }
        out.available = true;
        return out;
    }

private:
    static uint64_t lookup_symbol(const char* elf_path, const char* symbol) {
        char command[256];
        std::snprintf(command,
                      sizeof(command),
                      "riscv64-unknown-elf-nm -n %s",
                      elf_path);
        FILE* pipe = popen(command, "r");
        if (pipe == nullptr) {
            return 0;
        }

        uint64_t address = 0;
        char line[256];
        while (std::fgets(line, sizeof(line), pipe) != nullptr) {
            unsigned long long parsed_addr = 0;
            char kind = '\0';
            char parsed_symbol[128];
            if (std::sscanf(line,
                            "%llx %c %127s",
                            &parsed_addr,
                            &kind,
                            parsed_symbol) == 3 &&
                std::strcmp(parsed_symbol, symbol) == 0) {
                address = static_cast<uint64_t>(parsed_addr);
                break;
            }
        }
        pclose(pipe);
        return address;
    }

    static uint64_t linux_runtime_addr(const char* elf_path) {
        const uint64_t stage_addr = lookup_symbol(elf_path, "g_stage");
        if (stage_addr == 0) {
            return 0;
        }
        return stage_addr + offsetof(course_os_stage3_t, shell) +
               offsetof(course_shell_t, linux_compat_runtime);
    }

    static bool read_u64(DebugSession& session, uint64_t addr, uint64_t& out) {
        return session.debug_try_load_guest_memory(addr, 8, out);
    }

    static bool read_u32(DebugSession& session, uint64_t addr, uint32_t& out) {
        uint64_t value = 0;
        if (!session.debug_try_load_guest_memory(addr, 4, value)) {
            return false;
        }
        out = static_cast<uint32_t>(value);
        return true;
    }

    static bool read_bool(DebugSession& session, uint64_t addr, bool& out) {
        uint64_t value = 0;
        if (!session.debug_try_load_guest_memory(addr, 1, value)) {
            return false;
        }
        out = value != 0;
        return true;
    }

    static bool read_cstr(DebugSession& session,
                          uint64_t addr,
                          char* out,
                          size_t out_size) {
        if (out == nullptr || out_size == 0) {
            return false;
        }
        for (size_t i = 0; i < out_size; ++i) {
            uint64_t value = 0;
            if (!session.debug_try_load_guest_memory(addr + i, 1, value)) {
                out[0] = '\0';
                return false;
            }
            out[i] = static_cast<char>(value & 0xffU);
            if (out[i] == '\0') {
                return true;
            }
        }
        out[out_size - 1U] = '\0';
        return true;
    }

    static bool read_record(DebugSession& session,
                            uint64_t addr,
                            GuestLinuxTraceRecordSnapshot& out) {
        uint64_t raw = 0;
        uint32_t raw32 = 0;
        if (!read_u64(session,
                      addr +
                          offsetof(linux_compat_syscall_trace_record_t,
                                   number),
                      out.number) ||
            !read_u64(session,
                      addr +
                          offsetof(linux_compat_syscall_trace_record_t,
                                   return_value),
                      raw) ||
            !read_u32(session,
                      addr +
                          offsetof(linux_compat_syscall_trace_record_t,
                                   errno_value),
                      raw32) ||
            !read_u64(session,
                      addr + offsetof(linux_compat_syscall_trace_record_t, pc),
                      out.pc) ||
            !read_cstr(session,
                       addr +
                           offsetof(linux_compat_syscall_trace_record_t,
                                    message),
                       out.message,
                       sizeof(out.message))) {
            return false;
        }
        out.return_value = static_cast<int64_t>(raw);
        out.errno_value = static_cast<int32_t>(raw32);
        return true;
    }

    uint64_t runtime_addr_{0};
};

inline void print_guest_linux_trace_record(
    const char* label,
    const GuestLinuxTraceRecordSnapshot& record) {
    if (!record.valid) {
        std::fprintf(stderr, "%s=none", label);
        return;
    }
    std::fprintf(stderr,
                 "%s=nr:%llu ret:%lld errno:%d pc:0x%llx msg:\"%s\"",
                 label,
                 (unsigned long long)record.number,
                 (long long)record.return_value,
                 record.errno_value,
                 (unsigned long long)record.pc,
                 record.message);
}

inline void print_guest_linux_trace(DebugSession& session,
                                    const GuestLinuxTraceReader& reader) {
    const GuestLinuxTraceSnapshot trace = reader.read(session);
    if (!trace.available) {
        std::fprintf(stderr, "guest linux trace unavailable\n");
        return;
    }
    std::fprintf(stderr,
                 "guest linux trace runtime=0x%llx trace_count=%llu "
                 "trace_truncated=%d ",
                 (unsigned long long)trace.runtime_addr,
                 (unsigned long long)trace.trace_count,
                 trace.trace_truncated ? 1 : 0);
    print_guest_linux_trace_record("latest", trace.latest);
    std::fprintf(stderr, " ");
    print_guest_linux_trace_record("latest_error", trace.latest_error);
    std::fprintf(stderr, "\n");
    for (uint64_t i = 0;
         i < trace.trace_count && i < LINUX_COMPAT_MAX_TRACE_RECORDS;
         ++i) {
        std::fprintf(stderr, "guest linux trace ");
        char label[24];
        std::snprintf(label, sizeof(label), "record[%llu]",
                      (unsigned long long)i);
        print_guest_linux_trace_record(label, trace.records[i]);
        std::fprintf(stderr, "\n");
    }
}

}  // namespace course_os_linux_compat_trace_debug

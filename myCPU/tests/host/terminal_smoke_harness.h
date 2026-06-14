#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../../src/debug/debug_protocol.h"
#include "../../src/debug/debug_session.h"

namespace terminal_smoke {

struct ExpectedText {
    const char* needle;
    const char* message;
};

inline bool expect_contains(const std::string& haystack,
                            const char* needle,
                            const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

inline BackendKind parse_backend_kind(const char* backend) {
    const std::string value = backend ? backend : "";
    if (value == "functional") {
        return BackendKind::Functional;
    }
    if (value == "pipeline") {
        return BackendKind::Pipeline;
    }
    std::fprintf(stderr, "unknown backend: %s\n", backend ? backend : "(null)");
    std::exit(1);
}

inline DebugSession::UartOutputChunk load_and_wait_for_prompt(
    DebugSession& session,
    const char* image,
    BackendKind backend,
    const char* prompt,
    std::uint64_t boot_max_steps,
    const char* disk_image = "tests/data/storage_basic.txt",
    BlockTransport block_transport = BlockTransport::SimpleStorage) {
    session.load_elf(image, backend, block_transport, disk_image);
    session.run_until_uart_contains(prompt, boot_max_steps);
    return session.uart_output(0);
}

inline bool wait_for_new_uart_text(DebugSession& session,
                                   size_t offset,
                                   const char* needle,
                                   std::uint64_t max_steps,
                                   const char* failure_context,
                                   DebugSession::UartOutputChunk& chunk) {
    try {
        chunk = session.run_until_new_uart_contains(offset, needle, max_steps);
        return true;
    } catch (const std::runtime_error& error) {
        chunk = session.uart_output(offset);
        std::fprintf(stderr, "%s\n", error.what());
        std::fprintf(stderr,
                     "%s failed while waiting for command UART text: %s\n",
                     failure_context,
                     needle);
        std::fprintf(stderr,
                     "command output since offset %zu was:\n%s\n",
                     offset,
                     chunk.text.c_str());
        return false;
    }
}

inline bool expect_chunk_contains(const DebugSession::UartOutputChunk& chunk,
                                  const ExpectedText* expectations,
                                  size_t expectation_count) {
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!expect_contains(chunk.text,
                             expectations[i].needle,
                             expectations[i].message)) {
            return false;
        }
    }
    return true;
}

inline bool run_shell_command(DebugSession& session,
                              size_t& offset,
                              const char* command,
                              const ExpectedText* expectations,
                              size_t expectation_count,
                              std::uint64_t max_steps,
                              const char* failure_context) {
    DebugSession::UartOutputChunk chunk{};

    session.uart_input(command);
    for (size_t i = 0; i < expectation_count; ++i) {
        if (!wait_for_new_uart_text(session,
                                    offset,
                                    expectations[i].needle,
                                    max_steps,
                                    failure_context,
                                    chunk)) {
            return false;
        }
    }
    if (!expect_chunk_contains(chunk, expectations, expectation_count)) {
        return false;
    }
    offset = chunk.next_offset;
    return true;
}

inline std::string debug_cli_run_until_uart_contains_command(const char* text,
                                                             std::uint64_t max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_uart_contains\",\"text\":\"" << text
           << "\",\"max_steps\":" << max_steps << "}\n";
    return script.str();
}

inline std::string debug_cli_run_until_halt_command(std::uint64_t max_steps) {
    std::ostringstream script;
    script << "{\"cmd\":\"run_until_halt\",\"max_steps\":" << max_steps << "}\n";
    return script.str();
}

inline std::string run_debug_cli_script(const std::string& script) {
    std::istringstream in(script);
    std::ostringstream out;
    std::ostringstream err;

    const int status = run_debug_cli(in, out, err);
    if (status != 0) {
        std::fprintf(stderr, "debug cli exited with status %d\n", status);
        std::fprintf(stderr, "stderr:\n%s\n", err.str().c_str());
        std::exit(1);
    }

    return out.str();
}

}  // namespace terminal_smoke

#include "debug_protocol.h"

#include <stdexcept>
#include <string>

#include "debug_protocol_command.h"
#include "debug_protocol_response.h"
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

BackendKind parse_backend_kind(const std::string& name) {
    if (name == "functional") {
        return BackendKind::Functional;
    }
    if (name == "pipeline") {
        return BackendKind::Pipeline;
    }
    throw std::runtime_error("unknown backend: " + name);
}

BlockTransport parse_debug_block_transport(const std::string& name) {
    return name.empty() ? BlockTransport::SimpleStorage : parse_block_transport(name);
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

            const DebugCliCommand command = parse_debug_cli_command(line);
            if (command.kind == DebugCliCommandKind::Load) {
                const BackendKind backend_kind =
                    parse_backend_kind(command.backend.empty() ? std::string("pipeline") : command.backend);
                const BlockTransport block_transport = parse_debug_block_transport(command.block_transport);
                if (command.flat) {
                    session.load_binary(
                        command.image,
                        command.addr,
                        backend_kind,
                        block_transport,
                        command.disk.empty() ? nullptr : command.disk.c_str(),
                        command.disk_ready,
                        command.disk_magic_valid,
                        command.l1d_enabled);
                } else {
                    session.load_elf(
                        command.image,
                        backend_kind,
                        block_transport,
                        command.disk.empty() ? nullptr : command.disk.c_str(),
                        command.disk_ready,
                        command.disk_magic_valid,
                        command.l1d_enabled);
                }
                out << debug_protocol_ok_json("load") << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::LoadPayload) {
                session.load_binary_payload(command.image, command.addr);
                out << debug_protocol_ok_json("load_payload") << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::SetGpr) {
                session.set_gpr(command.reg_name, command.value);
                out << debug_protocol_ok_json("set_gpr") << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::Snapshot) {
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::StepCycle) {
                for (uint64_t i = 0; i < command.count; ++i) {
                    session.step_cycle();
                }
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::StepCommit) {
                for (uint64_t i = 0; i < command.count; ++i) {
                    session.step_commit();
                }
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::RunUntilUartContains) {
                session.run_until_uart_contains(command.text, command.max_steps);
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::RunUntilHalt) {
                session.run_until_halt(command.max_steps);
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::Reset) {
                session.reset();
                out << debug_snapshot_json(session.snapshot()) << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::UartInput) {
                session.uart_input(command.text);
                out << debug_protocol_ok_json("uart_input") << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::UartOutput) {
                out << debug_protocol_uart_output_json(session.uart_output(command.offset))
                    << '\n';
                continue;
            }
            if (command.kind == DebugCliCommandKind::Quit) {
                out << debug_protocol_ok_json("quit") << '\n';
                return 0;
            }
        }
    } catch (const std::exception& ex) {
        out << debug_protocol_error_json(ex.what()) << '\n';
        err << ex.what() << '\n';
        return 1;
    }

    return 0;
}

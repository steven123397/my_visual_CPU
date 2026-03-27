#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "../../src/debug/debug_protocol.h"

namespace {

constexpr uint64_t kDebugProgramAddr = 0x80000078ULL;

constexpr std::array<uint32_t, 45> kSupervisorExternalProgram = {
    0x10000437U, 0x0c0004b7U, 0x00000297U, 0x08428293U, 0x10529073U, 0x00100293U, 0x0054a223U,
    0x00200293U, 0x00002337U, 0x0803031bU, 0x00648333U, 0x00532023U, 0x00201337U, 0x00648333U,
    0x00032023U, 0x20000293U, 0x30329073U, 0x10429073U, 0x000012b7U, 0x8022829bU, 0x30029073U,
    0x00200293U, 0x005400a3U, 0x00000297U, 0x01028293U, 0x34129073U, 0x30200073U, 0x0000006fU,
    0x05000293U, 0x00540023U, 0x04500293U, 0x00540023U, 0x05d00893U, 0x00000513U, 0x00000073U,
    0x00201337U, 0x0043031bU, 0x00648333U, 0x00032383U, 0x000400a3U, 0x00732023U, 0x00000297U,
    0xfcc28293U, 0x14129073U, 0x10200073U,
};

constexpr std::array<uint32_t, 39> kSupervisorTimerProgram = {
    0x10000437U, 0x020004b7U, 0x00000297U, 0x06c28293U, 0x10529073U, 0x0000c2b7U, 0xff82829bU,
    0x005482b3U, 0x0002b303U, 0x00430313U, 0x000043b7U, 0x007483b3U, 0x0063b023U, 0x02000293U,
    0x30329073U, 0x10429073U, 0x000012b7U, 0x8022829bU, 0x30029073U, 0x00000297U, 0x01028293U,
    0x34129073U, 0x30200073U, 0x0000006fU, 0x05400293U, 0x00540023U, 0x05d00893U, 0x00000513U,
    0x00000073U, 0xfff00293U, 0x00004337U, 0x00648333U, 0x00533023U, 0x02000293U, 0x1442b073U,
    0x00000297U, 0xfd428293U, 0x14129073U, 0x10200073U,
};

constexpr std::array<uint32_t, 4> kPredictorProgram = {
    0x0080006fU,
    0x06300093U,
    0x05d00893U,
    0x00000073U,
};

bool expect_contains(const std::string& haystack, const char* needle, const char* message) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "output was:\n%s\n", haystack.c_str());
        return false;
    }
    return true;
}

template <size_t N>
std::string write_temp_binary(const char* tag, const std::array<uint32_t, N>& words) {
    char path[] = "/tmp/debug_cli_smoke_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) {
        std::fprintf(stderr, "failed to create temp binary for %s\n", tag);
        std::exit(1);
    }

    for (uint32_t word : words) {
        const unsigned char bytes[4] = {
            static_cast<unsigned char>(word & 0xffU),
            static_cast<unsigned char>((word >> 8) & 0xffU),
            static_cast<unsigned char>((word >> 16) & 0xffU),
            static_cast<unsigned char>((word >> 24) & 0xffU),
        };
        if (::write(fd, bytes, sizeof(bytes)) != static_cast<ssize_t>(sizeof(bytes))) {
            std::fprintf(stderr, "failed to write temp binary for %s\n", tag);
            ::close(fd);
            ::unlink(path);
            std::exit(1);
        }
    }

    ::close(fd);
    return path;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

bool expect_line_with_fields(const std::vector<std::string>& lines,
                             const std::string& output,
                             std::initializer_list<const char*> needles,
                             const char* message) {
    for (const std::string& line : lines) {
        bool matched = true;
        for (const char* needle : needles) {
            if (line.find(needle) == std::string::npos) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    std::fprintf(stderr, "%s\n", message);
    std::fprintf(stderr, "output was:\n%s\n", output.c_str());
    return false;
}

std::string build_flat_load_command(const std::string& path) {
    std::ostringstream command;
    command << "{\"cmd\":\"load\",\"image\":\"" << path
            << "\",\"backend\":\"pipeline\",\"flat\":true,\"addr\":\"0x" << std::hex << kDebugProgramAddr
            << "\"}\n";
    return command.str();
}

std::string repeat_command(const char* command, int count) {
    std::ostringstream script;
    for (int i = 0; i < count; ++i) {
        script << command << '\n';
    }
    return script.str();
}

std::string run_cli_script(const std::string& script) {
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

struct TempBinary {
    std::string path{};

    ~TempBinary() {
        if (!path.empty()) {
            ::unlink(path.c_str());
        }
    }
};

}  // namespace

int main() {
    const TempBinary external_binary{write_temp_binary("external", kSupervisorExternalProgram)};
    const TempBinary timer_binary{write_temp_binary("timer", kSupervisorTimerProgram)};
    const TempBinary predictor_binary{write_temp_binary("predictor", kPredictorProgram)};

    const std::string external_pending_output =
        run_cli_script(build_flat_load_command(external_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 40) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> external_pending_lines = split_lines(external_pending_output);
    if (!expect_contains(external_pending_output, "\"cmd\":\"load\"", "load response should be emitted")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"type\":\"snapshot\"", "snapshot response should be emitted")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"backend\":\"pipeline\"", "snapshot should report pipeline backend")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"pipeline\"", "snapshot should include pipeline section")) {
        return 1;
    }
    if (!expect_contains(external_pending_output, "\"gpr\"", "snapshot should include register state")) {
        return 1;
    }
    if (!expect_line_with_fields(
            external_pending_lines,
            external_pending_output,
            {
                "\"halted\":false",
                "\"privilege\":\"S\"",
                "\"ier\":2",
                "\"pending\":true",
                "\"supervisor_has_pending\":true",
                "\"scause\":\"0x8000000000000009\"",
            },
            "external interrupt pending snapshot should expose delegated supervisor interrupt state")) {
        return 1;
    }

    const std::string external_final_output =
        run_cli_script(build_flat_load_command(external_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 64) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> external_final_lines = split_lines(external_final_output);
    if (!expect_line_with_fields(
            external_final_lines,
            external_final_output,
            {
                "\"halted\":true",
                "\"privilege\":\"S\"",
                "\"trap_flush\":true",
                "\"recent_output\":\"PE\"",
                "\"ier\":0",
                "\"pending\":false",
                "\"claimed\":false",
                "\"supervisor_has_pending\":false",
                "\"scause\":\"0x8000000000000009\"",
                "\"kind\":\"trap\"",
                "\"kind\":\"flush\"",
                "\"kind\":\"halt\"",
            },
            "external interrupt final snapshot should expose handler completion and pipeline events")) {
        return 1;
    }

    const std::string timer_pending_output =
        run_cli_script(build_flat_load_command(timer_binary.path) +
                       repeat_command("{\"cmd\":\"step_cycle\"}", 35) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> timer_pending_lines = split_lines(timer_pending_output);
    if (!expect_line_with_fields(
            timer_pending_lines,
            timer_pending_output,
            {
                "\"halted\":false",
                "\"privilege\":\"M\"",
                "\"timer_interrupt_pending\":true",
                "\"mip\":\"0x20\"",
                "\"sip\":\"0x20\"",
            },
            "timer snapshot should expose pending CLINT state before delegated delivery")) {
        return 1;
    }

    const std::string timer_final_output =
        run_cli_script(build_flat_load_command(timer_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 64) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> timer_final_lines = split_lines(timer_final_output);
    if (!expect_line_with_fields(
            timer_final_lines,
            timer_final_output,
            {
                "\"halted\":true",
                "\"privilege\":\"S\"",
                "\"trap_flush\":true",
                "\"recent_output\":\"T\"",
                "\"timer_interrupt_pending\":false",
                "\"scause\":\"0x8000000000000005\"",
                "\"kind\":\"trap\"",
                "\"kind\":\"flush\"",
                "\"kind\":\"halt\"",
            },
            "timer final snapshot should expose delegated CLINT interrupt completion and pipeline events")) {
        return 1;
    }

    if (!expect_contains(timer_final_output, "\"cmd\":\"quit\"", "quit response should be emitted")) {
        return 1;
    }

    const std::string predictor_output =
        run_cli_script(build_flat_load_command(predictor_binary.path) +
                       repeat_command("{\"cmd\":\"step_commit\"}", 16) + "{\"cmd\":\"quit\"}\n");
    const std::vector<std::string> predictor_lines = split_lines(predictor_output);
    if (!expect_line_with_fields(
            predictor_lines,
            predictor_output,
            {
                "\"halted\":true",
                "\"backend\":\"pipeline\"",
                "\"predictor\"",
                "\"mode\":\"bimodal-2bit\"",
                "\"last_prediction_valid\":true",
                "\"last_prediction_taken\":true",
                "\"last_prediction_correct\":true",
                "\"last_prediction_pc\":\"0x80000078\"",
                "\"last_prediction_target\":\"0x80000080\"",
                "\"total_predictions\":1",
                "\"correct_predictions\":1",
                "\"mispredictions\":0",
            },
            "predictor snapshot should expose branch prediction state and counters")) {
        return 1;
    }

    return 0;
}

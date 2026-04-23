#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

#include "../../src/devices/ai_graph_package.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_file_exists(const std::filesystem::path& path, const char* message) {
    if (!std::filesystem::exists(path)) {
        std::fprintf(stderr, "%s: %s\n", message, path.string().c_str());
        return false;
    }
    return true;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool expect_contains(const std::string& text, const char* needle, const char* message) {
    if (text.find(needle) == std::string::npos) {
        std::fprintf(stderr, "%s\n", message);
        std::fprintf(stderr, "text was:\n%s\n", text.c_str());
        return false;
    }
    return true;
}

struct CommandResult {
    int exit_code{0};
    std::string output{};
};

CommandResult run_command(const std::string& command) {
    CommandResult result{};
    std::array<char, 256> buffer{};
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr) {
        result.exit_code = 127;
        result.output = "popen failed";
        return result;
    }
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }
    const int status = pclose(pipe);
    result.exit_code = status == -1 ? 127 : WEXITSTATUS(status);
    return result;
}

bool expect_pack_and_profile(const std::filesystem::path& temp_dir,
                             const char* workload,
                             const char* expected_name,
                             uint64_t expected_device_cycles,
                             uint64_t expected_dma_cycles,
                             uint64_t expected_compute_cycles,
                             uint64_t expected_bytes_moved,
                             uint64_t expected_retired_ops) {
    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --workload " + std::string(workload) +
        " --out-dir " + temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected ai_proto pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / (std::string(workload) + ".manifest");
    const std::filesystem::path graph = temp_dir / (std::string(workload) + ".graph.bin");
    const std::filesystem::path actual = temp_dir / (std::string(workload) + ".output0.actual.bin");
    const std::filesystem::path expected = temp_dir / (std::string(workload) + ".output0.expected.bin");
    if (!expect_file_exists(manifest, "expected ai_proto manifest") ||
        !expect_file_exists(graph, "expected ai_proto graph package") ||
        !expect_file_exists(expected, "expected ai_proto expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(!package.ops.empty(), "expected packaged graph ops") ||
        !expect(package.scratchpad_budget_bytes != 0, "expected packaged scratchpad budget")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, expected_name, "expected workload name in profile output") ||
        !expect_contains(profile.output, "progress=completed", "expected completed progress in profile output") ||
        !expect_contains(profile.output, "baseline=none", "expected raw-counter baseline summary") ||
        !expect_contains(profile.output,
                         ("device_cycles=" + std::to_string(expected_device_cycles)).c_str(),
                         "expected device cycle summary") ||
        !expect_contains(profile.output,
                         ("dma_cycles=" + std::to_string(expected_dma_cycles)).c_str(),
                         "expected DMA cycle summary") ||
        !expect_contains(profile.output,
                         ("compute_cycles=" + std::to_string(expected_compute_cycles)).c_str(),
                         "expected compute cycle summary") ||
        !expect_contains(profile.output,
                         ("bytes_moved=" + std::to_string(expected_bytes_moved)).c_str(),
                         "expected bytes moved summary") ||
        !expect_contains(profile.output,
                         ("retired_ops=" + std::to_string(expected_retired_ops)).c_str(),
                         "expected retired op summary") ||
        !expect_file_exists(actual, "expected ai profile actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected ai profile output to match packaged expectation");
}

}  // namespace

int main() {
    try {
        const std::filesystem::path profile_mk = "workloads/ai_proto/profile.mk";
        const std::filesystem::path pack_graph = "workloads/ai_proto/pack_graph.py";
        const std::filesystem::path readme = "workloads/ai_proto/README.md";

        if (!expect_file_exists(profile_mk, "ai profile smoke expects workload profile") ||
            !expect_file_exists(pack_graph, "ai profile smoke expects packer script") ||
            !expect_file_exists(readme, "ai profile smoke expects ai workload readme")) {
            return 1;
        }

        const std::string profile_text = read_text_file(profile_mk);
        const CommandResult make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=cnn");
        if (!expect(make_run.exit_code == 0, "expected ai workload make dry-run to succeed") ||
            !expect_contains(profile_text,
                             "WORKLOAD_RUN_MODE := ai-profile",
                             "expected ai_proto workload run mode") ||
            !expect_contains(make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/cnn.manifest",
                             "expected ai workload dry-run manifest argument")) {
            std::fprintf(stderr, "%s\n", make_run.output.c_str());
            return 1;
        }

        const std::filesystem::path temp_dir =
            std::filesystem::temp_directory_path() / "mycpu_ai_proto_profile_smoke";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        const bool ok =
            expect_pack_and_profile(temp_dir, "cnn", "name=cnn", 18, 9, 9, 32, 63) &&
            expect_pack_and_profile(temp_dir, "gemm", "name=gemm", 13, 9, 4, 20, 12);

        std::filesystem::remove_all(temp_dir);
        if (!ok) {
            return 1;
        }

        std::puts("ai_accelerator_profile_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

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

void write_text_file(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    out << text;
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

std::filesystem::path manifest_without_format(const std::filesystem::path& source,
                                              const std::filesystem::path& destination) {
    std::istringstream input(read_text_file(source));
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("format=", 0) == 0) {
            continue;
        }
        output << line << '\n';
    }
    write_text_file(destination, output.str());
    return destination;
}

std::filesystem::path manifest_with_duplicate_scalar_key(const std::filesystem::path& source,
                                                         const std::filesystem::path& destination,
                                                         const char* key,
                                                         const char* duplicate_value) {
    const std::string key_prefix = std::string(key) + "=";
    std::istringstream input(read_text_file(source));
    std::ostringstream output;
    std::string line;
    bool inserted_duplicate = false;
    while (std::getline(input, line)) {
        output << line << '\n';
        if (!inserted_duplicate && line.rfind(key_prefix, 0) == 0) {
            output << key_prefix << duplicate_value << '\n';
            inserted_duplicate = true;
        }
    }
    write_text_file(destination, output.str());
    return destination;
}

bool expect_profile_failure(const std::filesystem::path& manifest,
                            const char* expected_error,
                            const char* message) {
    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code != 0, message)) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }
    return expect_contains(profile.output, expected_error, "expected specific ai profile failure reason");
}

bool expect_pack_and_profile(const std::filesystem::path& temp_dir,
                             const char* workload,
                             const char* expected_name,
                             uint64_t expected_device_cycles,
                             uint64_t expected_dma_cycles,
                             uint64_t expected_compute_cycles,
                             uint64_t expected_busy_cycles,
                             uint64_t expected_queue_cycles,
                             uint64_t expected_completion_cycles,
                             uint64_t expected_effective_ops_per_cycle,
                             uint64_t expected_utilization,
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
                         ("busy_cycles=" + std::to_string(expected_busy_cycles)).c_str(),
                         "expected busy cycle summary") ||
        !expect_contains(profile.output,
                         ("queue_cycles=" + std::to_string(expected_queue_cycles)).c_str(),
                         "expected queue cycle summary") ||
        !expect_contains(profile.output,
                         ("completion_cycles=" + std::to_string(expected_completion_cycles)).c_str(),
                         "expected completion cycle summary") ||
        !expect_contains(profile.output,
                         ("effective_ops_per_cycle=" +
                          std::to_string(expected_effective_ops_per_cycle)).c_str(),
                         "expected effective ops per cycle summary") ||
        !expect_contains(profile.output,
                         ("utilization=" + std::to_string(expected_utilization)).c_str(),
                         "expected utilization summary") ||
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

bool expect_pack_and_profile_dynamic(const std::filesystem::path& temp_dir) {
    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --workload dynamic_gemm --out-dir " +
        temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected dynamic_gemm pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "dynamic_gemm.manifest";
    const std::filesystem::path graph = temp_dir / "dynamic_gemm.graph.bin";
    const std::filesystem::path runtime_shape = temp_dir / "dynamic_gemm.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "dynamic_gemm.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "dynamic_gemm.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected dynamic_gemm manifest") ||
        !expect_file_exists(graph, "expected dynamic_gemm graph package") ||
        !expect_file_exists(runtime_shape, "expected dynamic_gemm runtime shape table") ||
        !expect_file_exists(expected, "expected dynamic_gemm expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected dynamic_gemm packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 2,
                "expected dynamic_gemm packaged graph dynamic tensors")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected dynamic_gemm ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, "name=dynamic_gemm", "expected dynamic_gemm workload name") ||
        !expect_contains(profile.output, "shape_mode=dynamic_bounded", "expected dynamic shape mode summary") ||
        !expect_contains(profile.output, "runtime_shapes=t0:2x8,t2:2x4", "expected runtime shape summary") ||
        !expect_contains(profile.output, "device_cycles=15", "expected dynamic_gemm device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=11", "expected dynamic_gemm DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=4", "expected dynamic_gemm compute cycle summary") ||
        !expect_contains(profile.output, "busy_cycles=17", "expected dynamic_gemm busy cycle summary") ||
        !expect_contains(profile.output, "queue_cycles=1", "expected dynamic_gemm queue cycle summary") ||
        !expect_contains(profile.output, "completion_cycles=1", "expected dynamic_gemm completion cycle summary") ||
        !expect_contains(profile.output, "effective_ops_per_cycle=16", "expected dynamic_gemm op/cycle summary") ||
        !expect_contains(profile.output, "utilization=23", "expected dynamic_gemm utilization summary") ||
        !expect_contains(profile.output, "bytes_moved=80", "expected dynamic_gemm bytes moved summary") ||
        !expect_contains(profile.output, "retired_ops=64", "expected dynamic_gemm retired op summary") ||
        !expect_file_exists(actual, "expected dynamic_gemm actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected dynamic_gemm output to match packaged expectation");
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
        const CommandResult tiny_model_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model");
        const CommandResult dynamic_gemm_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_gemm");
        if (!expect(make_run.exit_code == 0, "expected ai workload make dry-run to succeed") ||
            !expect(tiny_model_make_run.exit_code == 0,
                    "expected tiny model ai workload make dry-run to succeed") ||
            !expect(dynamic_gemm_make_run.exit_code == 0,
                    "expected dynamic_gemm ai workload make dry-run to succeed") ||
            !expect_contains(profile_text,
                             "WORKLOAD_RUN_MODE := ai-profile",
                             "expected ai_proto workload run mode") ||
            !expect_contains(make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/cnn.manifest",
                             "expected ai workload dry-run manifest argument") ||
            !expect_contains(tiny_model_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/tiny_model.manifest",
                             "expected tiny model dry-run manifest argument") ||
            !expect_contains(dynamic_gemm_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/dynamic_gemm.manifest",
                             "expected dynamic_gemm dry-run manifest argument")) {
            std::fprintf(stderr, "%s\n", make_run.output.c_str());
            return 1;
        }

        const std::filesystem::path temp_dir =
            std::filesystem::temp_directory_path() / "mycpu_ai_proto_profile_smoke";
        std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directories(temp_dir);

        const std::filesystem::path malformed_dir = temp_dir / "malformed";
        const CommandResult malformed_pack = run_command(
            "python3 workloads/ai_proto/pack_graph.py --workload cnn --out-dir " +
            malformed_dir.string());
        if (!expect(malformed_pack.exit_code == 0, "expected malformed-input pack command to succeed")) {
            std::fprintf(stderr, "%s\n", malformed_pack.output.c_str());
            std::filesystem::remove_all(temp_dir);
            return 1;
        }

        const std::filesystem::path manifest = malformed_dir / "cnn.manifest";
        const std::filesystem::path no_format_manifest =
            manifest_without_format(manifest, malformed_dir / "cnn.no_format.manifest");
        const std::filesystem::path duplicate_name_manifest =
            manifest_with_duplicate_scalar_key(manifest,
                                               malformed_dir / "cnn.dup_name.manifest",
                                               "name",
                                               "cnn-duplicate");

        const bool ok =
            expect_profile_failure(no_format_manifest,
                                   "AI profile manifest is missing format",
                                   "expected ai profile manifest without format to fail") &&
            expect_profile_failure(duplicate_name_manifest,
                                   "duplicate AI profile manifest key: name",
                                   "expected ai profile manifest with duplicate name to fail") &&
            expect_pack_and_profile(temp_dir, "cnn", "name=cnn", 18, 9, 9, 20, 1, 1, 7, 45, 32, 63) &&
            expect_pack_and_profile(temp_dir, "gemm", "name=gemm", 13, 9, 4, 15, 1, 1, 3, 26, 20, 12) &&
            expect_pack_and_profile(temp_dir,
                                    "tiny_model",
                                    "name=tiny_model",
                                    15,
                                    9,
                                    6,
                                    17,
                                    1,
                                    1,
                                    3,
                                    35,
                                    28,
                                    20) &&
            expect_pack_and_profile_dynamic(temp_dir);

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

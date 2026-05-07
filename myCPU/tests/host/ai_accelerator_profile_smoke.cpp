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
#include "../../src/platform/machine.h"

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

void write_binary_file(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
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

uint32_t align_up_u32(uint32_t value, uint32_t alignment) {
    const uint32_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

const char* ai_tensor_role_name_local(AiTensorRole role) {
    switch (role) {
    case AiTensorRole::Input:
        return "input";
    case AiTensorRole::Output:
        return "output";
    case AiTensorRole::Weight:
        return "weight";
    case AiTensorRole::Intermediate:
        return "intermediate";
    case AiTensorRole::Constant:
        return "constant";
    case AiTensorRole::Invalid:
        return "invalid";
    }
    return "unknown";
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

std::filesystem::path manifest_without_scalar_key(const std::filesystem::path& source,
                                                  const std::filesystem::path& destination,
                                                  const char* key) {
    const std::string key_prefix = std::string(key) + "=";
    std::istringstream input(read_text_file(source));
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind(key_prefix, 0) == 0) {
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

std::filesystem::path manifest_with_inserted_scalar_key(const std::filesystem::path& source,
                                                        const std::filesystem::path& destination,
                                                        const char* after_key,
                                                        const char* new_key,
                                                        const char* new_value) {
    const std::string after_prefix = std::string(after_key) + "=";
    const std::string inserted_line = std::string(new_key) + "=" + new_value;
    std::istringstream input(read_text_file(source));
    std::ostringstream output;
    std::string line;
    bool inserted = false;
    while (std::getline(input, line)) {
        output << line << '\n';
        if (!inserted && line.rfind(after_prefix, 0) == 0) {
            output << inserted_line << '\n';
            inserted = true;
        }
    }
    if (!inserted) {
        output << inserted_line << '\n';
    }
    write_text_file(destination, output.str());
    return destination;
}

std::filesystem::path manifest_with_replaced_scalar_key(const std::filesystem::path& source,
                                                        const std::filesystem::path& destination,
                                                        const char* key,
                                                        const char* new_value) {
    const std::string key_prefix = std::string(key) + "=";
    std::istringstream input(read_text_file(source));
    std::ostringstream output;
    std::string line;
    bool replaced = false;
    while (std::getline(input, line)) {
        if (line.rfind(key_prefix, 0) == 0) {
            output << key_prefix << new_value << '\n';
            replaced = true;
        } else {
            output << line << '\n';
        }
    }
    if (!replaced) {
        output << key_prefix << new_value << '\n';
    }
    write_text_file(destination, output.str());
    return destination;
}

std::filesystem::path graph_with_faulty_pool_attr(const std::filesystem::path& source,
                                                  const std::filesystem::path& destination) {
    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(source), package, error)) {
        throw std::runtime_error("failed to parse source graph package: " + error);
    }
    if (package.ops.size() < 2 || package.ops[1].opcode != AiOpCode::PoolMax) {
        throw std::runtime_error("faulty pool graph source is missing expected PoolMax op");
    }
    package.ops[1].attrs[0] = 0;
    std::vector<uint8_t> bytes{};
    if (!serialize_ai_graph_package(package, bytes, error)) {
        throw std::runtime_error("failed to serialize faulty pool graph package: " + error);
    }
    write_binary_file(destination, bytes);
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

bool expect_task_spec_pack_failure(const std::filesystem::path& task_spec,
                                   const std::filesystem::path& out_dir,
                                   const char* expected_error,
                                   const char* message) {
    const CommandResult pack = run_command(
        "python3 workloads/ai_proto/pack_graph.py --task-spec " + task_spec.string() +
        " --out-dir " + out_dir.string());
    if (!expect(pack.exit_code != 0, message)) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }
    return expect_contains(pack.output, expected_error, "expected specific task spec pack failure reason");
}

bool expect_matching_binary_file(const std::filesystem::path& actual,
                                 const std::filesystem::path& expected,
                                 const char* context) {
    return expect_file_exists(actual, context) &&
           expect_file_exists(expected, context) &&
           expect(read_binary_file(actual) == read_binary_file(expected), context);
}

bool expect_matching_text_file(const std::filesystem::path& actual,
                               const std::filesystem::path& expected,
                               const char* context) {
    return expect_file_exists(actual, context) &&
           expect_file_exists(expected, context) &&
           expect(read_text_file(actual) == read_text_file(expected), context);
}

bool expect_memory_plan_summary_matches(const std::filesystem::path& summary_path,
                                        const AiGraphPackage& package,
                                        const char* expected_shape_mode,
                                        const char* context) {
    if (!expect_file_exists(summary_path, context)) {
        return false;
    }
    const std::string summary = read_text_file(summary_path);
    if (!expect_contains(summary, "format=ai_proto_memory_plan_v1", context) ||
        !expect_contains(summary,
                         ("shape_mode=" + std::string(expected_shape_mode)).c_str(),
                         context) ||
        !expect_contains(summary,
                         ("scratchpad_budget_bytes=" +
                          std::to_string(package.scratchpad_budget_bytes)).c_str(),
                         context) ||
        !expect_contains(summary,
                         ("tensor_count=" + std::to_string(package.tensors.size())).c_str(),
                         context) ||
        !expect_contains(summary,
                         ("memory_plan_entries=" + std::to_string(package.memory_plan.size())).c_str(),
                         context)) {
        return false;
    }
    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        if (!expect(entry.tensor_index < package.tensors.size(), context)) {
            return false;
        }
        const AiTensorMetadata& tensor = package.tensors[entry.tensor_index];
        const std::string line =
            "memory_plan_entry tensor_index=" + std::to_string(entry.tensor_index) +
            " role=" + ai_tensor_role_name_local(tensor.role) +
            " dtype=" + std::string(ai_dtype_name(tensor.dtype)) +
            " system_offset=" + std::to_string(entry.system_offset) +
            " scratchpad_offset=" + std::to_string(entry.scratchpad_offset) +
            " byte_size=" + std::to_string(entry.byte_size) +
            " scratchpad_bytes=" + std::to_string(entry.scratchpad_bytes);
        if (!expect_contains(summary, line.c_str(), context)) {
            return false;
        }
    }
    return true;
}

bool expect_resolved_memory_plan_summary_matches(const std::filesystem::path& summary_path,
                                                 const AiGraphPackage& package,
                                                 const std::filesystem::path& runtime_shape_path,
                                                 const char* context) {
    if (!expect_file_exists(runtime_shape_path, context)) {
        return false;
    }
    std::vector<AiRuntimeShapeEntry> runtime_shapes{};
    std::string error;
    if (!parse_ai_runtime_shape_table(read_binary_file(runtime_shape_path),
                                      package.dynamic_tensors.size(),
                                      runtime_shapes,
                                      error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    AiGraphPackage resolved_package{};
    if (!resolve_ai_runtime_shape_package(package, runtime_shapes, resolved_package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    return expect_memory_plan_summary_matches(summary_path, resolved_package, "static", context);
}

bool expect_default_timing_model(const AiAcceleratorProfileSummary& summary, const char* context) {
    return expect(summary.timing_model == AiAcceleratorTimingModel::TimedSimpleNoOverlap, context) &&
           expect(summary.scheduler_ops_per_cycle == 32, context) &&
           expect(summary.scheduler_tile_setup_cycles == 1, context) &&
           expect(!summary.allow_dma_compute_overlap, context) &&
           expect(summary.dma_setup_cycles == 2, context) &&
           expect(summary.dma_bytes_per_cycle == 16, context);
}

bool expect_submission_timing(const AiAcceleratorProfileSummary& summary,
                              uint64_t expected_device_cycles,
                              uint64_t expected_dma_cycles,
                              uint64_t expected_compute_cycles,
                              uint64_t expected_stall_cycles,
                              uint64_t expected_queue_cycles,
                              uint64_t expected_completion_cycles,
                              uint64_t expected_busy_cycles,
                              const char* context) {
    return expect(summary.last_submission_device_cycles == expected_device_cycles, context) &&
           expect(summary.last_submission_dma_cycles == expected_dma_cycles, context) &&
           expect(summary.last_submission_compute_cycles == expected_compute_cycles, context) &&
           expect(summary.last_submission_stall_cycles == expected_stall_cycles, context) &&
           expect(summary.last_submission_queue_cycles == expected_queue_cycles, context) &&
           expect(summary.last_submission_completion_cycles == expected_completion_cycles, context) &&
           expect(summary.last_submission_busy_cycles == expected_busy_cycles, context);
}

bool expect_submission_outcome(const AiAcceleratorProfileSummary& summary,
                               uint32_t expected_fault,
                               uint64_t expected_retired_ops,
                               uint64_t expected_bytes_moved,
                               const char* context) {
    return expect(summary.last_submission_fault == expected_fault, context) &&
           expect(summary.last_submission_retired_ops == expected_retired_ops, context) &&
           expect(summary.last_submission_bytes_moved == expected_bytes_moved, context);
}

bool expect_submission_dma_breakdown(const AiAcceleratorProfileSummary& summary,
                                     uint64_t expected_load_cycles,
                                     uint64_t expected_store_cycles,
                                     uint64_t expected_load_bytes,
                                     uint64_t expected_store_bytes,
                                     const char* context) {
    return expect(summary.last_submission_dma_load_cycles == expected_load_cycles, context) &&
           expect(summary.last_submission_dma_store_cycles == expected_store_cycles, context) &&
           expect(summary.last_submission_dma_load_bytes == expected_load_bytes, context) &&
           expect(summary.last_submission_dma_store_bytes == expected_store_bytes, context);
}

struct ExpectedManifestCompileContract {
    AiShapeMode shape_mode{AiShapeMode::Static};
    uint32_t runtime_shape_count{0};
    uint32_t tensor_count{0};
    uint32_t memory_plan_entries{0};
    uint32_t dynamic_tensor_count{0};
    uint32_t input_tensor_count{0};
    uint32_t output_tensor_count{0};
    uint32_t weight_tensor_count{0};
    uint32_t constant_tensor_count{0};
    uint32_t intermediate_tensor_count{0};
    uint32_t scratchpad_budget_bytes{0};
    uint32_t op_count{0};
    uint32_t dependency_count{0};
    uint32_t root_op_count{0};
    uint32_t leaf_op_count{0};
    uint32_t dependency_depth{0};
    uint32_t max_fanin{0};
    uint32_t max_fanout{0};
    uint32_t load_entry_count{0};
    uint32_t store_entry_count{0};
    uint32_t load_plan_bytes{0};
    uint32_t store_plan_bytes{0};
    uint64_t token{0xA1A1A1A1ULL};
    uint32_t flags{AI_ACCEL_SUBMISSION_FLAG_PROFILE};
    uint64_t graph_package_addr{MEM_BASE + 0x26000};
    uint64_t input_table_addr{MEM_BASE + 0x28000};
    uint64_t output_table_addr{MEM_BASE + 0x29000};
    uint64_t submission_base_snapshot{MEM_BASE + 0x22000};
    uint64_t completion_base_snapshot{MEM_BASE + 0x24000};
    uint32_t graph_package_bytes{0};
    uint32_t runtime_shape_table_offset{0};
    uint64_t runtime_shape_table_addr{0};
    uint32_t source_tag{0};
    uint32_t queue_depth_snapshot{1};
    uint32_t submission_queue_size_snapshot{4};
    uint32_t completion_queue_size_snapshot{4};
    uint32_t submission_head_snapshot{0};
    uint32_t submission_tail_snapshot{1};
    uint32_t completion_head_snapshot{0};
    uint32_t completion_tail_snapshot{0};
    bool queue_configured_snapshot{true};
};

ExpectedManifestCompileContract expected_manifest_compile_contract(const std::filesystem::path& manifest,
                                                                  uint32_t graph_package_bytes) {
    auto count_roots = [](const AiGraphPackage& package) -> uint32_t {
        if (package.ops.empty()) {
            return 0;
        }
        std::vector<uint32_t> indegree(package.ops.size(), 0);
        for (const AiDependencyEdge& edge : package.dependencies) {
            if (edge.target_op < indegree.size()) {
                ++indegree[edge.target_op];
            }
        }
        uint32_t count = 0;
        for (uint32_t degree : indegree) {
            if (degree == 0) {
                ++count;
            }
        }
        return count;
    };
    auto count_leaves = [](const AiGraphPackage& package) -> uint32_t {
        if (package.ops.empty()) {
            return 0;
        }
        std::vector<uint32_t> outdegree(package.ops.size(), 0);
        for (const AiDependencyEdge& edge : package.dependencies) {
            if (edge.source_op < outdegree.size()) {
                ++outdegree[edge.source_op];
            }
        }
        uint32_t count = 0;
        for (uint32_t degree : outdegree) {
            if (degree == 0) {
                ++count;
            }
        }
        return count;
    };
    auto count_max_fanin = [](const AiGraphPackage& package) -> uint32_t {
        if (package.ops.empty()) {
            return 0;
        }
        std::vector<uint32_t> indegree(package.ops.size(), 0);
        for (const AiDependencyEdge& edge : package.dependencies) {
            if (edge.target_op < indegree.size()) {
                ++indegree[edge.target_op];
            }
        }
        uint32_t max_fanin = 0;
        for (uint32_t degree : indegree) {
            max_fanin = std::max(max_fanin, degree);
        }
        return max_fanin;
    };
    auto count_max_fanout = [](const AiGraphPackage& package) -> uint32_t {
        if (package.ops.empty()) {
            return 0;
        }
        std::vector<uint32_t> outdegree(package.ops.size(), 0);
        for (const AiDependencyEdge& edge : package.dependencies) {
            if (edge.source_op < outdegree.size()) {
                ++outdegree[edge.source_op];
            }
        }
        uint32_t max_fanout = 0;
        for (uint32_t degree : outdegree) {
            max_fanout = std::max(max_fanout, degree);
        }
        return max_fanout;
    };
    auto count_max_depth = [](const AiGraphPackage& package) -> uint32_t {
        if (package.ops.empty()) {
            return 0;
        }
        std::vector<uint32_t> indegree(package.ops.size(), 0);
        std::vector<std::vector<uint32_t>> adjacency(package.ops.size());
        for (const AiDependencyEdge& edge : package.dependencies) {
            if (edge.source_op < adjacency.size() && edge.target_op < indegree.size()) {
                adjacency[edge.source_op].push_back(edge.target_op);
                ++indegree[edge.target_op];
            }
        }
        std::vector<uint32_t> ready{};
        ready.reserve(package.ops.size());
        std::vector<uint32_t> depth(package.ops.size(), 1);
        for (uint32_t op = 0; op < indegree.size(); ++op) {
            if (indegree[op] == 0) {
                ready.push_back(op);
            }
        }
        size_t cursor = 0;
        uint32_t max_depth = ready.empty() ? 0 : 1;
        while (cursor < ready.size()) {
            const uint32_t source = ready[cursor++];
            max_depth = std::max(max_depth, depth[source]);
            for (uint32_t target : adjacency[source]) {
                depth[target] = std::max(depth[target], depth[source] + 1);
                if (indegree[target] > 0) {
                    --indegree[target];
                    if (indegree[target] == 0) {
                        ready.push_back(target);
                    }
                }
            }
        }
        if (ready.size() != package.ops.size()) {
            return 0;
        }
        return max_depth;
    };
    auto count_transfers = [](const AiGraphPackage& package,
                              uint32_t& load_entries,
                              uint32_t& store_entries,
                              uint32_t& load_plan_bytes,
                              uint32_t& store_plan_bytes) {
        load_entries = 0;
        store_entries = 0;
        load_plan_bytes = 0;
        store_plan_bytes = 0;
        for (const AiMemoryPlanEntry& entry : package.memory_plan) {
            if (entry.tensor_index >= package.tensors.size()) {
                continue;
            }
            const AiTensorRole role = package.tensors[entry.tensor_index].role;
            if (role == AiTensorRole::Output) {
                ++store_entries;
                store_plan_bytes += entry.byte_size;
            } else if (role != AiTensorRole::Intermediate && role != AiTensorRole::Invalid) {
                ++load_entries;
                load_plan_bytes += entry.byte_size;
            }
        }
    };

    const std::filesystem::path manifest_dir = manifest.parent_path();
    const std::string graph_name = manifest.stem().string();
    const std::filesystem::path graph_path = manifest_dir / (graph_name + ".graph.bin");
    AiGraphPackage packaged{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph_path), packaged, error)) {
        throw std::runtime_error("failed to parse manifest graph package: " + error);
    }

    ExpectedManifestCompileContract expected{};
    expected.shape_mode = packaged.shape_mode;
    expected.tensor_count = static_cast<uint32_t>(packaged.tensors.size());
    expected.memory_plan_entries = static_cast<uint32_t>(packaged.memory_plan.size());
    expected.dynamic_tensor_count = static_cast<uint32_t>(packaged.dynamic_tensors.size());
    for (const AiTensorMetadata& tensor : packaged.tensors) {
        switch (tensor.role) {
        case AiTensorRole::Input:
            ++expected.input_tensor_count;
            break;
        case AiTensorRole::Output:
            ++expected.output_tensor_count;
            break;
        case AiTensorRole::Weight:
            ++expected.weight_tensor_count;
            break;
        case AiTensorRole::Constant:
            ++expected.constant_tensor_count;
            break;
        case AiTensorRole::Intermediate:
            ++expected.intermediate_tensor_count;
            break;
        case AiTensorRole::Invalid:
            break;
        }
    }
    expected.scratchpad_budget_bytes = packaged.scratchpad_budget_bytes;
    expected.op_count = static_cast<uint32_t>(packaged.ops.size());
    expected.dependency_count = static_cast<uint32_t>(packaged.dependencies.size());
    expected.root_op_count = count_roots(packaged);
    expected.leaf_op_count = count_leaves(packaged);
    expected.dependency_depth = count_max_depth(packaged);
    expected.max_fanin = count_max_fanin(packaged);
    expected.max_fanout = count_max_fanout(packaged);
    expected.graph_package_bytes = graph_package_bytes;
    expected.runtime_shape_table_offset =
        packaged.shape_mode == AiShapeMode::DynamicBounded ? align_up_u32(graph_package_bytes, 64) : 0;
    expected.runtime_shape_table_addr =
        expected.runtime_shape_table_offset == 0
            ? 0
            : expected.graph_package_addr + expected.runtime_shape_table_offset;

    const std::string manifest_text = read_text_file(manifest);
    const std::string source_tag_key = "source_tag=";
    const size_t source_tag_pos = manifest_text.find(source_tag_key);
    if (source_tag_pos != std::string::npos) {
        const size_t value_begin = source_tag_pos + source_tag_key.size();
        const size_t value_end = manifest_text.find_first_of("\r\n", value_begin);
        const std::string value = manifest_text.substr(value_begin, value_end - value_begin);
        expected.source_tag = static_cast<uint32_t>(std::stoul(value, nullptr, 0));
    }

    if (packaged.shape_mode == AiShapeMode::DynamicBounded) {
        const std::filesystem::path runtime_shape_path = manifest_dir / (graph_name + ".runtime_shape.bin");
        std::vector<AiRuntimeShapeEntry> runtime_shapes{};
        if (!parse_ai_runtime_shape_table(read_binary_file(runtime_shape_path),
                                          packaged.dynamic_tensors.size(),
                                          runtime_shapes,
                                          error)) {
            throw std::runtime_error("failed to parse manifest runtime shape table: " + error);
        }
        expected.runtime_shape_count = static_cast<uint32_t>(runtime_shapes.size());
    }

    count_transfers(packaged,
                    expected.load_entry_count,
                    expected.store_entry_count,
                    expected.load_plan_bytes,
                    expected.store_plan_bytes);
    return expected;
}

bool expect_submission_compile_contract(const AiAcceleratorProfileSummary& summary,
                                        AiShapeMode expected_shape_mode,
                                        uint32_t expected_runtime_shape_count,
                                        uint32_t expected_tensor_count,
                                        uint32_t expected_memory_plan_entries,
                                        uint32_t expected_dynamic_tensor_count,
                                        uint32_t expected_input_tensor_count,
                                        uint32_t expected_output_tensor_count,
                                        uint32_t expected_weight_tensor_count,
                                        uint32_t expected_constant_tensor_count,
                                        uint32_t expected_intermediate_tensor_count,
                                        uint32_t expected_scratchpad_budget_bytes,
                                        uint32_t expected_op_count,
                                        uint32_t expected_dependency_count,
                                        uint32_t expected_root_op_count,
                                        uint32_t expected_leaf_op_count,
                                        uint32_t expected_dependency_depth,
                                        uint32_t expected_max_fanin,
                                        uint32_t expected_max_fanout,
                                        uint32_t expected_load_entry_count,
                                        uint32_t expected_store_entry_count,
                                        uint32_t expected_load_plan_bytes,
                                        uint32_t expected_store_plan_bytes,
                                        uint64_t expected_token,
                                        uint32_t expected_flags,
                                        uint64_t expected_graph_package_addr,
                                        uint64_t expected_input_table_addr,
                                        uint64_t expected_output_table_addr,
                                        uint64_t expected_submission_base_snapshot,
                                        uint64_t expected_completion_base_snapshot,
                                        uint32_t expected_graph_package_bytes,
                                        uint32_t expected_runtime_shape_table_offset,
                                        uint64_t expected_runtime_shape_table_addr,
                                        uint32_t expected_source_tag,
                                        uint32_t expected_queue_depth_snapshot,
                                        uint32_t expected_submission_queue_size_snapshot,
                                        uint32_t expected_completion_queue_size_snapshot,
                                        uint32_t expected_submission_head_snapshot,
                                        uint32_t expected_submission_tail_snapshot,
                                        uint32_t expected_completion_head_snapshot,
                                        uint32_t expected_completion_tail_snapshot,
                                        bool expected_queue_configured_snapshot,
                                        const char* context) {
    return expect(summary.last_submission_shape_mode == expected_shape_mode, context) &&
           expect(summary.last_submission_runtime_shape_count == expected_runtime_shape_count, context) &&
           expect(summary.last_submission_tensor_count == expected_tensor_count, context) &&
           expect(summary.last_submission_memory_plan_entries == expected_memory_plan_entries, context) &&
           expect(summary.last_submission_dynamic_tensor_count == expected_dynamic_tensor_count, context) &&
           expect(summary.last_submission_input_tensor_count == expected_input_tensor_count, context) &&
           expect(summary.last_submission_output_tensor_count == expected_output_tensor_count, context) &&
           expect(summary.last_submission_weight_tensor_count == expected_weight_tensor_count, context) &&
           expect(summary.last_submission_constant_tensor_count == expected_constant_tensor_count, context) &&
           expect(summary.last_submission_intermediate_tensor_count ==
                      expected_intermediate_tensor_count,
                  context) &&
           expect(summary.last_submission_scratchpad_budget_bytes ==
                      expected_scratchpad_budget_bytes,
                  context) &&
           expect(summary.last_submission_op_count == expected_op_count, context) &&
           expect(summary.last_submission_dependency_count == expected_dependency_count, context) &&
           expect(summary.last_submission_root_op_count == expected_root_op_count, context) &&
           expect(summary.last_submission_leaf_op_count == expected_leaf_op_count, context) &&
           expect(summary.last_submission_dependency_depth == expected_dependency_depth, context) &&
           expect(summary.last_submission_max_fanin == expected_max_fanin, context) &&
           expect(summary.last_submission_max_fanout == expected_max_fanout, context) &&
           expect(summary.last_submission_load_entry_count == expected_load_entry_count, context) &&
           expect(summary.last_submission_store_entry_count == expected_store_entry_count, context) &&
           expect(summary.last_submission_load_plan_bytes == expected_load_plan_bytes, context) &&
           expect(summary.last_submission_store_plan_bytes == expected_store_plan_bytes, context) &&
           expect(summary.last_submission_token == expected_token, context) &&
           expect(summary.last_submission_flags == expected_flags, context) &&
           expect(summary.last_submission_graph_package_addr == expected_graph_package_addr, context) &&
           expect(summary.last_submission_input_table_addr == expected_input_table_addr, context) &&
           expect(summary.last_submission_output_table_addr == expected_output_table_addr, context) &&
           expect(summary.submission_base_snapshot == expected_submission_base_snapshot, context) &&
           expect(summary.completion_base_snapshot == expected_completion_base_snapshot, context) &&
           expect(summary.last_submission_graph_package_bytes == expected_graph_package_bytes, context) &&
           expect(summary.last_submission_runtime_shape_table_offset ==
                      expected_runtime_shape_table_offset,
                  context) &&
           expect(summary.last_submission_runtime_shape_table_addr ==
                      expected_runtime_shape_table_addr,
                  context) &&
           expect(summary.last_submission_source_tag == expected_source_tag, context) &&
           expect(summary.queue_depth_snapshot == expected_queue_depth_snapshot, context) &&
           expect(summary.submission_queue_size_snapshot ==
                      expected_submission_queue_size_snapshot,
                  context) &&
           expect(summary.completion_queue_size_snapshot ==
                      expected_completion_queue_size_snapshot,
                  context) &&
           expect(summary.submission_head_snapshot == expected_submission_head_snapshot, context) &&
           expect(summary.submission_tail_snapshot == expected_submission_tail_snapshot, context) &&
           expect(summary.completion_head_snapshot == expected_completion_head_snapshot, context) &&
           expect(summary.completion_tail_snapshot == expected_completion_tail_snapshot, context) &&
           expect(summary.queue_configured_snapshot == expected_queue_configured_snapshot, context);
}

bool expect_matching_op_summaries(const std::vector<AiOpProfileSummary>& actual,
                                  const std::vector<AiAcceleratorOpProfileSummary>& expected,
                                  const char* context) {
    if (!expect(actual.size() == expected.size(), context)) {
        return false;
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (!expect(actual[i].op_index == expected[i].op_index, context) ||
            !expect(actual[i].opcode == expected[i].opcode, context) ||
            !expect(actual[i].retired_ops == expected[i].retired_ops, context) ||
            !expect(actual[i].compute_cycles == expected[i].compute_cycles, context) ||
            !expect(actual[i].stall_cycles == expected[i].stall_cycles, context) ||
            !expect(actual[i].tile_count == expected[i].tile_count, context)) {
            return false;
        }
    }
    return true;
}

bool expect_empty_submission_compile_contract(const AiAcceleratorProfileSummary& summary,
                                              const char* context) {
    return expect_submission_compile_contract(summary,
                                              AiShapeMode::Static,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              0,
                                              false,
                                              context);
}

bool expect_manifest_device_profile_summary(const std::filesystem::path& manifest,
                                            uint64_t expected_device_cycles,
                                            uint64_t expected_dma_cycles,
                                            uint64_t expected_compute_cycles,
                                            uint64_t expected_stall_cycles,
                                            uint64_t expected_busy_cycles,
                                            uint64_t expected_queue_cycles,
                                            uint64_t expected_completion_cycles,
                                            uint64_t expected_dma_load_cycles,
                                            uint64_t expected_dma_store_cycles,
                                            uint64_t expected_dma_load_bytes,
                                            uint64_t expected_dma_store_bytes,
                                            uint64_t expected_retired_ops,
                                            uint64_t expected_bytes_moved,
                                            uint64_t expected_tile_count,
                                            uint32_t expected_scratchpad_peak_bytes,
                                            size_t expected_op_count,
                                            const char* context) {
    Machine machine{};
    const Machine::AiProfileRunResult result = machine.run_ai_profile_manifest(manifest.string());
    const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
    const ExpectedManifestCompileContract expected =
        expected_manifest_compile_contract(manifest, result.graph_package_bytes);
    return expect(result.completed, context) &&
           expect(result.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, context) &&
           expect(result.fault_code == AI_ACCEL_FAULT_NONE, context) &&
           expect(result.device_cycles == expected_device_cycles, context) &&
           expect(result.dma_cycles == expected_dma_cycles, context) &&
           expect(result.compute_cycles == expected_compute_cycles, context) &&
           expect(result.stall_cycles == expected_stall_cycles, context) &&
           expect(result.busy_cycles == expected_busy_cycles, context) &&
           expect(result.queue_cycles == expected_queue_cycles, context) &&
           expect(result.completion_cycles == expected_completion_cycles, context) &&
           expect(result.retired_ops == expected_retired_ops, context) &&
           expect(result.bytes_moved == expected_bytes_moved, context) &&
           expect(result.tile_count == expected_tile_count, context) &&
           expect(result.scratchpad_peak_bytes == expected_scratchpad_peak_bytes, context) &&
           expect(result.op_summaries.size() == expected_op_count, context) &&
           expect(machine.ai_accelerator().completion_count() == 1, context) &&
           expect(machine.ai_accelerator().doorbell_count() == 1, context) &&
           expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, context) &&
           expect_default_timing_model(summary, context) &&
           expect_submission_timing(summary,
                                    expected_device_cycles,
                                    expected_dma_cycles,
                                    expected_compute_cycles,
                                    expected_stall_cycles,
                                    expected_queue_cycles,
                                    expected_completion_cycles,
                                    expected_busy_cycles,
                                    context) &&
           expect_submission_outcome(summary,
                                     AI_ACCEL_FAULT_NONE,
                                     expected_retired_ops,
                                     expected_bytes_moved,
                                     context) &&
           expect_submission_dma_breakdown(summary,
                                           expected_dma_load_cycles,
                                           expected_dma_store_cycles,
                                           expected_dma_load_bytes,
                                           expected_dma_store_bytes,
                                           context) &&
           expect_submission_compile_contract(summary,
                                              expected.shape_mode,
                                              expected.runtime_shape_count,
                                              expected.tensor_count,
                                              expected.memory_plan_entries,
                                              expected.dynamic_tensor_count,
                                              expected.input_tensor_count,
                                              expected.output_tensor_count,
                                              expected.weight_tensor_count,
                                              expected.constant_tensor_count,
                                              expected.intermediate_tensor_count,
                                              expected.scratchpad_budget_bytes,
                                              expected.op_count,
                                              expected.dependency_count,
                                              expected.root_op_count,
                                              expected.leaf_op_count,
                                              expected.dependency_depth,
                                              expected.max_fanin,
                                              expected.max_fanout,
                                              expected.load_entry_count,
                                              expected.store_entry_count,
                                              expected.load_plan_bytes,
                                              expected.store_plan_bytes,
                                              expected.token,
                                              expected.flags,
                                              expected.graph_package_addr,
                                              expected.input_table_addr,
                                              expected.output_table_addr,
                                              expected.submission_base_snapshot,
                                              expected.completion_base_snapshot,
                                              expected.graph_package_bytes,
                                              expected.runtime_shape_table_offset,
                                              expected.runtime_shape_table_addr,
                                              expected.source_tag,
                                              expected.queue_depth_snapshot,
                                              expected.submission_queue_size_snapshot,
                                              expected.completion_queue_size_snapshot,
                                              expected.submission_head_snapshot,
                                              expected.submission_tail_snapshot,
                                              expected.completion_head_snapshot,
                                              expected.completion_tail_snapshot,
                                              expected.queue_configured_snapshot,
                                              context) &&
           expect(summary.tile_count == expected_tile_count, context) &&
           expect(summary.scratchpad_peak_bytes == expected_scratchpad_peak_bytes, context) &&
           expect(summary.op_summaries.size() == expected_op_count, context) &&
           expect_matching_op_summaries(result.op_summaries, summary.op_summaries, context);
}

bool expect_manifest_rerun_refresh(const std::filesystem::path& first_manifest,
                                   const std::filesystem::path& second_manifest,
                                   uint64_t expected_second_device_cycles,
                                   uint64_t expected_second_dma_cycles,
                                   uint64_t expected_second_compute_cycles,
                                   uint64_t expected_second_stall_cycles,
                                   uint64_t expected_second_busy_cycles,
                                   uint64_t expected_second_queue_cycles,
                                   uint64_t expected_second_completion_cycles,
                                   uint64_t expected_second_dma_load_cycles,
                                   uint64_t expected_second_dma_store_cycles,
                                   uint64_t expected_second_dma_load_bytes,
                                   uint64_t expected_second_dma_store_bytes,
                                   uint64_t expected_second_retired_ops,
                                   uint64_t expected_second_bytes_moved,
                                   uint64_t expected_second_tile_count,
                                   uint32_t expected_second_scratchpad_peak_bytes,
                                   size_t expected_second_op_count,
                                   const char* context) {
    Machine machine{};
    const Machine::AiProfileRunResult first = machine.run_ai_profile_manifest(first_manifest.string());
    const Machine::AiProfileRunResult second = machine.run_ai_profile_manifest(second_manifest.string());
    const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
    const ExpectedManifestCompileContract expected =
        expected_manifest_compile_contract(second_manifest, second.graph_package_bytes);
    return expect(first.completed, context) &&
           expect(second.completed, context) &&
           expect(second.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, context) &&
           expect(second.fault_code == AI_ACCEL_FAULT_NONE, context) &&
           expect(second.device_cycles == expected_second_device_cycles, context) &&
           expect(second.dma_cycles == expected_second_dma_cycles, context) &&
           expect(second.compute_cycles == expected_second_compute_cycles, context) &&
           expect(second.stall_cycles == expected_second_stall_cycles, context) &&
           expect(second.busy_cycles == expected_second_busy_cycles, context) &&
           expect(second.queue_cycles == expected_second_queue_cycles, context) &&
           expect(second.completion_cycles == expected_second_completion_cycles, context) &&
           expect(second.retired_ops == expected_second_retired_ops, context) &&
           expect(second.bytes_moved == expected_second_bytes_moved, context) &&
           expect(second.tile_count == expected_second_tile_count, context) &&
           expect(second.scratchpad_peak_bytes == expected_second_scratchpad_peak_bytes, context) &&
           expect(second.op_summaries.size() == expected_second_op_count, context) &&
           expect(machine.ai_accelerator().completion_count() == 1, context) &&
           expect(machine.ai_accelerator().doorbell_count() == 1, context) &&
           expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, context) &&
           expect_default_timing_model(summary, context) &&
           expect_submission_timing(summary,
                                    expected_second_device_cycles,
                                    expected_second_dma_cycles,
                                    expected_second_compute_cycles,
                                    expected_second_stall_cycles,
                                    expected_second_queue_cycles,
                                    expected_second_completion_cycles,
                                    expected_second_busy_cycles,
                                    context) &&
           expect_submission_outcome(summary,
                                     AI_ACCEL_FAULT_NONE,
                                     expected_second_retired_ops,
                                     expected_second_bytes_moved,
                                     context) &&
           expect_submission_dma_breakdown(summary,
                                           expected_second_dma_load_cycles,
                                           expected_second_dma_store_cycles,
                                           expected_second_dma_load_bytes,
                                           expected_second_dma_store_bytes,
                                           context) &&
           expect_submission_compile_contract(summary,
                                              expected.shape_mode,
                                              expected.runtime_shape_count,
                                              expected.tensor_count,
                                              expected.memory_plan_entries,
                                              expected.dynamic_tensor_count,
                                              expected.input_tensor_count,
                                              expected.output_tensor_count,
                                              expected.weight_tensor_count,
                                              expected.constant_tensor_count,
                                              expected.intermediate_tensor_count,
                                              expected.scratchpad_budget_bytes,
                                              expected.op_count,
                                              expected.dependency_count,
                                              expected.root_op_count,
                                              expected.leaf_op_count,
                                              expected.dependency_depth,
                                              expected.max_fanin,
                                              expected.max_fanout,
                                              expected.load_entry_count,
                                              expected.store_entry_count,
                                              expected.load_plan_bytes,
                                              expected.store_plan_bytes,
                                              expected.token,
                                              expected.flags,
                                              expected.graph_package_addr,
                                              expected.input_table_addr,
                                              expected.output_table_addr,
                                              expected.submission_base_snapshot,
                                              expected.completion_base_snapshot,
                                              expected.graph_package_bytes,
                                              expected.runtime_shape_table_offset,
                                              expected.runtime_shape_table_addr,
                                              expected.source_tag,
                                              expected.queue_depth_snapshot,
                                              expected.submission_queue_size_snapshot,
                                              expected.completion_queue_size_snapshot,
                                              expected.submission_head_snapshot,
                                              expected.submission_tail_snapshot,
                                              expected.completion_head_snapshot,
                                              expected.completion_tail_snapshot,
                                              expected.queue_configured_snapshot,
                                              context) &&
           expect(summary.tile_count == expected_second_tile_count, context) &&
           expect(summary.scratchpad_peak_bytes == expected_second_scratchpad_peak_bytes, context) &&
           expect(summary.op_summaries.size() == expected_second_op_count, context) &&
           expect_matching_op_summaries(second.op_summaries, summary.op_summaries, context);
}

bool expect_manifest_failure_resets_device_state(const std::filesystem::path& success_manifest,
                                                 const std::filesystem::path& failing_manifest,
                                                 const char* expected_error,
                                                 const char* context) {
    Machine machine{};
    const Machine::AiProfileRunResult success = machine.run_ai_profile_manifest(success_manifest.string());
    if (!expect(success.completed, context) ||
        !expect(success.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, context)) {
        return false;
    }

    bool threw = false;
    try {
        static_cast<void>(machine.run_ai_profile_manifest(failing_manifest.string()));
    } catch (const std::runtime_error& ex) {
        threw = true;
        if (!expect(std::string(ex.what()).find(expected_error) != std::string::npos, context)) {
            return false;
        }
    }
    if (!expect(threw, context)) {
        return false;
    }

    const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
    return expect(machine.ai_accelerator().completion_count() == 0, context) &&
           expect(machine.ai_accelerator().doorbell_count() == 0, context) &&
           expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, context) &&
           expect_default_timing_model(summary, context) &&
           expect_submission_timing(summary, 0, 0, 0, 0, 0, 0, 0, context) &&
           expect_submission_outcome(summary, AI_ACCEL_FAULT_NONE, 0, 0, context) &&
           expect_submission_dma_breakdown(summary, 0, 0, 0, 0, context) &&
           expect_empty_submission_compile_contract(summary, context) &&
           expect(summary.tile_count == 0, context) &&
           expect(summary.scratchpad_peak_bytes == 0, context) &&
           expect(summary.op_summaries.empty(), context);
}

bool expect_manifest_timeout_state(const std::filesystem::path& manifest,
                                   uint64_t expected_ticks,
                                   uint64_t expected_device_cycles,
                                   uint64_t expected_dma_cycles,
                                   uint64_t expected_compute_cycles,
                                   uint64_t expected_stall_cycles,
                                   uint64_t expected_busy_cycles,
                                   uint64_t expected_queue_cycles,
                                   uint64_t expected_completion_cycles,
                                   const char* context) {
    Machine machine{};
    const Machine::AiProfileRunResult result = machine.run_ai_profile_manifest(manifest.string());
    const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
    return expect(!result.completed, context) &&
           expect(result.completion_status == AI_ACCEL_COMPLETION_STATUS_FAULT, context) &&
           expect(result.fault_code == AI_ACCEL_FAULT_TIMEOUT, context) &&
           expect(result.ticks == expected_ticks, context) &&
           expect(result.device_cycles == expected_device_cycles, context) &&
           expect(result.dma_cycles == expected_dma_cycles, context) &&
           expect(result.compute_cycles == expected_compute_cycles, context) &&
           expect(result.stall_cycles == expected_stall_cycles, context) &&
           expect(result.busy_cycles == expected_busy_cycles, context) &&
           expect(result.queue_cycles == expected_queue_cycles, context) &&
           expect(result.completion_cycles == expected_completion_cycles, context) &&
           expect(result.bytes_moved == 0, context) &&
           expect(result.retired_ops == 0, context) &&
           expect(result.tile_count == 0, context) &&
           expect(result.scratchpad_peak_bytes == 0, context) &&
           expect(result.op_summaries.empty(), context) &&
           expect(machine.ai_accelerator().completion_count() == 0, context) &&
           expect(machine.ai_accelerator().doorbell_count() == 1, context) &&
           expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, context) &&
           expect_default_timing_model(summary, context) &&
           expect_submission_timing(summary, 0, 0, 0, 0, 0, 0, 0, context) &&
           expect_submission_outcome(summary, AI_ACCEL_FAULT_NONE, 0, 0, context) &&
           expect_submission_dma_breakdown(summary, 0, 0, 0, 0, context) &&
           expect_empty_submission_compile_contract(summary, context) &&
           expect(summary.tile_count == 0, context) &&
           expect(summary.scratchpad_peak_bytes == 0, context) &&
           expect(summary.op_summaries.empty(), context);
}

bool expect_manifest_completion_fault_state(const std::filesystem::path& manifest,
                                            uint64_t expected_ticks,
                                            uint32_t expected_fault_code,
                                            uint64_t expected_device_cycles,
                                            uint64_t expected_dma_cycles,
                                            uint64_t expected_compute_cycles,
                                            uint64_t expected_stall_cycles,
                                            uint64_t expected_busy_cycles,
                                            uint64_t expected_queue_cycles,
                                            uint64_t expected_completion_cycles,
                                            uint64_t expected_bytes_moved,
                                            const char* context) {
    Machine machine{};
    const Machine::AiProfileRunResult result = machine.run_ai_profile_manifest(manifest.string());
    const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
    return expect(result.completed, context) &&
           expect(result.completion_status == AI_ACCEL_COMPLETION_STATUS_FAULT, context) &&
           expect(result.fault_code == expected_fault_code, context) &&
           expect(result.ticks == expected_ticks, context) &&
           expect(result.device_cycles == expected_device_cycles, context) &&
           expect(result.dma_cycles == expected_dma_cycles, context) &&
           expect(result.compute_cycles == expected_compute_cycles, context) &&
           expect(result.stall_cycles == expected_stall_cycles, context) &&
           expect(result.busy_cycles == expected_busy_cycles, context) &&
           expect(result.queue_cycles == expected_queue_cycles, context) &&
           expect(result.completion_cycles == expected_completion_cycles, context) &&
           expect(result.bytes_moved == expected_bytes_moved, context) &&
           expect(result.retired_ops == 0, context) &&
           expect(result.tile_count == 0, context) &&
           expect(result.scratchpad_peak_bytes == 0, context) &&
           expect(result.op_summaries.empty(), context) &&
           expect(machine.ai_accelerator().completion_count() == 1, context) &&
           expect(machine.ai_accelerator().doorbell_count() == 1, context) &&
           expect(machine.ai_accelerator().last_fault() == expected_fault_code, context) &&
           expect_default_timing_model(summary, context) &&
           expect_submission_timing(summary,
                                    expected_device_cycles,
                                    expected_dma_cycles,
                                    expected_compute_cycles,
                                    expected_stall_cycles,
                                    expected_queue_cycles,
                                    expected_completion_cycles,
                                    expected_busy_cycles,
                                    context) &&
           expect_submission_outcome(summary, expected_fault_code, 0, expected_bytes_moved, context) &&
           expect_submission_dma_breakdown(summary, 6, 0, expected_bytes_moved, 0, context) &&
           expect_empty_submission_compile_contract(summary, context) &&
           expect(summary.tile_count == 0, context) &&
           expect(summary.scratchpad_peak_bytes == 0, context) &&
           expect(summary.op_summaries.empty(), context);
}

bool expect_pack_and_profile(const std::filesystem::path& temp_dir,
                             const char* workload,
                             const char* expected_name,
                             uint64_t expected_device_cycles,
                             uint64_t expected_dma_cycles,
                             uint64_t expected_compute_cycles,
                             uint64_t expected_stall_cycles,
                             uint64_t expected_busy_cycles,
                             uint64_t expected_queue_cycles,
                             uint64_t expected_completion_cycles,
                             uint64_t expected_effective_ops_per_cycle,
                             uint64_t expected_utilization,
                             uint64_t expected_bytes_moved,
                             uint64_t expected_retired_ops,
                             const std::vector<std::string>& extra_needles = {}) {
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
    const std::filesystem::path memory_plan_summary =
        temp_dir / (std::string(workload) + ".memory_plan.txt");
    const std::filesystem::path actual = temp_dir / (std::string(workload) + ".output0.actual.bin");
    const std::filesystem::path expected = temp_dir / (std::string(workload) + ".output0.expected.bin");
    if (!expect_file_exists(manifest, "expected ai_proto manifest") ||
        !expect_file_exists(graph, "expected ai_proto graph package") ||
        !expect_file_exists(memory_plan_summary, "expected ai_proto memory plan summary") ||
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
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            package.shape_mode == AiShapeMode::DynamicBounded
                                                ? "dynamic_bounded"
                                                : "static",
                                            "expected ai_proto memory plan summary to match graph package")) {
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
                         ("stall_cycles=" + std::to_string(expected_stall_cycles)).c_str(),
                         "expected stall cycle summary") ||
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

    for (const std::string& needle : extra_needles) {
        if (!expect_contains(profile.output, needle.c_str(), "expected itemized ai profile output")) {
            return false;
        }
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
    const std::filesystem::path memory_plan_summary = temp_dir / "dynamic_gemm.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "dynamic_gemm.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "dynamic_gemm.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "dynamic_gemm.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "dynamic_gemm.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected dynamic_gemm manifest") ||
        !expect_file_exists(graph, "expected dynamic_gemm graph package") ||
        !expect_file_exists(memory_plan_summary, "expected dynamic_gemm memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected dynamic_gemm resolved memory plan summary") ||
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
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            "dynamic_bounded",
                                            "expected dynamic_gemm memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected dynamic_gemm resolved memory plan summary to match runtime-resolved graph package")) {
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
        !expect_contains(profile.output, "compute_cycles=2", "expected dynamic_gemm compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=2", "expected dynamic_gemm stall cycle summary") ||
        !expect_contains(profile.output, "busy_cycles=17", "expected dynamic_gemm busy cycle summary") ||
        !expect_contains(profile.output, "queue_cycles=1", "expected dynamic_gemm queue cycle summary") ||
        !expect_contains(profile.output, "completion_cycles=1", "expected dynamic_gemm completion cycle summary") ||
        !expect_contains(profile.output, "effective_ops_per_cycle=32", "expected dynamic_gemm op/cycle summary") ||
        !expect_contains(profile.output, "utilization=11", "expected dynamic_gemm utilization summary") ||
        !expect_contains(profile.output, "bytes_moved=80", "expected dynamic_gemm bytes moved summary") ||
        !expect_contains(profile.output, "retired_ops=64", "expected dynamic_gemm retired op summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=2 scratchpad_peak_bytes=80 op_count=1",
                         "expected dynamic_gemm aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=gemm retired_ops=64 compute_cycles=2 stall_cycles=2 tile_count=2",
                         "expected dynamic_gemm op itemized profile output") ||
        !expect_file_exists(actual, "expected dynamic_gemm actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected dynamic_gemm output to match packaged expectation");
}

bool expect_pack_and_profile_task_spec_dynamic_gemm(const std::filesystem::path& temp_dir) {
    const std::filesystem::path task_spec = temp_dir / "custom_dynamic_gemm.task_spec.json";
    write_text_file(task_spec,
                    "{\n"
                    "  \"format\": \"ai_task_spec_v1\",\n"
                    "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                    "  \"name\": \"custom_dynamic_gemm\",\n"
                    "  \"source_tag\": 73,\n"
                    "  \"max_ticks\": 128,\n"
                    "  \"input0\": [\n"
                    "    [2, 1, 0, -1, 3, 4, 5, 6],\n"
                    "    [6, 5, 4, 3, 2, 1, 0, -1]\n"
                    "  ],\n"
                    "  \"input1\": [\n"
                    "    [1, 0, 0, 1],\n"
                    "    [0, 1, 1, 0],\n"
                    "    [1, 1, 0, 0],\n"
                    "    [0, 0, 1, 1],\n"
                    "    [1, 0, 1, 0],\n"
                    "    [0, 1, 0, 1],\n"
                    "    [1, 0, 0, 0],\n"
                    "    [0, 0, 1, 0]\n"
                    "  ]\n"
                    "}\n");

    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --task-spec " + task_spec.string() +
        " --out-dir " + temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected task-spec pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "custom_dynamic_gemm.manifest";
    const std::filesystem::path graph = temp_dir / "custom_dynamic_gemm.graph.bin";
    const std::filesystem::path memory_plan_summary = temp_dir / "custom_dynamic_gemm.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "custom_dynamic_gemm.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "custom_dynamic_gemm.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "custom_dynamic_gemm.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "custom_dynamic_gemm.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected task-spec manifest") ||
        !expect_file_exists(graph, "expected task-spec graph package") ||
        !expect_file_exists(memory_plan_summary, "expected task-spec memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected task-spec resolved memory plan summary") ||
        !expect_file_exists(runtime_shape, "expected task-spec runtime shape table") ||
        !expect_file_exists(expected, "expected task-spec expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected task-spec packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 2,
                "expected task-spec packaged graph dynamic tensors") ||
        !expect(package.memory_plan.size() == 3,
                "expected task-spec packaged graph memory plan size") ||
        !expect(package.memory_plan[0].scratchpad_offset == 0 &&
                    package.memory_plan[0].byte_size == 16,
                "expected task-spec input tensor memory plan") ||
        !expect(package.memory_plan[1].scratchpad_offset == 16 &&
                    package.memory_plan[1].byte_size == 32,
                "expected task-spec weight tensor memory plan") ||
        !expect(package.memory_plan[2].scratchpad_offset == 48 &&
                    package.memory_plan[2].byte_size == 32,
                "expected task-spec output tensor memory plan") ||
        !expect(package.scratchpad_budget_bytes == 80,
                "expected task-spec scratchpad budget from automatic memory plan")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            "dynamic_bounded",
                                            "expected task-spec GEMM memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected task-spec GEMM resolved memory plan summary to match runtime-resolved graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected task-spec ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, "name=custom_dynamic_gemm", "expected task-spec workload name") ||
        !expect_contains(profile.output, "shape_mode=dynamic_bounded", "expected task-spec shape mode summary") ||
        !expect_contains(profile.output, "runtime_shapes=t0:2x8,t2:2x4", "expected task-spec runtime shape summary") ||
        !expect_contains(profile.output, "device_cycles=15", "expected task-spec device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=11", "expected task-spec DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=2", "expected task-spec compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=2", "expected task-spec stall cycle summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=2 scratchpad_peak_bytes=80 op_count=1",
                         "expected task-spec aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=gemm retired_ops=64 compute_cycles=2 stall_cycles=2 tile_count=2",
                         "expected task-spec op itemized profile output") ||
        !expect_file_exists(actual, "expected task-spec actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected task-spec output to match packaged expectation");
}

bool expect_pack_and_profile_task_spec_dynamic_cnn(const std::filesystem::path& temp_dir) {
    const std::filesystem::path task_spec = temp_dir / "custom_dynamic_cnn.task_spec.json";
    write_text_file(task_spec,
                    "{\n"
                    "  \"format\": \"ai_task_spec_v1\",\n"
                    "  \"task_kind\": \"bounded_dynamic_cnn_v1\",\n"
                    "  \"name\": \"custom_dynamic_cnn\",\n"
                    "  \"source_tag\": 79,\n"
                    "  \"max_ticks\": 128,\n"
                    "  \"input0\": [\n"
                    "    [1, -2, 3],\n"
                    "    [-4, 5, -6],\n"
                    "    [7, -8, 9]\n"
                    "  ],\n"
                    "  \"input1\": [\n"
                    "    [1, 0],\n"
                    "    [-1, 2]\n"
                    "  ]\n"
                    "}\n");

    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --task-spec " + task_spec.string() +
        " --out-dir " + temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected CNN task-spec pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "custom_dynamic_cnn.manifest";
    const std::filesystem::path graph = temp_dir / "custom_dynamic_cnn.graph.bin";
    const std::filesystem::path memory_plan_summary = temp_dir / "custom_dynamic_cnn.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "custom_dynamic_cnn.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "custom_dynamic_cnn.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "custom_dynamic_cnn.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "custom_dynamic_cnn.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected CNN task-spec manifest") ||
        !expect_file_exists(graph, "expected CNN task-spec graph package") ||
        !expect_file_exists(memory_plan_summary, "expected CNN task-spec memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected CNN task-spec resolved memory plan summary") ||
        !expect_file_exists(runtime_shape, "expected CNN task-spec runtime shape table") ||
        !expect_file_exists(expected, "expected CNN task-spec expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected CNN task-spec packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 5,
                "expected CNN task-spec packaged graph dynamic tensors") ||
        !expect(package.memory_plan.size() == 6,
                "expected CNN task-spec packaged graph memory plan size") ||
        !expect(package.memory_plan[0].scratchpad_offset == 0 &&
                    package.memory_plan[0].byte_size == 16,
                "expected CNN task-spec input tensor memory plan") ||
        !expect(package.memory_plan[1].scratchpad_offset == 16 &&
                    package.memory_plan[1].byte_size == 4,
                "expected CNN task-spec weight tensor memory plan") ||
        !expect(package.memory_plan[2].scratchpad_offset == 32 &&
                    package.memory_plan[2].byte_size == 36,
                "expected CNN task-spec conv output tensor memory plan") ||
        !expect(package.memory_plan[3].scratchpad_offset == 80 &&
                    package.memory_plan[3].byte_size == 36,
                "expected CNN task-spec relu tensor memory plan") ||
        !expect(package.memory_plan[4].scratchpad_offset == 128 &&
                    package.memory_plan[4].byte_size == 36,
                "expected CNN task-spec transpose tensor memory plan") ||
        !expect(package.memory_plan[5].scratchpad_offset == 176 &&
                    package.memory_plan[5].byte_size == 12,
                "expected CNN task-spec output tensor memory plan") ||
        !expect(package.scratchpad_budget_bytes == 192,
                "expected CNN task-spec scratchpad budget from automatic memory plan")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            "dynamic_bounded",
                                            "expected task-spec CNN memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected task-spec CNN resolved memory plan summary to match runtime-resolved graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected CNN task-spec ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, "name=custom_dynamic_cnn", "expected CNN task-spec workload name") ||
        !expect_contains(profile.output, "shape_mode=dynamic_bounded", "expected CNN task-spec shape mode summary") ||
        !expect_contains(profile.output,
                         "runtime_shapes=t0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2",
                         "expected CNN task-spec runtime shape summary") ||
        !expect_contains(profile.output, "device_cycles=17", "expected CNN task-spec device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=9", "expected CNN task-spec DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=4", "expected CNN task-spec compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=4", "expected CNN task-spec stall cycle summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=4 scratchpad_peak_bytes=184 op_count=4",
                         "expected CNN task-spec aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=conv2d retired_ops=16 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected CNN task-spec conv itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected CNN task-spec relu itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=2 opcode=layout_transpose retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected CNN task-spec transpose itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=3 opcode=reduce_sum retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected CNN task-spec reduce itemized profile output") ||
        !expect_file_exists(actual, "expected CNN task-spec actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected CNN task-spec output to match packaged expectation");
}

bool expect_pack_and_profile_task_spec_dynamic_tiny_model(const std::filesystem::path& temp_dir) {
    const std::filesystem::path task_spec = temp_dir / "custom_dynamic_tiny_model.task_spec.json";
    write_text_file(task_spec,
                    "{\n"
                    "  \"format\": \"ai_task_spec_v1\",\n"
                    "  \"task_kind\": \"bounded_dynamic_tiny_model_v1\",\n"
                    "  \"name\": \"custom_dynamic_tiny_model\",\n"
                    "  \"source_tag\": 83,\n"
                    "  \"max_ticks\": 128,\n"
                    "  \"input0\": [\n"
                    "    [0.5, 2.0, -1.0]\n"
                    "  ]\n"
                    "}\n");

    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --task-spec " + task_spec.string() +
        " --out-dir " + temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected dynamic tiny task-spec pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "custom_dynamic_tiny_model.manifest";
    const std::filesystem::path graph = temp_dir / "custom_dynamic_tiny_model.graph.bin";
    const std::filesystem::path memory_plan_summary =
        temp_dir / "custom_dynamic_tiny_model.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "custom_dynamic_tiny_model.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "custom_dynamic_tiny_model.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "custom_dynamic_tiny_model.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "custom_dynamic_tiny_model.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected dynamic tiny task-spec manifest") ||
        !expect_file_exists(graph, "expected dynamic tiny task-spec graph package") ||
        !expect_file_exists(memory_plan_summary, "expected dynamic tiny task-spec memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected dynamic tiny task-spec resolved memory plan summary") ||
        !expect_file_exists(runtime_shape, "expected dynamic tiny task-spec runtime shape table") ||
        !expect_file_exists(expected, "expected dynamic tiny task-spec expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected dynamic tiny task-spec packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 4,
                "expected dynamic tiny task-spec packaged graph dynamic tensors") ||
        !expect(package.memory_plan.size() == 5,
                "expected dynamic tiny task-spec packaged graph memory plan size") ||
        !expect(package.memory_plan[0].scratchpad_offset == 0 &&
                    package.memory_plan[0].byte_size == 12,
                "expected dynamic tiny task-spec input tensor memory plan") ||
        !expect(package.memory_plan[1].scratchpad_offset == 12 &&
                    package.memory_plan[1].byte_size == 12,
                "expected dynamic tiny task-spec weight tensor memory plan") ||
        !expect(package.memory_plan[2].scratchpad_offset == 24 &&
                    package.memory_plan[2].byte_size == 16,
                "expected dynamic tiny task-spec gemm tensor memory plan") ||
        !expect(package.memory_plan[3].scratchpad_offset == 40 &&
                    package.memory_plan[3].byte_size == 16,
                "expected dynamic tiny task-spec relu tensor memory plan") ||
        !expect(package.memory_plan[4].scratchpad_offset == 56 &&
                    package.memory_plan[4].byte_size == 8,
                "expected dynamic tiny task-spec output tensor memory plan") ||
        !expect(package.scratchpad_budget_bytes == 64,
                "expected dynamic tiny task-spec scratchpad budget from automatic memory plan")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(
            memory_plan_summary,
            package,
            "dynamic_bounded",
            "expected dynamic tiny task-spec memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected dynamic tiny task-spec resolved memory plan summary to match runtime-resolved graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0,
                "expected dynamic tiny task-spec ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output,
                         "name=custom_dynamic_tiny_model",
                         "expected dynamic tiny task-spec workload name") ||
        !expect_contains(profile.output,
                         "shape_mode=dynamic_bounded",
                         "expected dynamic tiny task-spec shape mode summary") ||
        !expect_contains(profile.output,
                         "runtime_shapes=t0:1x3,t2:1x2,t3:1x2,t4:1x1",
                         "expected dynamic tiny task-spec runtime shape summary") ||
        !expect_contains(profile.output,
                         "device_cycles=15",
                         "expected dynamic tiny task-spec device cycle summary") ||
        !expect_contains(profile.output,
                         "dma_cycles=9",
                         "expected dynamic tiny task-spec DMA cycle summary") ||
        !expect_contains(profile.output,
                         "compute_cycles=3",
                         "expected dynamic tiny task-spec compute cycle summary") ||
        !expect_contains(profile.output,
                         "stall_cycles=3",
                         "expected dynamic tiny task-spec stall cycle summary") ||
        !expect_contains(profile.output,
                         "busy_cycles=17",
                         "expected dynamic tiny task-spec busy cycle summary") ||
        !expect_contains(profile.output,
                         "queue_cycles=1",
                         "expected dynamic tiny task-spec queue cycle summary") ||
        !expect_contains(profile.output,
                         "completion_cycles=1",
                         "expected dynamic tiny task-spec completion cycle summary") ||
        !expect_contains(profile.output,
                         "effective_ops_per_cycle=3",
                         "expected dynamic tiny task-spec op/cycle summary") ||
        !expect_contains(profile.output,
                         "utilization=17",
                         "expected dynamic tiny task-spec utilization summary") ||
        !expect_contains(profile.output,
                         "bytes_moved=22",
                         "expected dynamic tiny task-spec bytes moved summary") ||
        !expect_contains(profile.output,
                         "retired_ops=10",
                         "expected dynamic tiny task-spec retired op summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=3 scratchpad_peak_bytes=60 op_count=3",
                         "expected dynamic tiny task-spec aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=gemm retired_ops=6 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny task-spec GEMM itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny task-spec ReLU itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=2 opcode=pool_max retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny task-spec pool itemized profile output") ||
        !expect_file_exists(actual, "expected dynamic tiny task-spec actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected dynamic tiny task-spec output to match packaged expectation");
}

bool expect_pack_and_profile_task_spec_static_tiny_attention(const std::filesystem::path& temp_dir) {
    const std::filesystem::path task_spec = temp_dir / "custom_tiny_attention_static.task_spec.json";
    write_text_file(task_spec,
                    "{\n"
                    "  \"format\": \"ai_task_spec_v1\",\n"
                    "  \"task_kind\": \"static_tiny_attention_v1\",\n"
                    "  \"name\": \"custom_tiny_attention_static\",\n"
                    "  \"source_tag\": 89,\n"
                    "  \"max_ticks\": 128,\n"
                    "  \"value_vector\": [2.0, 6.0]\n"
                    "}\n");

    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --task-spec " + task_spec.string() +
        " --out-dir " + temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected static tiny attention task-spec pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "custom_tiny_attention_static.manifest";
    const std::filesystem::path graph = temp_dir / "custom_tiny_attention_static.graph.bin";
    const std::filesystem::path memory_plan_summary =
        temp_dir / "custom_tiny_attention_static.memory_plan.txt";
    const std::filesystem::path actual = temp_dir / "custom_tiny_attention_static.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "custom_tiny_attention_static.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected static tiny attention task-spec manifest") ||
        !expect_file_exists(graph, "expected static tiny attention task-spec graph package") ||
        !expect_file_exists(memory_plan_summary,
                            "expected static tiny attention task-spec memory plan summary") ||
        !expect_file_exists(expected, "expected static tiny attention task-spec expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::Static,
                "expected static tiny attention task-spec packaged graph shape mode") ||
        !expect(package.dynamic_tensors.empty(),
                "expected static tiny attention task-spec packaged graph to stay static") ||
        !expect(package.memory_plan.size() == 6,
                "expected static tiny attention task-spec packaged graph memory plan size") ||
        !expect(package.memory_plan[0].scratchpad_offset == 0 &&
                    package.memory_plan[0].byte_size == 4,
                "expected static tiny attention task-spec input tensor memory plan") ||
        !expect(package.memory_plan[1].scratchpad_offset == 8 &&
                    package.memory_plan[1].byte_size == 8,
                "expected static tiny attention task-spec query weight tensor memory plan") ||
        !expect(package.memory_plan[2].scratchpad_offset == 16 &&
                    package.memory_plan[2].byte_size == 8,
                "expected static tiny attention task-spec first gemm tensor memory plan") ||
        !expect(package.memory_plan[3].scratchpad_offset == 24 &&
                    package.memory_plan[3].byte_size == 8,
                "expected static tiny attention task-spec softmax tensor memory plan") ||
        !expect(package.memory_plan[4].scratchpad_offset == 32 &&
                    package.memory_plan[4].byte_size == 8,
                "expected static tiny attention task-spec value tensor memory plan") ||
        !expect(package.memory_plan[5].scratchpad_offset == 48 &&
                    package.memory_plan[5].byte_size == 4,
                "expected static tiny attention task-spec output tensor memory plan") ||
        !expect(package.scratchpad_budget_bytes == 64,
                "expected static tiny attention task-spec scratchpad budget")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(
            memory_plan_summary,
            package,
            "static",
            "expected static tiny attention task-spec memory plan summary to match graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0,
                "expected static tiny attention task-spec ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output,
                         "name=custom_tiny_attention_static",
                         "expected static tiny attention task-spec workload name") ||
        !expect_contains(profile.output, "shape_mode=static", "expected static tiny attention shape mode summary") ||
        !expect_contains(profile.output, "device_cycles=18", "expected static tiny attention device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=12", "expected static tiny attention DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=3", "expected static tiny attention compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=3", "expected static tiny attention stall cycle summary") ||
        !expect_contains(profile.output, "busy_cycles=20", "expected static tiny attention busy cycle summary") ||
        !expect_contains(profile.output, "queue_cycles=1", "expected static tiny attention queue cycle summary") ||
        !expect_contains(profile.output, "completion_cycles=1", "expected static tiny attention completion cycle summary") ||
        !expect_contains(profile.output, "effective_ops_per_cycle=2", "expected static tiny attention op/cycle summary") ||
        !expect_contains(profile.output, "utilization=15", "expected static tiny attention utilization summary") ||
        !expect_contains(profile.output, "bytes_moved=24", "expected static tiny attention bytes moved summary") ||
        !expect_contains(profile.output, "retired_ops=8", "expected static tiny attention retired op summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=3 scratchpad_peak_bytes=52 op_count=3",
                         "expected static tiny attention aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=gemm retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected static tiny attention first GEMM itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=1 opcode=softmax retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected static tiny attention softmax itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=2 opcode=gemm retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected static tiny attention second GEMM itemized profile output") ||
        !expect_file_exists(actual, "expected static tiny attention task-spec actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected static tiny attention task-spec output to match packaged expectation");
}

bool expect_pack_and_profile_dynamic_tiny_model(const std::filesystem::path& temp_dir) {
    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --workload dynamic_tiny_model --out-dir " +
        temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected dynamic_tiny_model pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "dynamic_tiny_model.manifest";
    const std::filesystem::path graph = temp_dir / "dynamic_tiny_model.graph.bin";
    const std::filesystem::path memory_plan_summary = temp_dir / "dynamic_tiny_model.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "dynamic_tiny_model.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "dynamic_tiny_model.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "dynamic_tiny_model.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "dynamic_tiny_model.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected dynamic_tiny_model manifest") ||
        !expect_file_exists(graph, "expected dynamic_tiny_model graph package") ||
        !expect_file_exists(memory_plan_summary, "expected dynamic_tiny_model memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected dynamic_tiny_model resolved memory plan summary") ||
        !expect_file_exists(runtime_shape, "expected dynamic_tiny_model runtime shape table") ||
        !expect_file_exists(expected, "expected dynamic_tiny_model expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected dynamic_tiny_model packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 4,
                "expected dynamic_tiny_model packaged graph dynamic tensors")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            "dynamic_bounded",
                                            "expected dynamic_tiny_model memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected dynamic_tiny_model resolved memory plan summary to match runtime-resolved graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected dynamic_tiny_model ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, "name=dynamic_tiny_model", "expected dynamic tiny workload name") ||
        !expect_contains(profile.output, "shape_mode=dynamic_bounded", "expected dynamic tiny shape mode summary") ||
        !expect_contains(profile.output,
                         "runtime_shapes=t0:1x3,t2:1x2,t3:1x2,t4:1x1",
                         "expected dynamic tiny runtime shape summary") ||
        !expect_contains(profile.output, "device_cycles=15", "expected dynamic tiny device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=9", "expected dynamic tiny DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=3", "expected dynamic tiny compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=3", "expected dynamic tiny stall cycle summary") ||
        !expect_contains(profile.output, "busy_cycles=17", "expected dynamic tiny busy cycle summary") ||
        !expect_contains(profile.output, "queue_cycles=1", "expected dynamic tiny queue cycle summary") ||
        !expect_contains(profile.output, "completion_cycles=1", "expected dynamic tiny completion cycle summary") ||
        !expect_contains(profile.output,
                         "effective_ops_per_cycle=3",
                         "expected dynamic tiny op/cycle summary") ||
        !expect_contains(profile.output, "utilization=17", "expected dynamic tiny utilization summary") ||
        !expect_contains(profile.output, "bytes_moved=22", "expected dynamic tiny bytes moved summary") ||
        !expect_contains(profile.output, "retired_ops=10", "expected dynamic tiny retired op summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=3 scratchpad_peak_bytes=60 op_count=3",
                         "expected dynamic tiny aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=gemm retired_ops=6 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny GEMM itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny ReLU itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=2 opcode=pool_max retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic tiny pool itemized profile output") ||
        !expect_file_exists(actual, "expected dynamic_tiny_model actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected dynamic_tiny_model output to match packaged expectation");
}

bool expect_pack_and_profile_dynamic_cnn(const std::filesystem::path& temp_dir) {
    const std::string pack_command =
        "python3 workloads/ai_proto/pack_graph.py --workload dynamic_cnn --out-dir " +
        temp_dir.string();
    const CommandResult pack = run_command(pack_command);
    if (!expect(pack.exit_code == 0, "expected dynamic_cnn pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }

    const std::filesystem::path manifest = temp_dir / "dynamic_cnn.manifest";
    const std::filesystem::path graph = temp_dir / "dynamic_cnn.graph.bin";
    const std::filesystem::path memory_plan_summary = temp_dir / "dynamic_cnn.memory_plan.txt";
    const std::filesystem::path resolved_memory_plan_summary =
        temp_dir / "dynamic_cnn.resolved_memory_plan.txt";
    const std::filesystem::path runtime_shape = temp_dir / "dynamic_cnn.runtime_shape.bin";
    const std::filesystem::path actual = temp_dir / "dynamic_cnn.output0.actual.bin";
    const std::filesystem::path expected = temp_dir / "dynamic_cnn.output0.expected.bin";
    if (!expect_file_exists(manifest, "expected dynamic_cnn manifest") ||
        !expect_file_exists(graph, "expected dynamic_cnn graph package") ||
        !expect_file_exists(memory_plan_summary, "expected dynamic_cnn memory plan summary") ||
        !expect_file_exists(resolved_memory_plan_summary,
                            "expected dynamic_cnn resolved memory plan summary") ||
        !expect_file_exists(runtime_shape, "expected dynamic_cnn runtime shape table") ||
        !expect_file_exists(expected, "expected dynamic_cnn expected output")) {
        return false;
    }

    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(read_binary_file(graph), package, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    if (!expect(package.shape_mode == AiShapeMode::DynamicBounded,
                "expected dynamic_cnn packaged graph shape mode") ||
        !expect(package.dynamic_tensors.size() == 5,
                "expected dynamic_cnn packaged graph dynamic tensors")) {
        return false;
    }
    if (!expect_memory_plan_summary_matches(memory_plan_summary,
                                            package,
                                            "dynamic_bounded",
                                            "expected dynamic_cnn memory plan summary to match graph package")) {
        return false;
    }
    if (!expect_resolved_memory_plan_summary_matches(
            resolved_memory_plan_summary,
            package,
            runtime_shape,
            "expected dynamic_cnn resolved memory plan summary to match runtime-resolved graph package")) {
        return false;
    }

    const CommandResult profile =
        run_command("./mycpu --ai-profile-manifest " + manifest.string());
    if (!expect(profile.exit_code == 0, "expected dynamic_cnn ai profile command to succeed")) {
        std::fprintf(stderr, "%s\n", profile.output.c_str());
        return false;
    }

    if (!expect_contains(profile.output, "name=dynamic_cnn", "expected dynamic_cnn workload name") ||
        !expect_contains(profile.output, "shape_mode=dynamic_bounded", "expected dynamic_cnn shape mode summary") ||
        !expect_contains(profile.output,
                         "runtime_shapes=t0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2",
                         "expected dynamic_cnn runtime shape summary") ||
        !expect_contains(profile.output, "device_cycles=17", "expected dynamic_cnn device cycle summary") ||
        !expect_contains(profile.output, "dma_cycles=9", "expected dynamic_cnn DMA cycle summary") ||
        !expect_contains(profile.output, "compute_cycles=4", "expected dynamic_cnn compute cycle summary") ||
        !expect_contains(profile.output, "stall_cycles=4", "expected dynamic_cnn stall cycle summary") ||
        !expect_contains(profile.output, "busy_cycles=19", "expected dynamic_cnn busy cycle summary") ||
        !expect_contains(profile.output, "queue_cycles=1", "expected dynamic_cnn queue cycle summary") ||
        !expect_contains(profile.output, "completion_cycles=1", "expected dynamic_cnn completion cycle summary") ||
        !expect_contains(profile.output,
                         "effective_ops_per_cycle=7",
                         "expected dynamic_cnn op/cycle summary") ||
        !expect_contains(profile.output, "utilization=21", "expected dynamic_cnn utilization summary") ||
        !expect_contains(profile.output, "bytes_moved=21", "expected dynamic_cnn bytes moved summary") ||
        !expect_contains(profile.output, "retired_ops=28", "expected dynamic_cnn retired op summary") ||
        !expect_contains(profile.output,
                         "ai_profile_aggregate tile_count=4 scratchpad_peak_bytes=184 op_count=4",
                         "expected dynamic_cnn aggregate itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=0 opcode=conv2d retired_ops=16 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic_cnn conv itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic_cnn relu itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=2 opcode=layout_transpose retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic_cnn transpose itemized profile output") ||
        !expect_contains(profile.output,
                         "ai_profile_op op_index=3 opcode=reduce_sum retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                         "expected dynamic_cnn reduce itemized profile output") ||
        !expect_file_exists(actual, "expected dynamic_cnn actual output")) {
        return false;
    }

    return expect(read_binary_file(actual) == read_binary_file(expected),
                  "expected dynamic_cnn output to match packaged expectation");
}

bool expect_demo_v1_pack(const std::filesystem::path& temp_dir) {
    const CommandResult pack =
        run_command("python3 workloads/ai_proto/pack_graph.py --demo-v1 --out-dir " + temp_dir.string());
    if (!expect(pack.exit_code == 0, "expected demo_v1 pack command to succeed")) {
        std::fprintf(stderr, "%s\n", pack.output.c_str());
        return false;
    }
    if (!expect_contains(pack.output, "packed demo_v1 out_dir=", "expected demo_v1 pack summary")) {
        return false;
    }

    const std::filesystem::path guest_manifest = temp_dir / "guest_ai_accel_demo.manifest";
    const std::filesystem::path gemm_spec = temp_dir / "custom_dynamic_gemm.task_spec.json";
    const std::filesystem::path cnn_spec = temp_dir / "custom_dynamic_cnn.task_spec.json";
    const std::filesystem::path tiny_spec = temp_dir / "custom_dynamic_tiny_model.task_spec.json";
    const std::filesystem::path attention_spec = temp_dir / "custom_tiny_attention_static.task_spec.json";
    const std::filesystem::path fail_closed_spec =
        temp_dir / "custom_dynamic_gemm_fail_closed.task_spec.json";
    const std::filesystem::path gemm_manifest = temp_dir / "custom_dynamic_gemm.manifest";
    const std::filesystem::path cnn_manifest = temp_dir / "custom_dynamic_cnn.manifest";
    const std::filesystem::path tiny_manifest = temp_dir / "custom_dynamic_tiny_model.manifest";
    const std::filesystem::path attention_manifest = temp_dir / "custom_tiny_attention_static.manifest";

    return expect_file_exists(guest_manifest, "expected demo_v1 guest bridge manifest") &&
           expect_file_exists(gemm_spec, "expected demo_v1 GEMM task spec") &&
           expect_file_exists(cnn_spec, "expected demo_v1 CNN task spec") &&
           expect_file_exists(tiny_spec, "expected demo_v1 tiny-model task spec") &&
           expect_file_exists(attention_spec, "expected demo_v1 attention task spec") &&
           expect_file_exists(fail_closed_spec, "expected demo_v1 fail-closed task spec") &&
           expect_file_exists(gemm_manifest, "expected demo_v1 GEMM manifest") &&
           expect_file_exists(cnn_manifest, "expected demo_v1 CNN manifest") &&
           expect_file_exists(tiny_manifest, "expected demo_v1 tiny-model manifest") &&
           expect_file_exists(attention_manifest, "expected demo_v1 attention manifest");
}

bool expect_demo_v1_run_script(const std::filesystem::path& temp_dir) {
    const CommandResult run = run_command(
        "python3 workloads/ai_proto/run_demo_v1.py --out-dir " + temp_dir.string());
    if (!expect(run.exit_code == 0, "expected demo_v1 run script to succeed")) {
        std::fprintf(stderr, "%s\n", run.output.c_str());
        return false;
    }
    return expect_contains(run.output, "== demo_v1 pack ==", "expected demo_v1 run script pack section") &&
           expect_contains(run.output,
                           "== guest_ai_accel_demo summary ==",
                           "expected demo_v1 run script guest bridge section") &&
           expect_contains(run.output,
                           "== custom_dynamic_gemm summary ==",
                           "expected demo_v1 run script GEMM section") &&
           expect_contains(run.output,
                           "== custom_dynamic_cnn summary ==",
                           "expected demo_v1 run script CNN section") &&
           expect_contains(run.output,
                           "== custom_dynamic_tiny_model summary ==",
                           "expected demo_v1 run script tiny-model section") &&
           expect_contains(run.output,
                           "== demo_v1 fail-closed ==",
                           "expected demo_v1 run script fail-closed section") &&
           expect_contains(run.output,
                           "demo_v1 summary: packed fixed assets, ran 4 positive samples, and verified 1 fail-closed sample",
                           "expected demo_v1 run script final summary") &&
           expect_contains(run.output,
                           "demo_v1 note: custom_tiny_attention_static.manifest was packed but not run",
                           "expected demo_v1 run script optional attention note");
}

}  // namespace

int main() {
    try {
        const std::filesystem::path profile_mk = "workloads/ai_proto/profile.mk";
        const std::filesystem::path pack_graph = "workloads/ai_proto/pack_graph.py";
        const std::filesystem::path readme = "workloads/ai_proto/README.md";
        const std::filesystem::path demo_v1_runner = "workloads/ai_proto/run_demo_v1.py";

        if (!expect_file_exists(profile_mk, "ai profile smoke expects workload profile") ||
            !expect_file_exists(pack_graph, "ai profile smoke expects packer script") ||
            !expect_file_exists(demo_v1_runner, "ai profile smoke expects demo_v1 runner") ||
            !expect_file_exists(readme, "ai profile smoke expects ai workload readme")) {
            return 1;
        }

        const std::string profile_text = read_text_file(profile_mk);
        const std::string readme_text = read_text_file(readme);
        const CommandResult make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=cnn");
        const CommandResult tiny_model_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model");
        const CommandResult guest_ai_accel_demo_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=guest_ai_accel_demo");
        const CommandResult dynamic_gemm_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_gemm");
        const CommandResult dynamic_tiny_model_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_tiny_model");
        const CommandResult dynamic_cnn_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_cnn");
        const CommandResult tiny_attention_make_run = run_command(
            "make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_attention_static");
        if (!expect(make_run.exit_code == 0, "expected ai workload make dry-run to succeed") ||
            !expect(tiny_model_make_run.exit_code == 0,
                    "expected tiny model ai workload make dry-run to succeed") ||
            !expect(guest_ai_accel_demo_make_run.exit_code == 0,
                    "expected guest_ai_accel_demo ai workload make dry-run to succeed") ||
            !expect(dynamic_gemm_make_run.exit_code == 0,
                    "expected dynamic_gemm ai workload make dry-run to succeed") ||
            !expect(dynamic_tiny_model_make_run.exit_code == 0,
                    "expected dynamic_tiny_model ai workload make dry-run to succeed") ||
            !expect(dynamic_cnn_make_run.exit_code == 0,
                    "expected dynamic_cnn ai workload make dry-run to succeed") ||
            !expect(tiny_attention_make_run.exit_code == 0,
                    "expected tiny_attention_static ai workload make dry-run to succeed") ||
            !expect_contains(profile_text,
                             "WORKLOAD_RUN_MODE := ai-profile",
                             "expected ai_proto workload run mode") ||
            !expect_contains(make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/cnn.manifest",
                             "expected ai workload dry-run manifest argument") ||
            !expect_contains(tiny_model_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/tiny_model.manifest",
                             "expected tiny model dry-run manifest argument") ||
            !expect_contains(
                guest_ai_accel_demo_make_run.output,
                "--ai-profile-manifest workloads/ai_proto/generated/guest_ai_accel_demo.manifest",
                "expected guest_ai_accel_demo dry-run manifest argument") ||
            !expect_contains(dynamic_gemm_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/dynamic_gemm.manifest",
                             "expected dynamic_gemm dry-run manifest argument") ||
            !expect_contains(dynamic_tiny_model_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/dynamic_tiny_model.manifest",
                             "expected dynamic_tiny_model dry-run manifest argument") ||
            !expect_contains(dynamic_cnn_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/dynamic_cnn.manifest",
                             "expected dynamic_cnn dry-run manifest argument") ||
            !expect_contains(tiny_attention_make_run.output,
                             "--ai-profile-manifest workloads/ai_proto/generated/tiny_attention_static.manifest",
                             "expected tiny_attention_static dry-run manifest argument") ||
            !expect_contains(readme_text,
                             "python3 workloads/ai_proto/pack_graph.py --demo-v1 --out-dir workloads/ai_proto/generated/demo_v1",
                             "expected ai workload readme to document demo_v1 pack command") ||
            !expect_contains(readme_text,
                             "python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1",
                             "expected ai workload readme to document demo_v1 run command") ||
            !expect_contains(readme_text,
                             "./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_dynamic_gemm.manifest",
                             "expected ai workload readme to document demo_v1 manifest run command") ||
            !expect_contains(readme_text,
                             "custom_dynamic_gemm_fail_closed.task_spec.json",
                             "expected ai workload readme to document demo_v1 fail-closed example") ||
            !expect_contains(readme_text,
                             "guest_ai_accel_demo.manifest",
                             "expected ai workload readme to document guest bridge demo asset")) {
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

        const std::filesystem::path dynamic_malformed_dir = temp_dir / "dynamic_malformed";
        const CommandResult dynamic_malformed_pack = run_command(
            "python3 workloads/ai_proto/pack_graph.py --workload dynamic_gemm --out-dir " +
            dynamic_malformed_dir.string());
        if (!expect(dynamic_malformed_pack.exit_code == 0,
                    "expected dynamic malformed-input pack command to succeed")) {
            std::fprintf(stderr, "%s\n", dynamic_malformed_pack.output.c_str());
            std::filesystem::remove_all(temp_dir);
            return 1;
        }

        const std::filesystem::path task_spec_malformed_dir = temp_dir / "task_spec_malformed";
        std::filesystem::create_directories(task_spec_malformed_dir);
        const std::filesystem::path oversized_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_oversized_rows.json";
        write_text_file(oversized_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"oversized_rows_gemm\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8],\n"
                        "    [8, 7, 6, 5, 4, 3, 2, 1],\n"
                        "    [0, 0, 0, 0, 0, 0, 0, 0]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path oversized_cnn_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_cnn_oversized_rows.json";
        write_text_file(oversized_cnn_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_cnn_v1\",\n"
                        "  \"name\": \"oversized_rows_cnn\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4],\n"
                        "    [5, 6, 7, 8],\n"
                        "    [9, 10, 11, 12],\n"
                        "    [13, 14, 15, 16],\n"
                        "    [17, 18, 19, 20]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0],\n"
                        "    [-1, 2]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path oversized_dynamic_tiny_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_tiny_model_oversized_rows.json";
        write_text_file(oversized_dynamic_tiny_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_tiny_model_v1\",\n"
                        "  \"name\": \"oversized_rows_dynamic_tiny_model\",\n"
                        "  \"input0\": [\n"
                        "    [0.5, 2.0, -1.0],\n"
                        "    [1.0, -2.0, 3.0],\n"
                        "    [-1.0, -1.0, -1.0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path bad_attention_task_spec =
            task_spec_malformed_dir / "static_tiny_attention_bad_value_count.json";
        write_text_file(bad_attention_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"static_tiny_attention_v1\",\n"
                        "  \"name\": \"bad_value_count_attention\",\n"
                        "  \"value_vector\": [1.0, 2.0, 3.0]\n"
                        "}\n");
        const std::filesystem::path non_object_task_spec =
            task_spec_malformed_dir / "task_spec_non_object.json";
        write_text_file(non_object_task_spec, "[1, 2, 3]\n");
        const std::filesystem::path wrong_format_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_wrong_format.json";
        write_text_file(wrong_format_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v0\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"wrong_format_gemm\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path unsupported_task_kind_task_spec =
            task_spec_malformed_dir / "unsupported_task_kind.json";
        write_text_file(unsupported_task_kind_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"future_dynamic_attention_v1\",\n"
                        "  \"name\": \"unsupported_task_kind\",\n"
                        "  \"input0\": [1]\n"
                        "}\n");
        const std::filesystem::path empty_name_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_empty_name.json";
        write_text_file(empty_name_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path negative_source_tag_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_negative_source_tag.json";
        write_text_file(negative_source_tag_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"negative_source_tag_gemm\",\n"
                        "  \"source_tag\": -1,\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path zero_max_ticks_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_cnn_zero_max_ticks.json";
        write_text_file(zero_max_ticks_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_cnn_v1\",\n"
                        "  \"name\": \"zero_max_ticks_cnn\",\n"
                        "  \"max_ticks\": 0,\n"
                        "  \"input0\": [\n"
                        "    [1, -2, 3],\n"
                        "    [-4, 5, -6],\n"
                        "    [7, -8, 9]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0],\n"
                        "    [-1, 2]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path oversized_source_tag_task_spec =
            task_spec_malformed_dir / "dynamic_tiny_model_oversized_source_tag.json";
        write_text_file(oversized_source_tag_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_tiny_model_v1\",\n"
                        "  \"name\": \"oversized_source_tag_dynamic_tiny_model\",\n"
                        "  \"source_tag\": 4294967296,\n"
                        "  \"input0\": [\n"
                        "    [0.5, 2.0, -1.0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path oversized_max_ticks_attention_task_spec =
            task_spec_malformed_dir / "static_tiny_attention_oversized_max_ticks.json";
        write_text_file(oversized_max_ticks_attention_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"static_tiny_attention_v1\",\n"
                        "  \"name\": \"oversized_max_ticks_attention\",\n"
                        "  \"max_ticks\": 4294967296,\n"
                        "  \"value_vector\": [1.0, 3.0]\n"
                        "}\n");
        const std::filesystem::path unknown_top_level_key_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_unknown_top_level_key.json";
        write_text_file(unknown_top_level_key_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"unknown_top_level_key_gemm\",\n"
                        "  \"unknown_extra\": 1,\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path duplicate_name_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_duplicate_name.json";
        write_text_file(duplicate_name_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"duplicate_name_first\",\n"
                        "  \"name\": \"duplicate_name_second\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path bool_source_tag_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_bool_source_tag.json";
        write_text_file(bool_source_tag_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"bool_source_tag_gemm\",\n"
                        "  \"source_tag\": true,\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path bool_max_ticks_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_cnn_bool_max_ticks.json";
        write_text_file(bool_max_ticks_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_cnn_v1\",\n"
                        "  \"name\": \"bool_max_ticks_cnn\",\n"
                        "  \"max_ticks\": true,\n"
                        "  \"input0\": [\n"
                        "    [1, -2, 3],\n"
                        "    [-4, 5, -6],\n"
                        "    [7, -8, 9]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0],\n"
                        "    [-1, 2]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path bool_input0_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_bool_input0.json";
        write_text_file(bool_input0_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"bool_input0_gemm\",\n"
                        "  \"input0\": [\n"
                        "    [true, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path bool_kernel_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_cnn_bool_kernel.json";
        write_text_file(bool_kernel_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_cnn_v1\",\n"
                        "  \"name\": \"bool_kernel_cnn\",\n"
                        "  \"input0\": [\n"
                        "    [1, -2, 3],\n"
                        "    [-4, 5, -6],\n"
                        "    [7, -8, 9]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [true, 0],\n"
                        "    [-1, 2]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path path_escape_name_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_path_escape_name.json";
        write_text_file(path_escape_name_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"../escaped_name\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path newline_name_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_gemm_newline_name.json";
        write_text_file(newline_name_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_gemm_v1\",\n"
                        "  \"name\": \"evil\\nmax_ticks=1\",\n"
                        "  \"input0\": [\n"
                        "    [1, 2, 3, 4, 5, 6, 7, 8]\n"
                        "  ],\n"
                        "  \"input1\": [\n"
                        "    [1, 0, 0, 0],\n"
                        "    [0, 1, 0, 0],\n"
                        "    [0, 0, 1, 0],\n"
                        "    [0, 0, 0, 1],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0],\n"
                        "    [0, 0, 0, 0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path overflow_fp16_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_tiny_model_overflow_fp16.json";
        write_text_file(overflow_fp16_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_tiny_model_v1\",\n"
                        "  \"name\": \"overflow_fp16_tiny_model\",\n"
                        "  \"input0\": [\n"
                        "    [1e100, 2.0, -1.0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path overflow_fp32_attention_task_spec =
            task_spec_malformed_dir / "static_tiny_attention_overflow_fp32.json";
        write_text_file(overflow_fp32_attention_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"static_tiny_attention_v1\",\n"
                        "  \"name\": \"overflow_fp32_attention\",\n"
                        "  \"value_vector\": [1e100, 6.0]\n"
                        "}\n");
        const std::filesystem::path non_finite_fp16_task_spec =
            task_spec_malformed_dir / "bounded_dynamic_tiny_model_non_finite_fp16.json";
        write_text_file(non_finite_fp16_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"bounded_dynamic_tiny_model_v1\",\n"
                        "  \"name\": \"non_finite_fp16_tiny_model\",\n"
                        "  \"input0\": [\n"
                        "    [1.0e309, 2.0, -1.0]\n"
                        "  ]\n"
                        "}\n");
        const std::filesystem::path non_finite_fp32_attention_task_spec =
            task_spec_malformed_dir / "static_tiny_attention_non_finite_fp32.json";
        write_text_file(non_finite_fp32_attention_task_spec,
                        "{\n"
                        "  \"format\": \"ai_task_spec_v1\",\n"
                        "  \"task_kind\": \"static_tiny_attention_v1\",\n"
                        "  \"name\": \"non_finite_fp32_attention\",\n"
                        "  \"value_vector\": [1.0, -1.0e309]\n"
                        "}\n");

        const std::filesystem::path manifest = malformed_dir / "cnn.manifest";
        const std::filesystem::path no_format_manifest =
            manifest_without_format(manifest, malformed_dir / "cnn.no_format.manifest");
        const std::filesystem::path duplicate_name_manifest =
            manifest_with_duplicate_scalar_key(manifest,
                                               malformed_dir / "cnn.dup_name.manifest",
                                               "name",
                                               "cnn-duplicate");
        const std::filesystem::path dynamic_manifest = dynamic_malformed_dir / "dynamic_gemm.manifest";
        const std::filesystem::path dynamic_runtime_shape =
            dynamic_malformed_dir / "dynamic_gemm.runtime_shape.bin";
        const std::filesystem::path missing_runtime_shape_manifest =
            manifest_without_scalar_key(dynamic_manifest,
                                        dynamic_malformed_dir / "dynamic_gemm.no_runtime_shape.manifest",
                                        "runtime_shape_table");
        const std::filesystem::path duplicate_runtime_shape_manifest =
            manifest_with_duplicate_scalar_key(dynamic_manifest,
                                               dynamic_malformed_dir / "dynamic_gemm.dup_runtime_shape.manifest",
                                               "runtime_shape_table",
                                               "dynamic_gemm.runtime_shape.bin");
        write_binary_file(malformed_dir / "cnn.runtime_shape.bin", read_binary_file(dynamic_runtime_shape));
        const std::filesystem::path static_with_runtime_shape_manifest =
            manifest_with_inserted_scalar_key(manifest,
                                              malformed_dir / "cnn.with_runtime_shape.manifest",
                                              "graph_package",
                                              "runtime_shape_table",
                                              "cnn.runtime_shape.bin");

        const std::vector<uint8_t> dynamic_runtime_shape_bytes = read_binary_file(dynamic_runtime_shape);
        std::vector<uint8_t> truncated_runtime_shape_bytes = dynamic_runtime_shape_bytes;
        truncated_runtime_shape_bytes.pop_back();
        write_binary_file(dynamic_malformed_dir / "dynamic_gemm.runtime_shape.truncated.bin",
                          truncated_runtime_shape_bytes);
        std::vector<uint8_t> reserved_runtime_shape_bytes = dynamic_runtime_shape_bytes;
        reserved_runtime_shape_bytes[3] = 0x80;
        write_binary_file(dynamic_malformed_dir / "dynamic_gemm.runtime_shape.reserved.bin",
                          reserved_runtime_shape_bytes);
        std::vector<uint8_t> bad_rank_runtime_shape_bytes = dynamic_runtime_shape_bytes;
        bad_rank_runtime_shape_bytes[2] = 0;
        write_binary_file(dynamic_malformed_dir / "dynamic_gemm.runtime_shape.bad_rank.bin",
                          bad_rank_runtime_shape_bytes);
        std::vector<uint8_t> bad_dims_runtime_shape_bytes = dynamic_runtime_shape_bytes;
        bad_dims_runtime_shape_bytes[4] = 3;
        write_binary_file(dynamic_malformed_dir / "dynamic_gemm.runtime_shape.bad_dims.bin",
                          bad_dims_runtime_shape_bytes);
        const std::filesystem::path truncated_runtime_shape_manifest =
            manifest_with_replaced_scalar_key(dynamic_manifest,
                                             dynamic_malformed_dir /
                                                 "dynamic_gemm.truncated_runtime_shape.manifest",
                                             "runtime_shape_table",
                                             "dynamic_gemm.runtime_shape.truncated.bin");
        const std::filesystem::path reserved_runtime_shape_manifest =
            manifest_with_replaced_scalar_key(dynamic_manifest,
                                             dynamic_malformed_dir /
                                                 "dynamic_gemm.reserved_runtime_shape.manifest",
                                             "runtime_shape_table",
                                             "dynamic_gemm.runtime_shape.reserved.bin");
        const std::filesystem::path bad_rank_runtime_shape_manifest =
            manifest_with_replaced_scalar_key(dynamic_manifest,
                                             dynamic_malformed_dir /
                                                 "dynamic_gemm.bad_rank_runtime_shape.manifest",
                                             "runtime_shape_table",
                                             "dynamic_gemm.runtime_shape.bad_rank.bin");
        const std::filesystem::path bad_dims_runtime_shape_manifest =
            manifest_with_replaced_scalar_key(dynamic_manifest,
                                             dynamic_malformed_dir /
                                                 "dynamic_gemm.bad_dims_runtime_shape.manifest",
                                             "runtime_shape_table",
                                             "dynamic_gemm.runtime_shape.bad_dims.bin");

        const bool ok =
            expect_profile_failure(no_format_manifest,
                                   "AI profile manifest is missing format",
                                   "expected ai profile manifest without format to fail") &&
            expect_profile_failure(duplicate_name_manifest,
                                   "duplicate AI profile manifest key: name",
                                   "expected ai profile manifest with duplicate name to fail") &&
            expect_profile_failure(missing_runtime_shape_manifest,
                                   "AI profile manifest is missing runtime shape table for dynamic graph",
                                   "expected dynamic manifest without runtime shape table to fail") &&
            expect_profile_failure(static_with_runtime_shape_manifest,
                                   "AI profile runtime shape table requires a dynamic graph package",
                                   "expected static manifest with runtime shape table to fail") &&
            expect_profile_failure(duplicate_runtime_shape_manifest,
                                   "duplicate AI profile manifest key: runtime_shape_table",
                                   "expected duplicate runtime shape table key to fail") &&
            expect_profile_failure(truncated_runtime_shape_manifest,
                                   "failed to resolve AI profile runtime shapes: runtime shape table byte length mismatch",
                                   "expected truncated runtime shape table to fail") &&
            expect_profile_failure(reserved_runtime_shape_manifest,
                                   "failed to resolve AI profile runtime shapes: runtime shape table reserved byte must be zero",
                                   "expected reserved runtime shape byte to fail") &&
            expect_profile_failure(bad_rank_runtime_shape_manifest,
                                   "failed to resolve AI profile runtime shapes: runtime shape rank is out of range",
                                   "expected bad runtime shape rank to fail") &&
            expect_profile_failure(bad_dims_runtime_shape_manifest,
                                   "failed to resolve AI profile runtime shapes: runtime shape dims exceed bounded tensor dims",
                                   "expected bad runtime shape dims to fail") &&
            expect_task_spec_pack_failure(oversized_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 input0 row count must be between 1 and 2",
                                          "expected oversized task-spec runtime rows to fail") &&
            expect_task_spec_pack_failure(oversized_cnn_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_cnn_v1 input0 row count must be between 3 and 4",
                                          "expected oversized CNN task-spec runtime rows to fail") &&
            expect_task_spec_pack_failure(oversized_dynamic_tiny_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_tiny_model_v1 input0 row count must be between 1 and 2",
                                          "expected oversized dynamic tiny task-spec rows to fail") &&
            expect_task_spec_pack_failure(bad_attention_task_spec,
                                          task_spec_malformed_dir,
                                          "static_tiny_attention_v1 value_vector must have exactly 2 items",
                                          "expected malformed static tiny attention task-spec to fail") &&
            expect_task_spec_pack_failure(non_object_task_spec,
                                          task_spec_malformed_dir,
                                          "task spec must be a JSON object",
                                          "expected non-object task-spec to fail") &&
            expect_task_spec_pack_failure(wrong_format_task_spec,
                                          task_spec_malformed_dir,
                                          "task spec format must be ai_task_spec_v1",
                                          "expected wrong task-spec format to fail") &&
            expect_task_spec_pack_failure(unsupported_task_kind_task_spec,
                                          task_spec_malformed_dir,
                                          "unsupported task spec task_kind: future_dynamic_attention_v1",
                                          "expected unsupported task-spec task_kind to fail") &&
            expect_task_spec_pack_failure(empty_name_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 name must be a non-empty string",
                                          "expected empty task-spec name to fail") &&
            expect_task_spec_pack_failure(negative_source_tag_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 source_tag must fit in uint32",
                                          "expected negative source_tag task-spec to fail") &&
            expect_task_spec_pack_failure(zero_max_ticks_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_cnn_v1 max_ticks must be between 1 and 4294967295",
                                          "expected zero max_ticks task-spec to fail") &&
            expect_task_spec_pack_failure(oversized_source_tag_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_tiny_model_v1 source_tag must fit in uint32",
                                          "expected oversized source_tag dynamic tiny task-spec to fail") &&
            expect_task_spec_pack_failure(oversized_max_ticks_attention_task_spec,
                                          task_spec_malformed_dir,
                                          "static_tiny_attention_v1 max_ticks must be between 1 and 4294967295",
                                          "expected oversized max_ticks attention task-spec to fail") &&
            expect_task_spec_pack_failure(unknown_top_level_key_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 task spec has unexpected top-level key: unknown_extra",
                                          "expected task-spec with unknown top-level key to fail") &&
            expect_task_spec_pack_failure(duplicate_name_task_spec,
                                          task_spec_malformed_dir,
                                          "duplicate task spec key: name",
                                          "expected task-spec with duplicate top-level key to fail") &&
            expect_task_spec_pack_failure(bool_source_tag_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 source_tag must be an integer",
                                          "expected bool source_tag task-spec to fail") &&
            expect_task_spec_pack_failure(bool_max_ticks_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_cnn_v1 max_ticks must be an integer",
                                          "expected bool max_ticks task-spec to fail") &&
            expect_task_spec_pack_failure(bool_input0_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 input0 row 0 column 0 must be an int8 value",
                                          "expected bool GEMM input0 task-spec to fail") &&
            expect_task_spec_pack_failure(bool_kernel_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_cnn_v1 input1 row 0 column 0 must be an int8 value",
                                          "expected bool CNN kernel task-spec to fail") &&
            expect_task_spec_pack_failure(path_escape_name_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 name must be a safe basename",
                                          "expected path-escape task-spec name to fail") &&
            expect_task_spec_pack_failure(newline_name_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_gemm_v1 name must be a safe basename",
                                          "expected newline task-spec name to fail") &&
            expect_task_spec_pack_failure(overflow_fp16_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_tiny_model_v1 input0 row 0 column 0 must fit in fp16",
                                          "expected overflow fp16 task-spec to fail") &&
            expect_task_spec_pack_failure(overflow_fp32_attention_task_spec,
                                          task_spec_malformed_dir,
                                          "static_tiny_attention_v1 value_vector item 0 must fit in fp32",
                                          "expected overflow fp32 attention task-spec to fail") &&
            expect_task_spec_pack_failure(non_finite_fp16_task_spec,
                                          task_spec_malformed_dir,
                                          "bounded_dynamic_tiny_model_v1 input0 row 0 column 0 must be finite",
                                          "expected non-finite fp16 task-spec to fail") &&
            expect_task_spec_pack_failure(non_finite_fp32_attention_task_spec,
                                          task_spec_malformed_dir,
                                          "static_tiny_attention_v1 value_vector item 1 must be finite",
                                          "expected non-finite fp32 attention task-spec to fail") &&
            expect_demo_v1_pack(temp_dir / "demo_v1") &&
            expect_demo_v1_run_script(temp_dir / "demo_v1_run") &&
            expect_task_spec_pack_failure(temp_dir / "demo_v1" / "custom_dynamic_gemm_fail_closed.task_spec.json",
                                          temp_dir / "demo_v1",
                                          "bounded_dynamic_gemm_v1 task spec has unexpected top-level key: unexpected_extra",
                                          "expected demo_v1 fail-closed task-spec to fail") &&
            expect_pack_and_profile(temp_dir,
                                    "cnn",
                                    "name=cnn",
                                    18,
                                    9,
                                    5,
                                    4,
                                    20,
                                    1,
                                    1,
                                    12,
                                    25,
                                    32,
                                    63,
                                    {
                                        "ai_profile_aggregate tile_count=4 scratchpad_peak_bytes=188 op_count=4",
                                        "ai_profile_op op_index=0 opcode=conv2d retired_ops=36 compute_cycles=2 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=9 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=2 opcode=layout_transpose retired_ops=9 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=3 opcode=reduce_sum retired_ops=9 compute_cycles=1 stall_cycles=1 tile_count=1",
                                    }) &&
            expect_manifest_device_profile_summary(temp_dir / "cnn.manifest",
                                                   18,
                                                   9,
                                                   5,
                                                   4,
                                                   20,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   20,
                                                   12,
                                                   63,
                                                   32,
                                                   4,
                                                   188,
                                                   4,
                                                   "expected cnn manifest to repopulate device profile summary") &&
            expect_manifest_failure_resets_device_state(
                temp_dir / "cnn.manifest",
                bad_dims_runtime_shape_manifest,
                "failed to resolve AI profile runtime shapes: runtime shape dims exceed bounded tensor dims",
                "expected failing manifest rerun to reset device state") &&
            expect_pack_and_profile(temp_dir,
                                    "gemm",
                                    "name=gemm",
                                    13,
                                    9,
                                    2,
                                    2,
                                    15,
                                    1,
                                    1,
                                    6,
                                    13,
                                    20,
                                    12,
                                    {
                                        "ai_profile_aggregate tile_count=2 scratchpad_peak_bytes=36 op_count=2",
                                        "ai_profile_op op_index=0 opcode=gemm retired_ops=8 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=1 opcode=pool_max retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                                    }) &&
            expect_manifest_device_profile_summary(temp_dir / "gemm.manifest",
                                                   13,
                                                   9,
                                                   2,
                                                   2,
                                                   15,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   16,
                                                   4,
                                                   12,
                                                   20,
                                                   2,
                                                   36,
                                                   2,
                                                   "expected gemm manifest to repopulate device profile summary") &&
            expect_manifest_completion_fault_state(
                manifest_with_replaced_scalar_key(temp_dir / "gemm.manifest",
                                                  temp_dir / "fault_gemm.manifest",
                                                  "graph_package",
                                                  graph_with_faulty_pool_attr(temp_dir / "gemm.graph.bin",
                                                                              temp_dir / "fault_gemm.graph.bin")
                                                      .filename()
                                                      .string()
                                                      .c_str()),
                6,
                AI_ACCEL_FAULT_ILLEGAL_OP,
                6,
                6,
                0,
                0,
                8,
                1,
                1,
                16,
                "expected fault gemm manifest to return completion fault state") &&
            expect_pack_and_profile(temp_dir,
                                    "tiny_model",
                                    "name=tiny_model",
                                    15,
                                    9,
                                    3,
                                    3,
                                    17,
                                    1,
                                    1,
                                    6,
                                    17,
                                    28,
                                    20,
                                    {
                                        "ai_profile_aggregate tile_count=3 scratchpad_peak_bytes=60 op_count=3",
                                        "ai_profile_op op_index=0 opcode=gemm retired_ops=12 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=1 opcode=eltwise_relu retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=2 opcode=pool_max retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                                    }) &&
            expect_manifest_device_profile_summary(temp_dir / "tiny_model.manifest",
                                                   15,
                                                   9,
                                                   3,
                                                   3,
                                                   17,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   24,
                                                   4,
                                                   20,
                                                   28,
                                                   3,
                                                   60,
                                                   3,
                                                   "expected tiny_model manifest to repopulate device profile summary") &&
            expect_pack_and_profile(temp_dir,
                                    "guest_ai_accel_demo",
                                    "name=guest_ai_accel_demo",
                                    8,
                                    6,
                                    1,
                                    1,
                                    10,
                                    1,
                                    1,
                                    3,
                                    10,
                                    16,
                                    3,
                                    {
                                        "ai_profile_aggregate tile_count=1 scratchpad_peak_bytes=16 op_count=1",
                                        "ai_profile_op op_index=0 opcode=reduce_sum retired_ops=3 compute_cycles=1 stall_cycles=1 tile_count=1",
                                    }) &&
            expect_manifest_device_profile_summary(temp_dir / "guest_ai_accel_demo.manifest",
                                                   8,
                                                   6,
                                                   1,
                                                   1,
                                                   10,
                                                   1,
                                                   1,
                                                   3,
                                                   3,
                                                   12,
                                                   4,
                                                   3,
                                                   16,
                                                   1,
                                                   16,
                                                   1,
                                                   "expected guest_ai_accel_demo manifest to repopulate device profile summary") &&
            expect_pack_and_profile(temp_dir,
                                    "tiny_attention_static",
                                    "name=tiny_attention_static",
                                    18,
                                    12,
                                    3,
                                    3,
                                    20,
                                    1,
                                    1,
                                    2,
                                    15,
                                    24,
                                    8,
                                    {
                                        "ai_profile_aggregate tile_count=3 scratchpad_peak_bytes=52 op_count=3",
                                        "ai_profile_op op_index=0 opcode=gemm retired_ops=4 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=1 opcode=softmax retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                                        "ai_profile_op op_index=2 opcode=gemm retired_ops=2 compute_cycles=1 stall_cycles=1 tile_count=1",
                                    }) &&
            expect_manifest_device_profile_summary(temp_dir / "tiny_attention_static.manifest",
                                                   18,
                                                   12,
                                                   3,
                                                   3,
                                                   20,
                                                   1,
                                                   1,
                                                   9,
                                                   3,
                                                   20,
                                                   4,
                                                   8,
                                                   24,
                                                   3,
                                                   52,
                                                   3,
                                                   "expected tiny_attention_static manifest to repopulate device profile summary") &&
            expect_pack_and_profile_task_spec_static_tiny_attention(temp_dir) &&
            expect_manifest_device_profile_summary(temp_dir / "custom_tiny_attention_static.manifest",
                                                   18,
                                                   12,
                                                   3,
                                                   3,
                                                   20,
                                                   1,
                                                   1,
                                                   9,
                                                   3,
                                                   20,
                                                   4,
                                                   8,
                                                   24,
                                                   3,
                                                   52,
                                                   3,
                                                   "expected static tiny attention task-spec manifest to repopulate device profile summary") &&
            expect_matching_binary_file(temp_dir / "custom_tiny_attention_static.graph.bin",
                                        temp_dir / "tiny_attention_static.graph.bin",
                                        "expected task-spec tiny attention to share graph package with tiny_attention_static") &&
            expect_matching_text_file(
                temp_dir / "custom_tiny_attention_static.memory_plan.txt",
                temp_dir / "tiny_attention_static.memory_plan.txt",
                "expected task-spec tiny attention to share memory plan summary with tiny_attention_static") &&
            expect_pack_and_profile_task_spec_dynamic_gemm(temp_dir) &&
            expect_manifest_device_profile_summary(temp_dir / "custom_dynamic_gemm.manifest",
                                                   15,
                                                   11,
                                                   2,
                                                   2,
                                                   17,
                                                   1,
                                                   1,
                                                   7,
                                                   4,
                                                   48,
                                                   32,
                                                   64,
                                                   80,
                                                   2,
                                                   80,
                                                   1,
                                                   "expected task-spec dynamic gemm manifest to repopulate device profile summary") &&
            expect_pack_and_profile_task_spec_dynamic_cnn(temp_dir) &&
            expect_manifest_device_profile_summary(temp_dir / "custom_dynamic_cnn.manifest",
                                                   17,
                                                   9,
                                                   4,
                                                   4,
                                                   19,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   13,
                                                   8,
                                                   28,
                                                   21,
                                                   4,
                                                   184,
                                                   4,
                                                   "expected task-spec dynamic cnn manifest to repopulate device profile summary") &&
            expect_pack_and_profile_dynamic(temp_dir) &&
            expect_manifest_device_profile_summary(temp_dir / "dynamic_gemm.manifest",
                                                   15,
                                                   11,
                                                   2,
                                                   2,
                                                   17,
                                                   1,
                                                   1,
                                                   7,
                                                   4,
                                                   48,
                                                   32,
                                                   64,
                                                   80,
                                                   2,
                                                   80,
                                                   1,
                                                   "expected dynamic_gemm manifest to repopulate device profile summary") &&
            expect_matching_binary_file(temp_dir / "custom_dynamic_gemm.graph.bin",
                                        temp_dir / "dynamic_gemm.graph.bin",
                                        "expected task-spec dynamic gemm to share graph package with dynamic_gemm") &&
            expect_matching_binary_file(
                temp_dir / "custom_dynamic_gemm.runtime_shape.bin",
                temp_dir / "dynamic_gemm.runtime_shape.bin",
                "expected task-spec dynamic gemm to share runtime shape table with dynamic_gemm") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_gemm.memory_plan.txt",
                temp_dir / "dynamic_gemm.memory_plan.txt",
                "expected task-spec dynamic gemm to share memory plan summary with dynamic_gemm") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_gemm.resolved_memory_plan.txt",
                temp_dir / "dynamic_gemm.resolved_memory_plan.txt",
                "expected task-spec dynamic gemm to share resolved memory plan summary with dynamic_gemm") &&
            expect_pack_and_profile_task_spec_dynamic_tiny_model(temp_dir) &&
            expect_pack_and_profile_dynamic_tiny_model(temp_dir) &&
            expect_manifest_device_profile_summary(temp_dir / "custom_dynamic_tiny_model.manifest",
                                                   15,
                                                   9,
                                                   3,
                                                   3,
                                                   17,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   18,
                                                   4,
                                                   10,
                                                   22,
                                                   3,
                                                   60,
                                                   3,
                                                   "expected dynamic tiny task-spec manifest to repopulate device profile summary") &&
            expect_matching_binary_file(temp_dir / "custom_dynamic_tiny_model.graph.bin",
                                        temp_dir / "dynamic_tiny_model.graph.bin",
                                        "expected task-spec dynamic tiny model to share graph package with dynamic_tiny_model") &&
            expect_matching_binary_file(
                temp_dir / "custom_dynamic_tiny_model.runtime_shape.bin",
                temp_dir / "dynamic_tiny_model.runtime_shape.bin",
                "expected task-spec dynamic tiny model to share runtime shape table with dynamic_tiny_model") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_tiny_model.memory_plan.txt",
                temp_dir / "dynamic_tiny_model.memory_plan.txt",
                "expected task-spec dynamic tiny model to share memory plan summary with dynamic_tiny_model") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_tiny_model.resolved_memory_plan.txt",
                temp_dir / "dynamic_tiny_model.resolved_memory_plan.txt",
                "expected task-spec dynamic tiny model to share resolved memory plan summary with dynamic_tiny_model") &&
            expect_manifest_device_profile_summary(temp_dir / "dynamic_tiny_model.manifest",
                                                   15,
                                                   9,
                                                   3,
                                                   3,
                                                   17,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   18,
                                                   4,
                                                   10,
                                                   22,
                                                   3,
                                                   60,
                                                   3,
                                                   "expected dynamic_tiny_model manifest to repopulate device profile summary") &&
            expect_pack_and_profile_dynamic_cnn(temp_dir) &&
            expect_matching_binary_file(temp_dir / "custom_dynamic_cnn.graph.bin",
                                        temp_dir / "dynamic_cnn.graph.bin",
                                        "expected task-spec dynamic cnn to share graph package with dynamic_cnn") &&
            expect_matching_binary_file(
                temp_dir / "custom_dynamic_cnn.runtime_shape.bin",
                temp_dir / "dynamic_cnn.runtime_shape.bin",
                "expected task-spec dynamic cnn to share runtime shape table with dynamic_cnn") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_cnn.memory_plan.txt",
                temp_dir / "dynamic_cnn.memory_plan.txt",
                "expected task-spec dynamic cnn to share memory plan summary with dynamic_cnn") &&
            expect_matching_text_file(
                temp_dir / "custom_dynamic_cnn.resolved_memory_plan.txt",
                temp_dir / "dynamic_cnn.resolved_memory_plan.txt",
                "expected task-spec dynamic cnn to share resolved memory plan summary with dynamic_cnn") &&
            expect_manifest_device_profile_summary(temp_dir / "dynamic_cnn.manifest",
                                                   17,
                                                   9,
                                                   4,
                                                   4,
                                                   19,
                                                   1,
                                                   1,
                                                   6,
                                                   3,
                                                   13,
                                                   8,
                                                   28,
                                                   21,
                                                   4,
                                                   184,
                                                   4,
                                                   "expected dynamic_cnn manifest to repopulate device profile summary") &&
            expect_manifest_rerun_refresh(temp_dir / "cnn.manifest",
                                          temp_dir / "gemm.manifest",
                                          13,
                                          9,
                                          2,
                                          2,
                                          15,
                                          1,
                                          1,
                                          6,
                                          3,
                                          16,
                                          4,
                                          12,
                                          20,
                                          2,
                                          36,
                                          2,
                                          "expected rerun manifest path to refresh device profile summary") &&
            expect_manifest_rerun_refresh(temp_dir / "dynamic_gemm.manifest",
                                          temp_dir / "dynamic_cnn.manifest",
                                          17,
                                          9,
                                          4,
                                          4,
                                          19,
                                          1,
                                          1,
                                          6,
                                          3,
                                          13,
                                          8,
                                          28,
                                          21,
                                          4,
                                          184,
                                          4,
                                          "expected rerun dynamic manifest path to refresh device profile summary") &&
            expect_manifest_timeout_state(
                manifest_with_replaced_scalar_key(temp_dir / "dynamic_cnn.manifest",
                                                  temp_dir / "dynamic_cnn.timeout.manifest",
                                                  "max_ticks",
                                                  "1"),
                1,
                1,
                1,
                0,
                0,
                2,
                1,
                0,
                "expected timeout manifest to leave default device profile state");

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

#include "machine.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../exec/functional_backend.h"
#include "../exec/pipeline_backend.h"

namespace {

struct GprAlias {
    const char* name;
    uint32_t index;
};

constexpr std::array<GprAlias, 33> kGprAliases = {{
    {"zero", 0}, {"ra", 1},  {"sp", 2},  {"gp", 3},  {"tp", 4},  {"t0", 5},  {"t1", 6},
    {"t2", 7},   {"s0", 8},  {"fp", 8},  {"s1", 9},  {"a0", 10}, {"a1", 11}, {"a2", 12},
    {"a3", 13},  {"a4", 14}, {"a5", 15}, {"a6", 16}, {"a7", 17}, {"s2", 18}, {"s3", 19},
    {"s4", 20},  {"s5", 21}, {"s6", 22}, {"s7", 23}, {"s8", 24}, {"s9", 25}, {"s10", 26},
    {"s11", 27}, {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31},
}};

std::string normalize_gpr_name(const std::string& name) {
    std::string normalized = name;
    for (char& ch : normalized) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return normalized;
}

uint32_t parse_gpr_index(const std::string& reg_name) {
    const std::string normalized = normalize_gpr_name(reg_name);
    if (normalized.size() >= 2 && normalized[0] == 'x') {
        char* end = nullptr;
        const unsigned long index = std::strtoul(normalized.c_str() + 1, &end, 10);
        if (end != nullptr && *end == '\0' && index < 32UL) {
            return static_cast<uint32_t>(index);
        }
    }

    for (const GprAlias& alias : kGprAliases) {
        if (normalized == alias.name) {
            return alias.index;
        }
    }

    throw std::runtime_error("unknown GPR name: " + reg_name);
}

struct ParsedAiProfileManifest {
    std::string name{};
    std::filesystem::path manifest_path{};
    std::filesystem::path graph_package_path{};
    std::filesystem::path runtime_shape_table_path{};
    std::vector<std::filesystem::path> input_paths{};
    std::vector<std::filesystem::path> output_paths{};
    uint32_t max_ticks{128};
    uint32_t source_tag{0};
};

constexpr uint64_t kAiProfileSubmitQueueAddr = MEM_BASE + 0x22000;
constexpr uint64_t kAiProfileCompleteQueueAddr = MEM_BASE + 0x24000;
constexpr uint64_t kAiProfileGraphPackageAddr = MEM_BASE + 0x26000;
constexpr uint64_t kAiProfileInputTableAddr = MEM_BASE + 0x28000;
constexpr uint64_t kAiProfileOutputTableAddr = MEM_BASE + 0x29000;
constexpr uint64_t kAiProfileTensorBaseAddr = MEM_BASE + 0x2A000;
constexpr uint32_t kAiProfileQueueEntries = 4;

std::string trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open file: " + path.string());
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_binary_file(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("failed to write output file: " + path.string());
    }
}

ParsedAiProfileManifest parse_ai_profile_manifest_file(const std::string& manifest_path) {
    ParsedAiProfileManifest manifest{};
    manifest.manifest_path = std::filesystem::path(manifest_path);
    std::ifstream in(manifest.manifest_path);
    if (!in) {
        throw std::runtime_error("failed to open AI profile manifest: " + manifest.manifest_path.string());
    }

    const std::filesystem::path base_dir = manifest.manifest_path.parent_path();
    bool seen_format = false;
    bool seen_name = false;
    bool seen_graph_package = false;
    bool seen_runtime_shape_table = false;
    bool seen_max_ticks = false;
    bool seen_source_tag = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("invalid AI profile manifest line: " + trimmed);
        }
        const std::string key = trim(trimmed.substr(0, eq));
        const std::string value = trim(trimmed.substr(eq + 1));
        if (key == "format") {
            if (seen_format) {
                throw std::runtime_error("duplicate AI profile manifest key: format");
            }
            seen_format = true;
            if (value != "ai_proto_manifest_v1") {
                throw std::runtime_error("unsupported AI profile manifest format: " + value);
            }
        } else if (key == "name") {
            if (seen_name) {
                throw std::runtime_error("duplicate AI profile manifest key: name");
            }
            seen_name = true;
            manifest.name = value;
        } else if (key == "graph_package") {
            if (seen_graph_package) {
                throw std::runtime_error("duplicate AI profile manifest key: graph_package");
            }
            seen_graph_package = true;
            manifest.graph_package_path = base_dir / value;
        } else if (key == "runtime_shape_table") {
            if (seen_runtime_shape_table) {
                throw std::runtime_error("duplicate AI profile manifest key: runtime_shape_table");
            }
            seen_runtime_shape_table = true;
            manifest.runtime_shape_table_path = base_dir / value;
        } else if (key == "input") {
            manifest.input_paths.push_back(base_dir / value);
        } else if (key == "output") {
            manifest.output_paths.push_back(base_dir / value);
        } else if (key == "expected_output") {
            continue;
        } else if (key == "max_ticks") {
            if (seen_max_ticks) {
                throw std::runtime_error("duplicate AI profile manifest key: max_ticks");
            }
            seen_max_ticks = true;
            manifest.max_ticks = static_cast<uint32_t>(std::stoul(value, nullptr, 0));
        } else if (key == "source_tag") {
            if (seen_source_tag) {
                throw std::runtime_error("duplicate AI profile manifest key: source_tag");
            }
            seen_source_tag = true;
            manifest.source_tag = static_cast<uint32_t>(std::stoul(value, nullptr, 0));
        } else {
            throw std::runtime_error("unknown AI profile manifest key: " + key);
        }
    }

    if (!seen_format) {
        throw std::runtime_error("AI profile manifest is missing format");
    }
    if (manifest.name.empty()) {
        throw std::runtime_error("AI profile manifest is missing name");
    }
    if (manifest.graph_package_path.empty()) {
        throw std::runtime_error("AI profile manifest is missing graph package path");
    }
    if (manifest.input_paths.empty()) {
        throw std::runtime_error("AI profile manifest is missing input files");
    }
    if (manifest.output_paths.empty()) {
        throw std::runtime_error("AI profile manifest is missing output files");
    }
    if (manifest.max_ticks == 0) {
        throw std::runtime_error("AI profile manifest max_ticks must be non-zero");
    }
    return manifest;
}

const char* ai_shape_mode_name(AiShapeMode shape_mode) {
    switch (shape_mode) {
    case AiShapeMode::Static:
        return "static";
    case AiShapeMode::DynamicBounded:
        return "dynamic_bounded";
    }
    return "unknown";
}

std::string format_runtime_shape_summary(const std::vector<AiRuntimeShapeEntry>& runtime_shapes) {
    if (runtime_shapes.empty()) {
        return "none";
    }

    std::ostringstream out;
    for (size_t i = 0; i < runtime_shapes.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "t" << runtime_shapes[i].tensor_index << ":";
        for (uint8_t axis = 0; axis < runtime_shapes[i].rank; ++axis) {
            if (axis != 0) {
                out << "x";
            }
            out << runtime_shapes[i].dims[axis];
        }
    }
    return out.str();
}

uint64_t tensor_byte_size(const AiTensorMetadata& tensor) {
    uint64_t element_count = 1;
    for (uint8_t axis = 0; axis < tensor.rank; ++axis) {
        element_count *= tensor.dims[axis];
    }
    return element_count * ai_dtype_size_bytes(tensor.dtype);
}

uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

void store_u32_checked(Bus& bus, uint32_t reg, uint32_t value, const char* label) {
    if (!bus.try_store(AI_ACCEL_BASE + reg, value, 4)) {
        throw std::runtime_error(std::string("failed to program AI accelerator register: ") + label);
    }
}

uint64_t load_u32_checked(Bus& bus, uint32_t reg, const char* label) {
    uint64_t value = 0;
    if (!bus.try_load(AI_ACCEL_BASE + reg, 4, value)) {
        throw std::runtime_error(std::string("failed to read AI accelerator register: ") + label);
    }
    return value;
}

uint64_t load_counter_checked(Bus& bus,
                              uint32_t low_reg,
                              uint32_t high_reg,
                              const char* label) {
    uint64_t low = 0;
    uint64_t high = 0;
    if (!bus.try_load(AI_ACCEL_BASE + low_reg, 4, low) ||
        !bus.try_load(AI_ACCEL_BASE + high_reg, 4, high)) {
        throw std::runtime_error(std::string("failed to read AI accelerator counter: ") + label);
    }
    return (high << 32) | static_cast<uint32_t>(low);
}

void write_ram_bytes(Ram& ram, uint64_t addr, const std::vector<uint8_t>& bytes) {
    if (!bytes.empty()) {
        ram.write_bytes(addr, bytes.data(), bytes.size());
    }
}

void write_u64_table(Ram& ram, uint64_t addr, const std::vector<uint64_t>& values) {
    if (!values.empty()) {
        ram.write_bytes(addr, values.data(), values.size() * sizeof(uint64_t));
    }
}

}  // namespace

const char* block_transport_name(BlockTransport transport) {
    switch (transport) {
    case BlockTransport::SimpleStorage:
        return "simple_storage";
    case BlockTransport::VirtioBlk:
        return "virtio-blk";
    }
    return "unknown";
}

BlockTransport parse_block_transport(const std::string& name) {
    if (name == "simple_storage") {
        return BlockTransport::SimpleStorage;
    }
    if (name == "virtio-blk" || name == "virtio_blk") {
        return BlockTransport::VirtioBlk;
    }
    throw std::runtime_error("unknown block transport: " + name);
}

Machine::Machine()
    : uart_(plic_),
      virtio_mmio_(plic_, VIRTIO_MMIO_PLIC_SOURCE, virtio_blk_),
      ai_accelerator_(plic_, AI_ACCEL_PLIC_SOURCE),
      bus_(ram_) {
    cpu_.csr().bind_clint(&clint_);
    ai_accelerator_.bind_bus(bus_);
    bus_.attach(uart_);
    bus_.attach(clint_);
    bus_.attach(plic_);
    bus_.attach(ai_accelerator_);
    rebuild_backend();
}

void Machine::set_backend_kind(BackendKind kind) {
    backend_kind_ = kind;
    rebuild_backend();
}

BackendKind Machine::backend_kind() const {
    return backend_kind_;
}

void Machine::set_l1_data_cache_enabled(bool enabled) {
    cpu_.l1_data_cache().clear();
    cpu_.l1_data_cache().set_enabled(enabled);
}

bool Machine::l1_data_cache_enabled() const {
    return cpu_.l1_data_cache().enabled();
}

void Machine::set_block_transport(BlockTransport transport) {
    if (block_transport_bound_ && block_transport_ != transport) {
        throw std::runtime_error("block transport already bound");
    }
    block_transport_ = transport;
}

void Machine::bind_block_transport() {
    if (block_transport_bound_) {
        return;
    }

    switch (block_transport_) {
    case BlockTransport::SimpleStorage:
        bus_.attach(storage_);
        break;
    case BlockTransport::VirtioBlk:
        virtio_mmio_.bind_bus(bus_);
        bus_.attach(virtio_mmio_);
        break;
    }

    block_transport_bound_ = true;
}

void Machine::rebuild_backend() {
    switch (backend_kind_) {
    case BackendKind::Functional:
        backend_ = std::make_unique<FunctionalBackend>(cpu_, bus_);
        break;
    case BackendKind::Pipeline:
        backend_ = std::make_unique<PipelineBackend>(cpu_, bus_);
        break;
    }
}

void Machine::finish_image_load(uint64_t entry, Ram& staged_ram) {
    // Image reload swaps in a freshly loaded RAM image; device state is still
    // intentionally preserved, but reload should not carry over stale storage
    // command errors into the next guest image.
    ram_.swap(staged_ram);
    if (block_transport_ == BlockTransport::SimpleStorage) {
        bus_.try_store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_NONE, 8);
    }
    cpu_init(cpu_, entry);
    rebuild_backend();
    loaded_ = true;
}

void Machine::load_elf(const std::string& path) {
    bind_block_transport();
    Ram staged_ram;
    const uint64_t entry = elf_loader_.load(staged_ram, path.c_str());
    finish_image_load(entry, staged_ram);
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    bind_block_transport();
    Ram staged_ram;
    binary_loader_.load(staged_ram, path.c_str(), addr);
    finish_image_load(addr, staged_ram);
}

void Machine::load_binary_payload(const std::string& path, uint64_t addr) {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    const uint64_t byte_count = binary_loader_.load(ram_, path.c_str(), addr);
    cpu_.l1_data_cache().invalidate_range(addr, byte_count);
}

void Machine::set_gpr(const std::string& reg_name, uint64_t value) {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    cpu_.core().write_gpr(parse_gpr_index(reg_name), value);
}

void Machine::attach_storage_image(const std::string& path,
                                   bool ready,
                                   bool valid_magic) {
    bind_block_transport();
    switch (block_transport_) {
    case BlockTransport::SimpleStorage:
        storage_.load_image(path.c_str());
        storage_.set_ready(ready);
        storage_.set_magic_valid(valid_magic);
        break;
    case BlockTransport::VirtioBlk:
        if (!ready || !valid_magic) {
            throw std::runtime_error("virtio-blk transport does not support simple_storage readiness or magic overrides");
        }
        virtio_blk_.load_image(path.c_str());
        break;
    }
}

Machine::AiProfileRunResult Machine::run_ai_profile_manifest(const std::string& manifest_path) {
    const ParsedAiProfileManifest manifest = parse_ai_profile_manifest_file(manifest_path);
    AiProfileRunResult result{};
    result.workload_name = manifest.name;
    result.manifest_path = manifest.manifest_path.string();
    result.graph_package_path = manifest.graph_package_path.string();

    Ram cleared_ram;
    ram_.swap(cleared_ram);
    loaded_ = false;
    store_u32_checked(bus_, AI_ACCEL_REG_CONTROL, AI_ACCEL_CONTROL_RESET, "control");

    const std::vector<uint8_t> graph_bytes = read_binary_file(manifest.graph_package_path);
    result.graph_package_bytes = static_cast<uint32_t>(graph_bytes.size());
    AiGraphPackage package{};
    std::string error;
    if (!parse_ai_graph_package(graph_bytes, package, error)) {
        throw std::runtime_error("failed to parse AI profile graph package: " + error);
    }
    result.shape_mode = ai_shape_mode_name(package.shape_mode);

    AiGraphPackage resolved_package = package;
    std::vector<uint8_t> runtime_shape_bytes{};
    std::vector<AiRuntimeShapeEntry> runtime_shapes{};
    uint32_t runtime_shape_table_offset = 0;
    if (package.shape_mode == AiShapeMode::DynamicBounded) {
        if (manifest.runtime_shape_table_path.empty()) {
            throw std::runtime_error("AI profile manifest is missing runtime shape table for dynamic graph");
        }
        runtime_shape_bytes = read_binary_file(manifest.runtime_shape_table_path);
        if (!parse_ai_runtime_shape_table(runtime_shape_bytes,
                                          package.dynamic_tensors.size(),
                                          runtime_shapes,
                                          error) ||
            !resolve_ai_runtime_shape_package(package, runtime_shapes, resolved_package, error)) {
            throw std::runtime_error("failed to resolve AI profile runtime shapes: " + error);
        }
        runtime_shape_table_offset = static_cast<uint32_t>(align_up(graph_bytes.size(), 64));
        const uint64_t runtime_shape_addr =
            kAiProfileGraphPackageAddr + static_cast<uint64_t>(runtime_shape_table_offset);
        if (runtime_shape_addr < kAiProfileGraphPackageAddr ||
            runtime_shape_addr + runtime_shape_bytes.size() > kAiProfileInputTableAddr) {
            throw std::runtime_error("AI profile runtime shape table exceeds reserved graph window");
        }
    } else if (!manifest.runtime_shape_table_path.empty()) {
        throw std::runtime_error("AI profile runtime shape table requires a dynamic graph package");
    }
    result.runtime_shapes = format_runtime_shape_summary(runtime_shapes);
    const AiGraphPackage& concrete_package =
        package.shape_mode == AiShapeMode::DynamicBounded ? resolved_package : package;

    size_t expected_inputs = 0;
    size_t expected_outputs = 0;
    for (const AiTensorMetadata& tensor : concrete_package.tensors) {
        if (tensor.role == AiTensorRole::Input || tensor.role == AiTensorRole::Weight ||
            tensor.role == AiTensorRole::Constant) {
            ++expected_inputs;
        } else if (tensor.role == AiTensorRole::Output) {
            ++expected_outputs;
        }
    }
    if (manifest.input_paths.size() != expected_inputs) {
        throw std::runtime_error("AI profile manifest input count does not match graph package");
    }
    if (manifest.output_paths.size() != expected_outputs) {
        throw std::runtime_error("AI profile manifest output count does not match graph package");
    }

    std::vector<uint64_t> input_table(concrete_package.tensors.size(), 0);
    std::vector<uint64_t> output_table(concrete_package.tensors.size(), 0);
    struct OutputBinding {
        std::filesystem::path path{};
        uint64_t addr{0};
        uint64_t size{0};
    };
    std::vector<OutputBinding> outputs{};

    size_t input_index = 0;
    size_t output_index = 0;
    uint64_t tensor_addr = kAiProfileTensorBaseAddr;
    for (size_t tensor_index = 0; tensor_index < concrete_package.tensors.size(); ++tensor_index) {
        const AiTensorMetadata& tensor = concrete_package.tensors[tensor_index];
        const uint64_t bytes = tensor_byte_size(tensor);
        switch (tensor.role) {
        case AiTensorRole::Input:
        case AiTensorRole::Weight:
        case AiTensorRole::Constant: {
            const std::vector<uint8_t> input_bytes = read_binary_file(manifest.input_paths[input_index++]);
            if (input_bytes.size() != bytes) {
                throw std::runtime_error("AI profile input byte size does not match tensor metadata");
            }
            write_ram_bytes(ram_, tensor_addr, input_bytes);
            input_table[tensor_index] = tensor_addr;
            tensor_addr = align_up(tensor_addr + bytes, 64);
            break;
        }
        case AiTensorRole::Output: {
            const std::vector<uint8_t> zeros(static_cast<size_t>(bytes), 0);
            write_ram_bytes(ram_, tensor_addr, zeros);
            output_table[tensor_index] = tensor_addr;
            outputs.push_back(OutputBinding{manifest.output_paths[output_index++], tensor_addr, bytes});
            tensor_addr = align_up(tensor_addr + bytes, 64);
            break;
        }
        case AiTensorRole::Intermediate:
        case AiTensorRole::Invalid:
            break;
        }
    }

    write_ram_bytes(ram_, kAiProfileGraphPackageAddr, graph_bytes);
    if (!runtime_shape_bytes.empty()) {
        write_ram_bytes(ram_,
                        kAiProfileGraphPackageAddr + static_cast<uint64_t>(runtime_shape_table_offset),
                        runtime_shape_bytes);
    }
    write_u64_table(ram_, kAiProfileInputTableAddr, input_table);
    write_u64_table(ram_, kAiProfileOutputTableAddr, output_table);

    const AiSubmissionDescriptor descriptor{
        .token = 0xA1A1A1A1ULL,
        .graph_package_addr = kAiProfileGraphPackageAddr,
        .graph_package_bytes = result.graph_package_bytes,
        .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
        .input_table_addr = kAiProfileInputTableAddr,
        .output_table_addr = kAiProfileOutputTableAddr,
        .source_tag = manifest.source_tag,
        .runtime_shape_table_offset = runtime_shape_table_offset,
    };
    std::array<uint8_t, kAiSubmissionDescriptorBytes> descriptor_bytes{};
    encode_ai_submission_descriptor(descriptor, descriptor_bytes);
    ram_.write_bytes(kAiProfileSubmitQueueAddr, descriptor_bytes.data(), descriptor_bytes.size());

    store_u32_checked(bus_, AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW,
                      static_cast<uint32_t>(kAiProfileSubmitQueueAddr),
                      "submit queue base low");
    store_u32_checked(bus_, AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH,
                      static_cast<uint32_t>(kAiProfileSubmitQueueAddr >> 32),
                      "submit queue base high");
    store_u32_checked(bus_, AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, kAiProfileQueueEntries, "submit queue size");
    store_u32_checked(bus_, AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                      static_cast<uint32_t>(kAiProfileCompleteQueueAddr),
                      "complete queue base low");
    store_u32_checked(bus_, AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                      static_cast<uint32_t>(kAiProfileCompleteQueueAddr >> 32),
                      "complete queue base high");
    store_u32_checked(bus_, AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, kAiProfileQueueEntries, "complete queue size");
    store_u32_checked(bus_, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "submit queue tail");
    store_u32_checked(bus_, AI_ACCEL_REG_DOORBELL, 1, "doorbell");

    for (uint32_t tick = 0; tick < manifest.max_ticks; ++tick) {
        bus_.tick();
        ++result.ticks;
        if (load_u32_checked(bus_, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, "completion queue tail") != 0) {
            result.completed = true;
            break;
        }
    }

    if (result.completed) {
        std::array<uint8_t, kAiCompletionEntryBytes> completion_bytes{};
        for (size_t i = 0; i < completion_bytes.size(); ++i) {
            completion_bytes[i] = static_cast<uint8_t>(ram_.load(kAiProfileCompleteQueueAddr + i, 1) & 0xffU);
        }
        AiCompletionEntry completion{};
        decode_ai_completion_entry(completion_bytes, completion);
        result.completion_status = completion.status;
        result.fault_code = completion.fault_code;
        result.source_tag = completion.source_tag;
        result.bytes_moved = completion.bytes_moved;
        result.retired_ops = completion.retired_ops;
    } else {
        result.completion_status = AI_ACCEL_COMPLETION_STATUS_FAULT;
        result.fault_code = AI_ACCEL_FAULT_TIMEOUT;
        result.source_tag = manifest.source_tag;
    }

    result.device_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_DEVICE_CYCLES_LOW, AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                             "device cycles");
    result.dma_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_DMA_CYCLES_LOW, AI_ACCEL_REG_DMA_CYCLES_HIGH,
                             "dma cycles");
    result.compute_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_COMPUTE_CYCLES_LOW, AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                             "compute cycles");
    result.stall_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_STALL_CYCLES_LOW, AI_ACCEL_REG_STALL_CYCLES_HIGH,
                             "stall cycles");
    result.busy_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_BUSY_CYCLES_LOW, AI_ACCEL_REG_BUSY_CYCLES_HIGH,
                             "busy cycles");
    result.queue_cycles =
        load_counter_checked(bus_, AI_ACCEL_REG_QUEUE_CYCLES_LOW, AI_ACCEL_REG_QUEUE_CYCLES_HIGH,
                             "queue cycles");
    result.completion_cycles =
        load_counter_checked(bus_,
                             AI_ACCEL_REG_COMPLETION_CYCLES_LOW,
                             AI_ACCEL_REG_COMPLETION_CYCLES_HIGH,
                             "completion cycles");
    result.effective_ops_per_cycle =
        static_cast<uint32_t>(load_u32_checked(bus_,
                                               AI_ACCEL_REG_EFFECTIVE_OPS_PER_CYCLE,
                                               "effective ops per cycle"));
    result.utilization =
        static_cast<uint32_t>(load_u32_checked(bus_, AI_ACCEL_REG_UTILIZATION, "utilization"));
    const AiAcceleratorProfileSummary& profile_summary = ai_accelerator_.profile_summary();
    result.tile_count = profile_summary.tile_count;
    result.scratchpad_peak_bytes = profile_summary.scratchpad_peak_bytes;
    result.op_summaries = profile_summary.op_summaries;

    if (result.completed && result.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS) {
        for (const OutputBinding& output : outputs) {
            std::vector<uint8_t> bytes(static_cast<size_t>(output.size), 0);
            for (size_t i = 0; i < bytes.size(); ++i) {
                bytes[i] = static_cast<uint8_t>(ram_.load(output.addr + i, 1) & 0xffU);
            }
            write_binary_file(output.path, bytes);
        }
    }

    return result;
}

void Machine::step() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    if (!cpu_.core().halted()) {
        backend_->step();
    }
}

void Machine::run() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }

    while (!cpu_.core().halted()) {
        step();
    }
}

CPU& Machine::cpu() {
    return cpu_;
}

const CPU& Machine::cpu() const {
    return cpu_;
}

Bus& Machine::bus() {
    return bus_;
}

const Bus& Machine::bus() const {
    return bus_;
}

Uart16550& Machine::uart() {
    return uart_;
}

const Uart16550& Machine::uart() const {
    return uart_;
}

Clint& Machine::clint() {
    return clint_;
}

const Clint& Machine::clint() const {
    return clint_;
}

Plic& Machine::plic() {
    return plic_;
}

const Plic& Machine::plic() const {
    return plic_;
}

BlockTransport Machine::block_transport() const {
    return block_transport_;
}

SimpleStorage& Machine::storage() {
    return storage_;
}

const SimpleStorage& Machine::storage() const {
    return storage_;
}

VirtioBlk& Machine::virtio_blk() {
    return virtio_blk_;
}

const VirtioBlk& Machine::virtio_blk() const {
    return virtio_blk_;
}

AiAccelerator& Machine::ai_accelerator() {
    return ai_accelerator_;
}

const AiAccelerator& Machine::ai_accelerator() const {
    return ai_accelerator_;
}

ExecutionBackend& Machine::backend() {
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    return *backend_;
}

const ExecutionBackend& Machine::backend() const {
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    return *backend_;
}

bool Machine::loaded() const {
    return loaded_;
}

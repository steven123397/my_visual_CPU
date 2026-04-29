#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "../cpu.h"

#include "../devices/clint.h"
#include "../devices/ai_accelerator.h"
#include "../devices/plic.h"
#include "../devices/simple_storage.h"
#include "../devices/uart16550.h"
#include "../devices/virtio_blk.h"
#include "../devices/virtio_mmio.h"
#include "../exec/backend.h"
#include "../loader/binary_loader.h"
#include "../loader/elf_loader.h"
#include "../mem/bus.h"
#include "../mem/ram.h"

enum class BackendKind : uint8_t {
    Functional,
    Pipeline,
};

enum class BlockTransport : uint8_t {
    SimpleStorage,
    VirtioBlk,
};

const char* block_transport_name(BlockTransport transport);
BlockTransport parse_block_transport(const std::string& name);

class Machine {
public:
    struct AiProfileRunResult {
        std::string workload_name{};
        std::string manifest_path{};
        std::string graph_package_path{};
        std::string shape_mode{"static"};
        std::string runtime_shapes{"none"};
        uint32_t graph_package_bytes{0};
        uint64_t tile_count{0};
        uint32_t scratchpad_peak_bytes{0};
        std::vector<AiAcceleratorOpProfileSummary> op_summaries{};
        uint64_t ticks{0};
        uint32_t completion_status{AI_ACCEL_COMPLETION_STATUS_SUCCESS};
        uint32_t fault_code{AI_ACCEL_FAULT_NONE};
        uint32_t source_tag{0};
        uint64_t bytes_moved{0};
        uint64_t retired_ops{0};
        uint64_t device_cycles{0};
        uint64_t dma_cycles{0};
        uint64_t compute_cycles{0};
        uint64_t stall_cycles{0};
        uint64_t busy_cycles{0};
        uint64_t queue_cycles{0};
        uint64_t completion_cycles{0};
        uint32_t effective_ops_per_cycle{0};
        uint32_t utilization{0};
        bool completed{false};
    };

    Machine();

    void set_backend_kind(BackendKind kind);
    void set_l1_data_cache_enabled(bool enabled);
    bool l1_data_cache_enabled() const;
    void set_block_transport(BlockTransport transport);
    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);
    void load_binary_payload(const std::string& path, uint64_t addr);
    void set_gpr(const std::string& reg_name, uint64_t value);
    void attach_storage_image(const std::string& path,
                              bool ready = true,
                              bool valid_magic = true);
    AiProfileRunResult run_ai_profile_manifest(const std::string& manifest_path);
    void step();
    void run();
    CPU& cpu();
    const CPU& cpu() const;
    Bus& bus();
    const Bus& bus() const;
    Uart16550& uart();
    const Uart16550& uart() const;
    Clint& clint();
    const Clint& clint() const;
    Plic& plic();
    const Plic& plic() const;
    BlockTransport block_transport() const;
    SimpleStorage& storage();
    const SimpleStorage& storage() const;
    VirtioBlk& virtio_blk();
    const VirtioBlk& virtio_blk() const;
    AiAccelerator& ai_accelerator();
    const AiAccelerator& ai_accelerator() const;
    ExecutionBackend& backend();
    const ExecutionBackend& backend() const;
    bool loaded() const;

private:
    void bind_block_transport();
    void rebuild_backend();
    void finish_image_load(uint64_t entry, Ram& staged_ram);

    CPU cpu_{};
    Ram ram_;
    Plic plic_;
    Uart16550 uart_;
    SimpleStorage storage_;
    VirtioBlk virtio_blk_;
    VirtioMmio virtio_mmio_;
    AiAccelerator ai_accelerator_;
    Clint clint_;
    ElfLoader elf_loader_;
    BinaryLoader binary_loader_;
    Bus bus_;
    BackendKind backend_kind_{BackendKind::Functional};
    BlockTransport block_transport_{BlockTransport::SimpleStorage};
    bool block_transport_bound_{false};
    std::unique_ptr<ExecutionBackend> backend_;
    bool loaded_{false};
};

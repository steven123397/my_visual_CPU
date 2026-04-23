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
    Machine();

    void set_backend_kind(BackendKind kind);
    void set_block_transport(BlockTransport transport);
    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);
    void load_binary_payload(const std::string& path, uint64_t addr);
    void set_gpr(const std::string& reg_name, uint64_t value);
    void attach_storage_image(const std::string& path,
                              bool ready = true,
                              bool valid_magic = true);
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

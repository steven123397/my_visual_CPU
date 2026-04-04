#pragma once

#include <stdint.h>
#include <memory>
#include <string>

#include "../cpu.h"

#include "../devices/clint.h"
#include "../devices/plic.h"
#include "../devices/simple_storage.h"
#include "../devices/uart16550.h"
#include "../exec/backend.h"
#include "../loader/binary_loader.h"
#include "../loader/elf_loader.h"
#include "../mem/bus.h"
#include "../mem/ram.h"

enum class BackendKind : uint8_t {
    Functional,
    Pipeline,
};

class Machine {
public:
    Machine();

    void set_backend_kind(BackendKind kind);
    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);
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
    SimpleStorage& storage();
    const SimpleStorage& storage() const;
    ExecutionBackend& backend();
    const ExecutionBackend& backend() const;
    bool loaded() const;

private:
    void rebuild_backend();
    void finish_image_load(uint64_t entry, Ram& staged_ram);

    CPU cpu_{};
    Ram ram_;
    Plic plic_;
    Uart16550 uart_;
    SimpleStorage storage_;
    Clint clint_;
    ElfLoader elf_loader_;
    BinaryLoader binary_loader_;
    Bus bus_;
    BackendKind backend_kind_{BackendKind::Functional};
    std::unique_ptr<ExecutionBackend> backend_;
    bool loaded_{false};
};

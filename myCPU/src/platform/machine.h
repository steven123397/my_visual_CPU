#pragma once

#include <stdint.h>
#include <string>

#include "../cpu.h"

#include "../devices/clint.h"
#include "../devices/plic.h"
#include "../devices/simple_storage.h"
#include "../devices/uart16550.h"
#include "../loader/binary_loader.h"
#include "../loader/elf_loader.h"
#include "../mem/bus.h"
#include "../mem/ram.h"

class Machine {
public:
    Machine();

    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);
    void attach_storage_image(const std::string& path);
    void run();

private:
    CPU cpu_{};
    Ram ram_;
    Plic plic_;
    Uart16550 uart_;
    SimpleStorage storage_;
    Clint clint_;
    ElfLoader elf_loader_;
    BinaryLoader binary_loader_;
    Bus bus_;
    bool loaded_{false};
};

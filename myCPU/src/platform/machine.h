#pragma once

#include <stdint.h>
#include <string>

#include "../cpu.h"

#include "../devices/clint.h"
#include "../devices/uart16550.h"
#include "../mem/bus.h"
#include "../mem/ram.h"

class Machine {
public:
    Machine();

    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);
    void run();

private:
    CPU cpu_{};
    Ram ram_;
    Uart16550 uart_;
    Clint clint_;
    Bus bus_;
    bool loaded_{false};
};

#pragma once

extern "C" {
#include "../memory.h"
}

class Ram {
public:
    Ram();
    ~Ram();

    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;

    uint64_t load(uint64_t addr, int size);
    void store(uint64_t addr, uint64_t value, int size);
    void load_binary(const char* path, uint64_t addr);

    Memory* raw_memory();
    const Memory* raw_memory() const;

private:
    Memory mem_{};
};

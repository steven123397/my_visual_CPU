#pragma once

#include <cstddef>
#include <cstdint>

#include "../devices/device.h"

extern "C" {
#include "../memory.h"
}

class Ram : public Device {
public:
    Ram();
    ~Ram();

    Ram(const Ram&) = delete;
    Ram& operator=(const Ram&) = delete;

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;
    void write_bytes(uint64_t addr, const void* data, size_t size);
    void fill(uint64_t addr, uint8_t value, size_t size);

private:
    Memory mem_{};
};

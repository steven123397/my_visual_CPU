#pragma once

#include <cstdint>
#include <vector>

#include "device.h"
#include "../platform/address_map.h"

class SimpleStorage : public Device {
public:
    SimpleStorage();

    uint64_t load(uint64_t addr, int size) override;
    void store(uint64_t addr, uint64_t value, int size) override;

    void load_image(const char* path);

private:
    uint64_t register_value(uint32_t offset) const;
    uint64_t load_data_port(int size);
    void store_data_port(uint64_t value, int size);

    static constexpr uint64_t kMagic = 0x31474b4254534d4dULL;
    static constexpr uint64_t kBlockSize = 512;
    static constexpr uint32_t kMagicOffset = 0x00;
    static constexpr uint32_t kCapacityOffset = 0x08;
    static constexpr uint32_t kBlockSizeOffset = 0x10;
    static constexpr uint32_t kCursorOffset = 0x18;
    static constexpr uint32_t kStatusOffset = 0x20;
    static constexpr uint32_t kDataOffset = 0x28;

    std::vector<uint8_t> data_{};
    uint64_t cursor_{0};
    bool attached_{false};
};

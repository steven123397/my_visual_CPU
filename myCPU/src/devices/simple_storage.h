#pragma once

#include <array>
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
    uint64_t load_data_window(uint32_t offset, int size) const;
    void store_data_window(uint32_t offset, uint64_t value, int size);
    void execute_command(uint64_t command);
    void set_error(uint64_t error_code);
    void clear_error();

    static constexpr uint64_t kMagic = STORAGE_MMIO_MAGIC;
    static constexpr uint64_t kVersion = STORAGE_MMIO_VERSION;
    static constexpr uint64_t kBlockSize = STORAGE_BLOCK_SIZE;
    static constexpr uint32_t kMagicOffset = STORAGE_REG_MAGIC;
    static constexpr uint32_t kVersionOffset = STORAGE_REG_VERSION;
    static constexpr uint32_t kBlockSizeOffset = STORAGE_REG_BLOCK_SIZE;
    static constexpr uint32_t kCapacityBlocksOffset = STORAGE_REG_CAPACITY_BLOCKS;
    static constexpr uint32_t kStatusOffset = STORAGE_REG_STATUS;
    static constexpr uint32_t kCommandOffset = STORAGE_REG_COMMAND;
    static constexpr uint32_t kLbaOffset = STORAGE_REG_LBA;
    static constexpr uint32_t kBlockCountOffset = STORAGE_REG_BLOCK_COUNT;
    static constexpr uint32_t kErrorOffset = STORAGE_REG_ERROR;
    static constexpr uint32_t kDataWindowOffset = STORAGE_DATA_WINDOW_OFFSET;
    static constexpr uint32_t kDataWindowSize = STORAGE_DATA_WINDOW_SIZE;

    std::vector<uint8_t> data_{};
    std::array<uint8_t, kDataWindowSize> data_window_{};
    uint64_t capacity_blocks_{0};
    uint64_t status_{STORAGE_STATUS_READY};
    uint64_t lba_{0};
    uint64_t block_count_{1};
    uint64_t error_code_{STORAGE_ERR_NONE};
    bool attached_{false};
};

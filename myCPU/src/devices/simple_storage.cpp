#include "simple_storage.h"

#include <cstddef>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

bool is_valid_data_window_access(uint32_t offset, int size) {
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        return false;
    }

    return offset >= STORAGE_DATA_WINDOW_OFFSET
        && offset + static_cast<uint32_t>(size) <= STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE;
}

bool is_valid_register_load(uint32_t offset, int size) {
    if (size != 8) {
        return false;
    }

    switch (offset) {
    case STORAGE_REG_MAGIC:
    case STORAGE_REG_VERSION:
    case STORAGE_REG_BLOCK_SIZE:
    case STORAGE_REG_CAPACITY_BLOCKS:
    case STORAGE_REG_STATUS:
    case STORAGE_REG_LBA:
    case STORAGE_REG_BLOCK_COUNT:
    case STORAGE_REG_ERROR:
        return true;
    default:
        return false;
    }
}

bool is_valid_register_store(uint32_t offset, int size) {
    if (size != 8) {
        return false;
    }

    switch (offset) {
    case STORAGE_REG_LBA:
    case STORAGE_REG_BLOCK_COUNT:
    case STORAGE_REG_COMMAND:
        return true;
    default:
        return false;
    }
}

}  // namespace

SimpleStorage::SimpleStorage() : Device(STORAGE_BASE, STORAGE_SIZE) {}

uint64_t SimpleStorage::load(uint64_t addr, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - STORAGE_BASE);
    if (is_valid_data_window_access(offset, size)) {
        return load_data_window(offset, size);
    }
    if (!is_valid_register_load(offset, size)) {
        invalid_access(addr, size);
    }

    return register_value(offset);
}

void SimpleStorage::store(uint64_t addr, uint64_t value, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - STORAGE_BASE);
    if (is_valid_data_window_access(offset, size)) {
        store_data_window(offset, value, size);
        return;
    }
    if (!is_valid_register_store(offset, size)) {
        invalid_access(addr, size);
    }

    if (offset == kLbaOffset) {
        lba_ = value;
        return;
    }
    if (offset == kBlockCountOffset) {
        block_count_ = value;
        return;
    }
    if (offset == kCommandOffset) {
        execute_command(value);
        return;
    }
}

void SimpleStorage::load_image(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("failed to open storage image: ") + path);
    }

    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error(std::string("failed to size storage image: ") + path);
    }

    const uint64_t byte_count = static_cast<uint64_t>(size);
    const uint64_t padded_size =
        ((byte_count + kBlockSize - 1) / kBlockSize) * kBlockSize;
    data_.assign(static_cast<size_t>(padded_size), 0);
    file.seekg(0, std::ios::beg);
    if (size > 0 && !file.read(reinterpret_cast<char*>(data_.data()), size)) {
        throw std::runtime_error(std::string("short read while loading storage image: ") + path);
    }

    data_window_.fill(0);
    capacity_blocks_ = padded_size / kBlockSize;
    lba_ = 0;
    block_count_ = 1;
    attached_ = true;
    ready_ = true;
    error_code_ = STORAGE_ERR_NONE;
    update_status();
}

void SimpleStorage::set_ready(bool ready) {
    ready_ = attached_ && ready;
    update_status();
}

void SimpleStorage::set_magic_valid(bool valid) {
    magic_valid_ = valid;
}

void SimpleStorage::clear_image() {
    data_.clear();
    data_window_.fill(0);
    capacity_blocks_ = 0;
    status_ = 0;
    lba_ = 0;
    block_count_ = 1;
    error_code_ = STORAGE_ERR_NONE;
    attached_ = false;
    ready_ = false;
    magic_valid_ = true;
    update_status();
}

bool SimpleStorage::attached() const {
    return attached_;
}

uint64_t SimpleStorage::status() const {
    return status_;
}

uint64_t SimpleStorage::capacity_blocks() const {
    return capacity_blocks_;
}

uint64_t SimpleStorage::lba() const {
    return lba_;
}

uint64_t SimpleStorage::block_count() const {
    return block_count_;
}

uint64_t SimpleStorage::error_code() const {
    return error_code_;
}

uint64_t SimpleStorage::register_value(uint32_t offset) const {
    switch (offset) {
    case kMagicOffset:
        return magic_valid_ ? kMagic : 0;
    case kVersionOffset:
        return kVersion;
    case kBlockSizeOffset:
        return kBlockSize;
    case kCapacityBlocksOffset:
        return capacity_blocks_;
    case kStatusOffset:
        return status_;
    case kLbaOffset:
        return lba_;
    case kBlockCountOffset:
        return block_count_;
    case kErrorOffset:
        return error_code_;
    default:
        return 0;
    }
}

uint64_t SimpleStorage::load_data_window(uint32_t offset, int size) const {
    const uint32_t window_offset = offset - kDataWindowOffset;
    uint64_t value = 0;
    for (int i = 0; i < size; ++i) {
        const uint64_t byte = data_window_[static_cast<size_t>(window_offset + static_cast<uint32_t>(i))];
        value |= byte << (8 * i);
    }
    return value;
}

void SimpleStorage::store_data_window(uint32_t offset, uint64_t value, int size) {
    const uint32_t window_offset = offset - kDataWindowOffset;
    for (int i = 0; i < size; ++i) {
        data_window_[static_cast<size_t>(window_offset + static_cast<uint32_t>(i))] =
            static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

void SimpleStorage::execute_command(uint64_t command) {
    if (command == STORAGE_CMD_NONE) {
        clear_error();
        return;
    }

    clear_error();
    if (!attached_) {
        set_error(STORAGE_ERR_NO_MEDIA);
        return;
    }
    if (!ready_) {
        set_error(STORAGE_ERR_NOT_READY);
        return;
    }
    if (block_count_ != STORAGE_MAX_BLOCK_COUNT) {
        set_error(STORAGE_ERR_BAD_BLOCK_COUNT);
        return;
    }
    if (lba_ >= capacity_blocks_) {
        set_error(STORAGE_ERR_LBA_RANGE);
        return;
    }

    const uint64_t byte_offset = lba_ * kBlockSize;
    const auto begin = data_.begin() + static_cast<std::ptrdiff_t>(byte_offset);
    const auto end = begin + static_cast<std::ptrdiff_t>(kBlockSize);

    if (command == STORAGE_CMD_READ) {
        std::copy(begin, end, data_window_.begin());
        return;
    }
    if (command == STORAGE_CMD_WRITE) {
        std::copy(data_window_.begin(), data_window_.end(), begin);
        return;
    }

    set_error(STORAGE_ERR_BAD_COMMAND);
}

void SimpleStorage::update_status() {
    status_ = 0;
    if (attached_) {
        status_ |= STORAGE_STATUS_ATTACHED;
    }
    if (attached_ && ready_) {
        status_ |= STORAGE_STATUS_READY;
    }
    if (error_code_ != STORAGE_ERR_NONE) {
        status_ |= STORAGE_STATUS_ERROR;
    }
}

void SimpleStorage::set_error(uint64_t error_code) {
    error_code_ = error_code;
    update_status();
}

void SimpleStorage::clear_error() {
    error_code_ = STORAGE_ERR_NONE;
    update_status();
}

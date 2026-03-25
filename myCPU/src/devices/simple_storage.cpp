#include "simple_storage.h"

#include <cstddef>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

uint64_t mask_for_size(int size) {
    switch (size) {
    case 1:
        return 0xFFULL;
    case 2:
        return 0xFFFFULL;
    case 4:
        return 0xFFFFFFFFULL;
    case 8:
        return ~0ULL;
    default:
        return 0;
    }
}

}  // namespace

SimpleStorage::SimpleStorage() : Device(STORAGE_BASE, STORAGE_SIZE) {}

uint64_t SimpleStorage::load(uint64_t addr, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - STORAGE_BASE);
    if (offset >= kDataWindowOffset && offset + static_cast<uint32_t>(size) <= kDataWindowOffset + kDataWindowSize) {
        return load_data_window(offset, size);
    }

    return register_value(offset) & mask_for_size(size);
}

void SimpleStorage::store(uint64_t addr, uint64_t value, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - STORAGE_BASE);
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
    if (offset >= kDataWindowOffset && offset + static_cast<uint32_t>(size) <= kDataWindowOffset + kDataWindowSize) {
        store_data_window(offset, value, size);
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
    status_ = STORAGE_STATUS_READY | STORAGE_STATUS_ATTACHED;
    error_code_ = STORAGE_ERR_NONE;
    attached_ = true;
}

const char* SimpleStorage::debug_name() const {
    return "storage";
}

void SimpleStorage::reset() {
    data_.clear();
    data_window_.fill(0);
    capacity_blocks_ = 0;
    status_ = STORAGE_STATUS_READY;
    lba_ = 0;
    block_count_ = 1;
    error_code_ = STORAGE_ERR_NONE;
    attached_ = false;
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
        return kMagic;
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

void SimpleStorage::set_error(uint64_t error_code) {
    status_ |= STORAGE_STATUS_ERROR;
    error_code_ = error_code;
}

void SimpleStorage::clear_error() {
    status_ &= ~STORAGE_STATUS_ERROR;
    error_code_ = STORAGE_ERR_NONE;
}

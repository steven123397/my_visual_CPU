#include "simple_storage.h"

#include <fstream>
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
    if (offset == kDataOffset) {
        return load_data_port(size);
    }

    return register_value(offset) & mask_for_size(size);
}

void SimpleStorage::store(uint64_t addr, uint64_t value, int size) {
    const uint32_t offset = static_cast<uint32_t>(addr - STORAGE_BASE);
    if (offset == kCursorOffset) {
        cursor_ = value;
        return;
    }
    if (offset == kDataOffset) {
        store_data_port(value, size);
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

    data_.assign(static_cast<size_t>(size), 0);
    file.seekg(0, std::ios::beg);
    if (size > 0 && !file.read(reinterpret_cast<char*>(data_.data()), size)) {
        throw std::runtime_error(std::string("short read while loading storage image: ") + path);
    }

    cursor_ = 0;
    attached_ = true;
}

uint64_t SimpleStorage::register_value(uint32_t offset) const {
    switch (offset) {
    case kMagicOffset:
        return kMagic;
    case kCapacityOffset:
        return static_cast<uint64_t>(data_.size());
    case kBlockSizeOffset:
        return kBlockSize;
    case kCursorOffset:
        return cursor_;
    case kStatusOffset:
        return attached_ ? 1ULL : 0ULL;
    default:
        return 0;
    }
}

uint64_t SimpleStorage::load_data_port(int size) {
    uint64_t value = 0;
    for (int i = 0; i < size; ++i) {
        const uint64_t index = cursor_ + static_cast<uint64_t>(i);
        const uint64_t byte = index < data_.size() ? data_[static_cast<size_t>(index)] : 0;
        value |= byte << (8 * i);
    }
    cursor_ += static_cast<uint64_t>(size);
    return value;
}

void SimpleStorage::store_data_port(uint64_t value, int size) {
    for (int i = 0; i < size; ++i) {
        const uint64_t index = cursor_ + static_cast<uint64_t>(i);
        if (index < data_.size()) {
            data_[static_cast<size_t>(index)] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        }
    }
    cursor_ += static_cast<uint64_t>(size);
}

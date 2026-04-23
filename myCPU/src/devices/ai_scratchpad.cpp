#include "ai_scratchpad.h"

#include <algorithm>
#include <cstring>

namespace {

bool range_fits(uint32_t offset, size_t bytes, uint32_t limit) {
    if (bytes == 0) {
        return true;
    }
    return offset <= limit && bytes <= (static_cast<size_t>(limit) - offset);
}

}  // namespace

void AiScratchpad::configure(uint32_t scratchpad_bytes,
                             uint32_t accumulator_bytes,
                             uint32_t temporary_bytes) {
    layout_.scratchpad_base = 0;
    layout_.scratchpad_bytes = scratchpad_bytes;
    layout_.accumulator_base = layout_.scratchpad_base + layout_.scratchpad_bytes;
    layout_.accumulator_bytes = accumulator_bytes;
    layout_.temporary_base = layout_.accumulator_base + layout_.accumulator_bytes;
    layout_.temporary_bytes = temporary_bytes;
    layout_.total_bytes = layout_.temporary_base + layout_.temporary_bytes;
    bytes_.assign(layout_.total_bytes, 0);
}

void AiScratchpad::reset() {
    std::fill(bytes_.begin(), bytes_.end(), 0);
}

const AiScratchpadLayout& AiScratchpad::layout() const {
    return layout_;
}

bool AiScratchpad::contains(AiScratchpadSpace space, uint32_t offset, size_t bytes) const {
    return range_fits(offset, bytes, space_bytes(space));
}

bool AiScratchpad::read(AiScratchpadSpace space, uint32_t offset, void* data, size_t bytes) const {
    if (data == nullptr) {
        return bytes == 0;
    }
    if (!contains(space, offset, bytes)) {
        return false;
    }
    if (bytes == 0) {
        return true;
    }
    std::memcpy(data, bytes_.data() + space_base(space) + offset, bytes);
    return true;
}

bool AiScratchpad::write(AiScratchpadSpace space, uint32_t offset, const void* data, size_t bytes) {
    if (data == nullptr) {
        return bytes == 0;
    }
    if (!contains(space, offset, bytes)) {
        return false;
    }
    if (bytes == 0) {
        return true;
    }
    std::memcpy(bytes_.data() + space_base(space) + offset, data, bytes);
    return true;
}

uint32_t AiScratchpad::space_base(AiScratchpadSpace space) const {
    switch (space) {
    case AiScratchpadSpace::Scratchpad:
        return layout_.scratchpad_base;
    case AiScratchpadSpace::Accumulator:
        return layout_.accumulator_base;
    case AiScratchpadSpace::Temporary:
        return layout_.temporary_base;
    }
    return 0;
}

uint32_t AiScratchpad::space_bytes(AiScratchpadSpace space) const {
    switch (space) {
    case AiScratchpadSpace::Scratchpad:
        return layout_.scratchpad_bytes;
    case AiScratchpadSpace::Accumulator:
        return layout_.accumulator_bytes;
    case AiScratchpadSpace::Temporary:
        return layout_.temporary_bytes;
    }
    return 0;
}

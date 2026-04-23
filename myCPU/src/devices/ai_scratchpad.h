#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class AiScratchpadSpace : uint8_t {
    Scratchpad,
    Accumulator,
    Temporary,
};

struct AiScratchpadLayout {
    uint32_t scratchpad_base{0};
    uint32_t scratchpad_bytes{0};
    uint32_t accumulator_base{0};
    uint32_t accumulator_bytes{0};
    uint32_t temporary_base{0};
    uint32_t temporary_bytes{0};
    uint32_t total_bytes{0};
};

inline constexpr uint32_t kAiScratchpadDefaultAccumulatorBytes = 4096;
inline constexpr uint32_t kAiScratchpadDefaultTemporaryBytes = 4096;

class AiScratchpad {
public:
    void configure(uint32_t scratchpad_bytes,
                   uint32_t accumulator_bytes = kAiScratchpadDefaultAccumulatorBytes,
                   uint32_t temporary_bytes = kAiScratchpadDefaultTemporaryBytes);
    void reset();

    const AiScratchpadLayout& layout() const;
    bool contains(AiScratchpadSpace space, uint32_t offset, size_t bytes) const;
    bool read(AiScratchpadSpace space, uint32_t offset, void* data, size_t bytes) const;
    bool write(AiScratchpadSpace space, uint32_t offset, const void* data, size_t bytes);

private:
    uint32_t space_base(AiScratchpadSpace space) const;
    uint32_t space_bytes(AiScratchpadSpace space) const;

    AiScratchpadLayout layout_{};
    std::vector<uint8_t> bytes_{};
};

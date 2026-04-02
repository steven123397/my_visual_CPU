#pragma once

#include <array>
#include <cstdint>

struct RenameCheckpoint {
    std::array<uint32_t, 32> speculative_map{};
    uint32_t next_phys{32};
};

struct RenameDestResult {
    uint32_t phys{0};
    uint32_t previous_phys{0};
};

class RenameMap {
public:
    RenameMap();

    RenameCheckpoint checkpoint() const;
    RenameCheckpoint committed_checkpoint() const;
    uint32_t map_source(uint8_t arch) const;
    uint32_t architectural_source(uint8_t arch) const;
    RenameDestResult rename_dest(uint8_t arch);
    void commit_dest(uint8_t arch, uint32_t phys);
    void rollback(const RenameCheckpoint& checkpoint);

private:
    std::array<uint32_t, 32> architectural_map_{};
    std::array<uint32_t, 32> speculative_map_{};
    uint32_t next_phys_{32};
};

#pragma once

#include <array>
#include <cstdint>

struct RenameCheckpoint {
    std::array<uint16_t, 32> speculative_map{};
    uint16_t next_phys{32};
};

struct RenameDestResult {
    uint16_t phys{0};
    uint16_t previous_phys{0};
};

class RenameMap {
public:
    RenameMap();

    RenameCheckpoint checkpoint() const;
    uint16_t map_source(uint8_t arch) const;
    uint16_t architectural_source(uint8_t arch) const;
    RenameDestResult rename_dest(uint8_t arch);
    void commit_dest(uint8_t arch, uint16_t phys);
    void rollback(const RenameCheckpoint& checkpoint);

private:
    std::array<uint16_t, 32> architectural_map_{};
    std::array<uint16_t, 32> speculative_map_{};
    uint16_t next_phys_{32};
};

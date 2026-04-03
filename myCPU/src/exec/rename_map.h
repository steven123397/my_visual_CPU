#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct RenameCheckpoint {
    std::array<uint32_t, 32> speculative_map{};
    std::vector<uint32_t> free_list{};
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
    uint32_t commit_dest(uint8_t arch, uint32_t phys);
    void rollback(const RenameCheckpoint& checkpoint);

private:
    static void recycle_phys(uint32_t phys, std::vector<uint32_t>& free_list);
    static void remove_from_free_list(uint32_t phys, std::vector<uint32_t>& free_list);
    static uint32_t committed_next_phys(const std::array<uint32_t, 32>& architectural_map,
                                        const std::vector<uint32_t>& free_list);

    std::array<uint32_t, 32> architectural_map_{};
    std::array<uint32_t, 32> speculative_map_{};
    std::vector<uint32_t> committed_free_list_{};
    std::vector<uint32_t> speculative_free_list_{};
    uint32_t next_phys_{32};
};

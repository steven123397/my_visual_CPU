#include "rename_map.h"

#include <algorithm>

namespace {

std::array<uint32_t, 32> make_identity_map() {
    std::array<uint32_t, 32> map{};
    for (uint32_t index = 0; index < map.size(); ++index) {
        map[index] = index;
    }
    return map;
}

}  // namespace

RenameMap::RenameMap()
    : architectural_map_(make_identity_map()),
      speculative_map_(architectural_map_) {}

RenameCheckpoint RenameMap::checkpoint() const {
    return RenameCheckpoint{
        .speculative_map = speculative_map_,
        .free_list = speculative_free_list_,
        .next_phys = next_phys_,
    };
}

RenameCheckpoint RenameMap::committed_checkpoint() const {
    return RenameCheckpoint{
        .speculative_map = architectural_map_,
        .free_list = committed_free_list_,
        .next_phys = committed_next_phys(architectural_map_, committed_free_list_),
    };
}

uint32_t RenameMap::map_source(uint8_t arch) const {
    return speculative_map_[arch];
}

uint32_t RenameMap::architectural_source(uint8_t arch) const {
    return architectural_map_[arch];
}

RenameDestResult RenameMap::rename_dest(uint8_t arch) {
    if (arch == 0) {
        return {};
    }

    const uint32_t previous_phys = speculative_map_[arch];
    uint32_t phys = 0;
    if (!speculative_free_list_.empty()) {
        phys = speculative_free_list_.back();
        speculative_free_list_.pop_back();
    } else {
        phys = next_phys_++;
    }
    speculative_map_[arch] = phys;
    return RenameDestResult{
        .phys = phys,
        .previous_phys = previous_phys,
    };
}

uint32_t RenameMap::commit_dest(uint8_t arch, uint32_t phys) {
    if (arch == 0) {
        return 0;
    }

    const uint32_t previous_arch_phys = architectural_map_[arch];
    remove_from_free_list(phys, committed_free_list_);
    remove_from_free_list(phys, speculative_free_list_);
    architectural_map_[arch] = phys;

    if (previous_arch_phys != 0 && previous_arch_phys != phys) {
        recycle_phys(previous_arch_phys, committed_free_list_);
        recycle_phys(previous_arch_phys, speculative_free_list_);
        return previous_arch_phys;
    }
    return 0;
}

void RenameMap::rollback(const RenameCheckpoint& checkpoint) {
    speculative_map_ = checkpoint.speculative_map;
    speculative_free_list_ = checkpoint.free_list;
    next_phys_ = checkpoint.next_phys;
}

void RenameMap::recycle_phys(uint32_t phys, std::vector<uint32_t>& free_list) {
    if (phys == 0) {
        return;
    }
    if (std::find(free_list.begin(), free_list.end(), phys) != free_list.end()) {
        return;
    }
    free_list.push_back(phys);
}

void RenameMap::remove_from_free_list(uint32_t phys, std::vector<uint32_t>& free_list) {
    free_list.erase(std::remove(free_list.begin(), free_list.end(), phys), free_list.end());
}

uint32_t RenameMap::committed_next_phys(const std::array<uint32_t, 32>& architectural_map,
                                        const std::vector<uint32_t>& free_list) {
    uint32_t next_phys = 32;
    for (uint32_t phys : architectural_map) {
        if (phys >= next_phys) {
            next_phys = phys + 1;
        }
    }
    for (uint32_t phys : free_list) {
        if (phys >= next_phys) {
            next_phys = phys + 1;
        }
    }
    return next_phys;
}

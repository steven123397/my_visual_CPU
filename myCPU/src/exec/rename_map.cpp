#include "rename_map.h"

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
        .next_phys = next_phys_,
    };
}

RenameCheckpoint RenameMap::committed_checkpoint() const {
    uint32_t next_phys = 32;
    for (uint32_t phys : architectural_map_) {
        if (phys >= next_phys) {
            next_phys = phys + 1;
        }
    }
    return RenameCheckpoint{
        .speculative_map = architectural_map_,
        .next_phys = next_phys,
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
    const uint32_t phys = next_phys_++;
    speculative_map_[arch] = phys;
    return RenameDestResult{
        .phys = phys,
        .previous_phys = previous_phys,
    };
}

void RenameMap::commit_dest(uint8_t arch, uint32_t phys) {
    if (arch == 0) {
        return;
    }
    architectural_map_[arch] = phys;
}

void RenameMap::rollback(const RenameCheckpoint& checkpoint) {
    speculative_map_ = checkpoint.speculative_map;
    next_phys_ = checkpoint.next_phys;
}

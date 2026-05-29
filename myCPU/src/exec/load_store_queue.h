#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "../mem/memory_region.h"

enum class LsqEntryKind : uint8_t {
    Load,
    Store,
};

enum class LsqLoadState : uint8_t {
    None,
    BlockedByUnresolvedStore,
    BlockedByOverlappingStore,
    ReplayRequired,
};

struct LsqIndex {
    uint64_t value{0};
};

struct LsqLoadStatus {
    LsqLoadState state{LsqLoadState::None};
    uint64_t load_sequence_id{0};
    uint64_t store_sequence_id{0};

    bool blocks_issue() const {
        return state == LsqLoadState::BlockedByUnresolvedStore ||
               state == LsqLoadState::BlockedByOverlappingStore;
    }

    bool replay_required() const {
        return state == LsqLoadState::ReplayRequired;
    }
};

struct LsqForwardResult {
    uint64_t value{0};
    uint64_t store_sequence_id{0};
};

struct LsqAddressInfo {
    bool translation_fault{false};
    bool crosses_page{false};
    bool paddr_valid{false};
    uint64_t paddr{0};
    bool region_valid{false};
    PhysicalRegionInfo region{};
};

struct LsqLoadRequest {
    uint64_t sequence_id{0};
    uint8_t rd{0};
    int size{0};
    bool sign_extend{false};
    bool mmio{false};
    bool non_speculative{false};
};

struct LsqStoreRequest {
    uint64_t sequence_id{0};
    int size{0};
    bool mmio{false};
    bool non_speculative{false};
};

struct LsqEntry {
    LsqIndex index{};
    LsqEntryKind kind{LsqEntryKind::Load};
    uint64_t sequence_id{0};
    uint8_t rd{0};
    int size{0};
    bool order_ready{false};
    bool address_ready{false};
    uint64_t address{0};
    LsqAddressInfo address_info{};
    bool data_ready{false};
    uint64_t data{0};
    bool sign_extend{false};
    bool mmio{false};
    bool non_speculative{false};
    LsqLoadState load_state{LsqLoadState::None};
    uint64_t violating_store_sequence_id{0};
};

class Bus;

class LoadStoreQueue {
public:
    LsqIndex enqueue_load(const LsqLoadRequest& req);
    LsqIndex enqueue_store(const LsqStoreRequest& req);
    void mark_order_ready(LsqIndex index);
    void mark_address_ready(LsqIndex index, uint64_t addr);
    void mark_address_ready(LsqIndex index, uint64_t addr, const LsqAddressInfo& info);
    void mark_data_ready(LsqIndex index, uint64_t value);
    std::optional<LsqEntry> peek(LsqIndex index) const;
    std::optional<LsqEntry> peek_oldest() const;
    LsqLoadStatus classify_load(uint64_t sequence_id, uint64_t load_addr, int load_size) const;
    LsqLoadStatus classify_load(uint64_t sequence_id,
                                uint64_t load_addr,
                                int load_size,
                                const LsqAddressInfo& load_info) const;
    std::optional<LsqForwardResult> forwardable_load(const Bus& bus,
                                                     uint64_t sequence_id,
                                                     uint64_t load_addr,
                                                     int load_size) const;
    std::optional<LsqForwardResult> forwardable_load(uint64_t sequence_id,
                                                     uint64_t load_addr,
                                                     int load_size,
                                                     const LsqAddressInfo& load_info) const;
    LsqLoadStatus active_replay() const;
    LsqLoadStatus oldest_load_status() const;
    bool has_blocking_older_store(uint64_t sequence_id, uint64_t load_addr, int load_size) const;
    std::optional<LsqEntry> retire_entry(LsqIndex index);
    void clear();
    void flush_younger_than(uint64_t sequence_id);
    size_t size() const;

private:
    std::vector<LsqEntry> entries_{};
    uint64_t next_index_{1};
};

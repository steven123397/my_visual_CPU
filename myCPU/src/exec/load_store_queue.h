#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

enum class LsqEntryKind : uint8_t {
    Load,
    Store,
};

struct LsqIndex {
    uint64_t value{0};
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
    bool address_ready{false};
    uint64_t address{0};
    bool data_ready{false};
    uint64_t data{0};
    bool sign_extend{false};
    bool mmio{false};
    bool non_speculative{false};
};

class LoadStoreQueue {
public:
    LsqIndex enqueue_load(const LsqLoadRequest& req);
    LsqIndex enqueue_store(const LsqStoreRequest& req);
    void mark_address_ready(LsqIndex index, uint64_t addr);
    void mark_data_ready(LsqIndex index, uint64_t value);
    std::optional<LsqEntry> peek(LsqIndex index) const;
    std::optional<LsqEntry> retire_store(LsqIndex index);
    void flush_younger_than(uint64_t sequence_id);
    size_t size() const;

private:
    std::vector<LsqEntry> entries_{};
    uint64_t next_index_{1};
};

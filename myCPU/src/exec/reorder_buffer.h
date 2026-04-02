#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

struct RobIndex {
    uint64_t value{0};
};

struct RobAllocate {
    uint64_t sequence_id{0};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t arch_rd{0};
    uint16_t phys_rd{0};
    uint16_t previous_phys_rd{0};
};

struct RobReady {
    bool value_ready{false};
    uint64_t value{0};
    bool has_fault{false};
    uint64_t cause{0};
    uint64_t tval{0};
    bool redirect{false};
    uint64_t redirect_target{0};
};

struct RobEntry {
    RobIndex index{};
    uint64_t sequence_id{0};
    uint64_t pc{0};
    uint32_t raw{0};
    uint8_t arch_rd{0};
    uint16_t phys_rd{0};
    uint16_t previous_phys_rd{0};
    bool ready{false};
    bool value_ready{false};
    uint64_t value{0};
    bool has_fault{false};
    uint64_t cause{0};
    uint64_t tval{0};
    bool redirect{false};
    uint64_t redirect_target{0};
};

class ReorderBuffer {
public:
    RobIndex allocate(const RobAllocate& entry);
    void mark_ready(RobIndex index, const RobReady& ready);
    std::optional<RobEntry> peek_head() const;
    void commit_head();
    void flush_younger_than(uint64_t sequence_id);
    size_t size() const;

private:
    std::deque<RobEntry> entries_{};
    uint64_t next_index_{1};
};

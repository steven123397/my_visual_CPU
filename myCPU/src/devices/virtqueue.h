#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Bus;

constexpr uint16_t VIRTQ_DESC_F_NEXT = 0x1;
constexpr uint16_t VIRTQ_DESC_F_WRITE = 0x2;
constexpr uint16_t VIRTQ_DESC_F_INDIRECT = 0x4;

class VirtQueue {
public:
    struct Descriptor {
        uint64_t addr{0};
        uint32_t len{0};
        uint16_t flags{0};
        uint16_t next{0};
    };

    struct Chain {
        uint16_t head_index{0};
        uint16_t avail_index{0};
        std::vector<Descriptor> descriptors{};
    };

    explicit VirtQueue(uint16_t max_size);

    void reset();

    uint16_t max_size() const;
    uint16_t size() const;
    bool set_size(uint16_t size);

    uint64_t desc_addr() const;
    uint64_t avail_addr() const;
    uint64_t used_addr() const;
    void set_desc_addr(uint64_t addr);
    void set_avail_addr(uint64_t addr);
    void set_used_addr(uint64_t addr);

    bool ready() const;
    void set_ready(bool ready);
    bool configured() const;

    bool has_pending(Bus& bus, std::string& error) const;
    bool pop_chain(Bus& bus, Chain& chain, std::string& error);
    bool commit_chain(const Chain& chain, std::string& error);
    bool push_used(Bus& bus, uint16_t head_index, uint32_t len, std::string& error);

private:
    bool load_u16(Bus& bus, uint64_t addr, uint16_t& value, std::string& error) const;
    bool load_descriptor(Bus& bus, uint16_t index, Descriptor& descriptor, std::string& error) const;
    bool store_u16(Bus& bus, uint64_t addr, uint16_t value, std::string& error) const;
    bool store_u32(Bus& bus, uint64_t addr, uint32_t value, std::string& error) const;

    uint16_t max_size_{0};
    uint16_t size_{0};
    uint64_t desc_addr_{0};
    uint64_t avail_addr_{0};
    uint64_t used_addr_{0};
    bool ready_{false};
    uint16_t next_avail_idx_{0};
};

#pragma once

#include <cstdint>
#include <vector>

struct PhysicalRegisterCheckpoint {
    std::vector<uint64_t> values{};
    std::vector<uint8_t> ready{};
};

class PhysicalRegisterFile {
public:
    PhysicalRegisterFile();

    uint64_t read(uint16_t phys) const;
    bool is_ready(uint16_t phys) const;
    void set_pending(uint16_t phys);
    void write(uint16_t phys, uint64_t value);
    PhysicalRegisterCheckpoint checkpoint() const;
    void rollback(const PhysicalRegisterCheckpoint& checkpoint);
    void reset();

private:
    struct Entry {
        uint64_t value{0};
        bool ready{true};
    };

    void ensure_index(uint16_t phys);

    std::vector<Entry> entries_{};
};

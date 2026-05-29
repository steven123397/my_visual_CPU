#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class Bus;

struct SimpleL1DataCacheConfig {
    bool enabled{false};
    uint64_t line_size_bytes{64};
    size_t capacity_lines{64};
};

struct SimpleL1DataCacheStats {
    uint64_t loads{0};
    uint64_t stores{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    uint64_t bypasses{0};
    uint64_t write_through_stores{0};
};

class SimpleL1DataCache {
public:
    explicit SimpleL1DataCache(SimpleL1DataCacheConfig config = {});

    bool load(Bus& bus,
              uint64_t addr,
              int size,
              uint64_t& value,
              const char* source = "guest-data",
              const char* kind = "data-load");
    bool store(Bus& bus,
               uint64_t addr,
               uint64_t value,
               int size,
               const char* source = "guest-data",
               const char* kind = "data-store");

    void clear();
    void invalidate_range(uint64_t addr, uint64_t size);
    void set_enabled(bool enabled);
    bool enabled() const;
    uint64_t line_size_bytes() const;
    size_t capacity_lines() const;
    const SimpleL1DataCacheStats& stats() const;

private:
    struct Line {
        bool valid{false};
        uint64_t base{0};
        uint64_t last_used{0};
        std::vector<uint8_t> bytes{};
    };

    bool should_bypass(const Bus& bus, uint64_t addr, int size) const;
    bool refill_line(Bus& bus, uint64_t line_base, Line*& line);
    Line* find_line(uint64_t line_base);
    Line& select_victim();
    uint64_t line_base(uint64_t addr) const;
    uint64_t line_offset(uint64_t addr) const;
    bool access_fits_one_line(uint64_t addr, int size) const;
    uint64_t read_line_value(const Line& line, uint64_t offset, int size) const;
    void write_line_value(Line& line, uint64_t offset, uint64_t value, int size);
    void touch(Line& line);

    SimpleL1DataCacheConfig config_{};
    SimpleL1DataCacheStats stats_{};
    std::vector<Line> lines_{};
    uint64_t use_clock_{0};
};

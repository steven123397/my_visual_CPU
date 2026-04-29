#include "simple_l1_cache.h"

#include <algorithm>

#include "bus.h"

namespace {

bool valid_size(int size) {
    return size == 1 || size == 2 || size == 4 || size == 8;
}

}  // namespace

SimpleL1DataCache::SimpleL1DataCache(SimpleL1DataCacheConfig config) : config_(config) {
    if (config_.line_size_bytes == 0) {
        config_.line_size_bytes = 64;
    }
    lines_.resize(config_.capacity_lines);
    for (Line& line : lines_) {
        line.bytes.resize(static_cast<size_t>(config_.line_size_bytes), 0);
    }
}

bool SimpleL1DataCache::load(Bus& bus, uint64_t addr, int size, uint64_t& value) {
    ++stats_.loads;
    if (should_bypass(bus, addr, size)) {
        ++stats_.bypasses;
        return bus.try_load(addr, size, value);
    }

    const uint64_t base = line_base(addr);
    Line* line = find_line(base);
    if (line != nullptr) {
        ++stats_.hits;
        touch(*line);
        value = read_line_value(*line, line_offset(addr), size);
        return true;
    }

    ++stats_.misses;
    if (!refill_line(bus, base, line)) {
        value = 0;
        return false;
    }

    value = read_line_value(*line, line_offset(addr), size);
    return true;
}

bool SimpleL1DataCache::store(Bus& bus, uint64_t addr, uint64_t value, int size) {
    ++stats_.stores;
    if (should_bypass(bus, addr, size)) {
        ++stats_.bypasses;
        const bool stored = bus.try_store(addr, value, size);
        if (stored) {
            invalidate_range(addr, static_cast<uint64_t>(size));
        }
        return stored;
    }

    Line* line = find_line(line_base(addr));
    if (line != nullptr) {
        ++stats_.hits;
    } else {
        ++stats_.misses;
    }

    if (!bus.try_store(addr, value, size)) {
        return false;
    }
    ++stats_.write_through_stores;

    if (line != nullptr) {
        touch(*line);
        write_line_value(*line, line_offset(addr), value, size);
    }
    return true;
}

void SimpleL1DataCache::clear() {
    stats_ = {};
    use_clock_ = 0;
    for (Line& line : lines_) {
        line.valid = false;
        line.base = 0;
        line.last_used = 0;
        std::fill(line.bytes.begin(), line.bytes.end(), 0);
    }
}

void SimpleL1DataCache::invalidate_range(uint64_t addr, uint64_t size) {
    if (size == 0) {
        return;
    }

    const uint64_t access_begin = addr;
    const uint64_t access_end = size > UINT64_MAX - addr ? UINT64_MAX : addr + size;
    for (Line& line : lines_) {
        if (!line.valid) {
            continue;
        }
        const uint64_t line_begin = line.base;
        const uint64_t line_end = line.base + config_.line_size_bytes;
        if (access_begin < line_end && line_begin < access_end) {
            line.valid = false;
        }
    }
}

void SimpleL1DataCache::set_enabled(bool enabled) {
    config_.enabled = enabled;
}

bool SimpleL1DataCache::enabled() const {
    return config_.enabled;
}

uint64_t SimpleL1DataCache::line_size_bytes() const {
    return config_.line_size_bytes;
}

size_t SimpleL1DataCache::capacity_lines() const {
    return config_.capacity_lines;
}

const SimpleL1DataCacheStats& SimpleL1DataCache::stats() const {
    return stats_;
}

bool SimpleL1DataCache::should_bypass(const Bus& bus, uint64_t addr, int size) const {
    if (!config_.enabled || config_.capacity_lines == 0 || !valid_size(size) ||
        !access_fits_one_line(addr, size)) {
        return true;
    }

    const PhysicalSpanInfo access_span = bus.describe_span(addr, static_cast<uint64_t>(size));
    if (!access_span.ok || access_span.region.kind != PhysicalRegionKind::Ram ||
        !access_span.region.cacheable || access_span.region.has_side_effect) {
        return true;
    }

    const PhysicalSpanInfo line_span = bus.describe_span(line_base(addr), config_.line_size_bytes);
    return !line_span.ok || line_span.region.kind != PhysicalRegionKind::Ram ||
           !line_span.region.cacheable || line_span.region.has_side_effect;
}

bool SimpleL1DataCache::refill_line(Bus& bus, uint64_t base, Line*& line) {
    Line& victim = select_victim();
    victim.valid = false;
    victim.base = base;
    std::fill(victim.bytes.begin(), victim.bytes.end(), 0);

    for (uint64_t i = 0; i < config_.line_size_bytes; ++i) {
        uint64_t byte = 0;
        if (!bus.try_load(base + i, 1, byte)) {
            line = nullptr;
            return false;
        }
        victim.bytes[static_cast<size_t>(i)] = static_cast<uint8_t>(byte & 0xFFU);
    }

    victim.valid = true;
    touch(victim);
    line = &victim;
    return true;
}

SimpleL1DataCache::Line* SimpleL1DataCache::find_line(uint64_t base) {
    for (Line& line : lines_) {
        if (line.valid && line.base == base) {
            return &line;
        }
    }
    return nullptr;
}

SimpleL1DataCache::Line& SimpleL1DataCache::select_victim() {
    for (Line& line : lines_) {
        if (!line.valid) {
            return line;
        }
    }

    auto victim = std::min_element(
        lines_.begin(),
        lines_.end(),
        [](const Line& lhs, const Line& rhs) {
            return lhs.last_used < rhs.last_used;
        });
    ++stats_.evictions;
    return *victim;
}

uint64_t SimpleL1DataCache::line_base(uint64_t addr) const {
    return addr - (addr % config_.line_size_bytes);
}

uint64_t SimpleL1DataCache::line_offset(uint64_t addr) const {
    return addr - line_base(addr);
}

bool SimpleL1DataCache::access_fits_one_line(uint64_t addr, int size) const {
    return line_offset(addr) + static_cast<uint64_t>(size) <= config_.line_size_bytes;
}

uint64_t SimpleL1DataCache::read_line_value(const Line& line, uint64_t offset, int size) const {
    uint64_t value = 0;
    for (int i = 0; i < size; ++i) {
        value |= static_cast<uint64_t>(line.bytes[static_cast<size_t>(offset + i)]) << (i * 8);
    }
    return value;
}

void SimpleL1DataCache::write_line_value(Line& line, uint64_t offset, uint64_t value, int size) {
    for (int i = 0; i < size; ++i) {
        line.bytes[static_cast<size_t>(offset + i)] =
            static_cast<uint8_t>((value >> (i * 8)) & 0xFFU);
    }
}

void SimpleL1DataCache::touch(Line& line) {
    ++use_clock_;
    line.last_used = use_clock_;
}

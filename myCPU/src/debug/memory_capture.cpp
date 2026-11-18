#include "memory_capture.h"

#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace {
std::atomic<uint64_t> next_capture_id{1};
bool intersects(uint64_t address, uint64_t bytes, uint64_t begin, uint64_t end) {
    if (!bytes) return false;
    if (address >= begin && address < end) return true;
    if (address < begin && bytes > begin - address) return true;
    const uint64_t until_wrap = UINT64_MAX - address;
    return bytes - 1 > until_wrap && begin < bytes - until_wrap - 1;
}
}

void MemoryCapture::require_mutable() const {
    if (recording()) throw std::runtime_error("state mutation forbidden while recording");
}

void MemoryCapture::start(const CaptureConfig& config, uint64_t generation) {
    if (metadata_.state != CaptureState::Idle) throw std::runtime_error("clear previous capture before start");
    if (!config.capacity || config.capacity > 65536) throw std::runtime_error("capture capacity must be 1..65536");
    if (config.va_begin.has_value() != config.va_end.has_value() ||
        (config.va_begin && *config.va_begin >= *config.va_end))
        throw std::runtime_error("invalid capture VA interval");
    std::vector<MemoryRecord> allocated;
    allocated.reserve(config.capacity);
    records_.swap(allocated);
    metadata_ = {};
    metadata_.state = CaptureState::Recording;
    metadata_.capture_id = next_capture_id.fetch_add(1);
    metadata_.machine_generation = generation;
    metadata_.config = config;
}

void MemoryCapture::stop() {
    if (recording()) metadata_.state = CaptureState::Stopped;
}

void MemoryCapture::clear() {
    require_mutable();
    records_.clear();
    metadata_ = {};
}

void MemoryCapture::unsupported() {
    if (recording()) ++metadata_.unsupported;
}

void MemoryCapture::receive(MemoryRecord r) {
    if (!recording()) return;
    r.capture_id = metadata_.capture_id;
    r.machine_generation = metadata_.machine_generation;
    r.instruction_id = ++metadata_.attempted;
    const auto& cfg = metadata_.config;
    const bool matched = (!cfg.operation || *cfg.operation == r.operation) &&
        (!cfg.effective_privilege || *cfg.effective_privilege == r.effective_privilege) &&
        (!cfg.satp || *cfg.satp == r.satp) &&
        (!cfg.va_begin || intersects(r.vaddr, r.bytes, *cfg.va_begin, *cfg.va_end));
    if (!matched) { ++metadata_.filtered; return; }
    if (records_.size() == cfg.capacity) { ++metadata_.dropped; return; }
    records_.push_back(r);
    ++metadata_.recorded;
    if (!metadata_.first_id) metadata_.first_id = r.instruction_id;
    metadata_.last_id = r.instruction_id;
}

CapturePage MemoryCapture::read(CaptureCursor cursor, size_t limit) const {
    if (metadata_.state == CaptureState::Idle || cursor.capture_id != metadata_.capture_id)
        throw std::runtime_error("stale capture cursor");
    if (!limit || limit > 256) throw std::runtime_error("capture page size must be 1..256");
    const size_t upper = cursor.upper.value_or(records_.size());
    if (upper > records_.size() || cursor.position > upper) throw std::runtime_error("capture cursor out of range");
    const size_t end = cursor.position + std::min(limit, upper - cursor.position);
    CapturePage page;
    page.metadata = metadata_;
    page.next = {cursor.capture_id, end, upper};
    page.records.assign(records_.begin() + cursor.position, records_.begin() + end);
    page.done = end == upper;
    return page;
}

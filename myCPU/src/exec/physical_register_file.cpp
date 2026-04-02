#include "physical_register_file.h"

#include <cstddef>

namespace {

constexpr uint16_t kArchitecturalRegisterCount = 32;

}  // namespace

PhysicalRegisterFile::PhysicalRegisterFile() {
    reset();
}

uint64_t PhysicalRegisterFile::read(uint16_t phys) const {
    if (phys >= entries_.size()) {
        return 0;
    }
    return entries_[phys].value;
}

bool PhysicalRegisterFile::is_ready(uint16_t phys) const {
    if (phys >= entries_.size()) {
        return false;
    }
    return entries_[phys].ready;
}

void PhysicalRegisterFile::set_pending(uint16_t phys) {
    if (phys == 0) {
        return;
    }
    ensure_index(phys);
    entries_[phys].ready = false;
}

void PhysicalRegisterFile::write(uint16_t phys, uint64_t value) {
    if (phys == 0) {
        entries_[0].value = 0;
        entries_[0].ready = true;
        return;
    }
    ensure_index(phys);
    entries_[phys].value = value;
    entries_[phys].ready = true;
}

PhysicalRegisterCheckpoint PhysicalRegisterFile::checkpoint() const {
    PhysicalRegisterCheckpoint checkpoint;
    checkpoint.values.reserve(entries_.size());
    checkpoint.ready.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        checkpoint.values.push_back(entry.value);
        checkpoint.ready.push_back(entry.ready ? 1 : 0);
    }
    return checkpoint;
}

void PhysicalRegisterFile::rollback(const PhysicalRegisterCheckpoint& checkpoint) {
    entries_.clear();
    entries_.reserve(checkpoint.values.size());
    for (std::size_t index = 0; index < checkpoint.values.size(); ++index) {
        entries_.push_back(Entry{
            .value = checkpoint.values[index],
            .ready = checkpoint.ready[index] != 0,
        });
    }
    if (entries_.empty()) {
        reset();
        return;
    }
    entries_[0].value = 0;
    entries_[0].ready = true;
}

void PhysicalRegisterFile::reset() {
    entries_.assign(kArchitecturalRegisterCount, Entry{});
    entries_[0].value = 0;
    entries_[0].ready = true;
}

void PhysicalRegisterFile::ensure_index(uint16_t phys) {
    if (phys < entries_.size()) {
        return;
    }
    entries_.resize(static_cast<std::size_t>(phys) + 1, Entry{
        .value = 0,
        .ready = false,
    });
    entries_[0].value = 0;
    entries_[0].ready = true;
}

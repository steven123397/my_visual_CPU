#include "core_state.h"

void CoreState::reset(uint64_t entry) {
    gpr_.fill(0);
    fpr_.fill(0);
    pc_ = entry;
    cycle_ = 0;
    instret_ = 0;
    vector_.reset();
    halted_ = false;
    privilege_mode_ = PrivilegeMode::Machine;
}

uint64_t CoreState::read_gpr(uint32_t idx) const {
    return gpr_[idx & 0x1F];
}

void CoreState::write_gpr(uint32_t idx, uint64_t value) {
    if ((idx & 0x1F) != 0) {
        gpr_[idx & 0x1F] = value;
    }
}

uint64_t CoreState::read_fpr(uint32_t idx) const {
    return fpr_[idx & 0x1F];
}

void CoreState::write_fpr(uint32_t idx, uint64_t value) {
    fpr_[idx & 0x1F] = value;
}

uint64_t CoreState::pc() const {
    return pc_;
}

void CoreState::set_pc(uint64_t value) {
    pc_ = value;
}

uint64_t CoreState::cycle() const {
    return cycle_;
}

void CoreState::advance_cycle(uint64_t delta) {
    cycle_ += delta;
}

void CoreState::set_cycle(uint64_t value) {
    cycle_ = value;
}

uint64_t CoreState::instret() const {
    return instret_;
}

void CoreState::advance_instret(uint64_t delta) {
    instret_ += delta;
}

void CoreState::set_instret(uint64_t value) {
    instret_ = value;
}

VectorState& CoreState::vector() {
    return vector_;
}

const VectorState& CoreState::vector() const {
    return vector_;
}

bool CoreState::halted() const {
    return halted_;
}

void CoreState::set_halted(bool halted) {
    halted_ = halted;
}

PrivilegeMode CoreState::privilege_mode() const {
    return privilege_mode_;
}

void CoreState::set_privilege_mode(PrivilegeMode mode) {
    privilege_mode_ = mode;
}

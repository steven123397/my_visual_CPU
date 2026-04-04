#include "debug_session.h"
#include "debug_budget.h"

#include <cstdio>
#include <stdexcept>

#include "../arch/csr_file.h"

namespace {

std::string hex_u64(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string recent_tail(const std::string& value, size_t max_len) {
    if (value.size() <= max_len) {
        return value;
    }
    return value.substr(value.size() - max_len);
}

}  // namespace

void DebugSession::load_elf(const std::string& path,
                            BackendKind backend_kind,
                            const char* disk_image,
                            bool disk_ready,
                            bool disk_magic_valid) {
    config_.image_kind = ImageKind::Elf;
    config_.image_path = path;
    config_.binary_addr = MEM_BASE;
    config_.backend_kind = backend_kind;
    config_.disk_attached = disk_image != nullptr && *disk_image != '\0';
    config_.disk_image = config_.disk_attached ? disk_image : "";
    config_.disk_ready = disk_ready;
    config_.disk_magic_valid = disk_magic_valid;
    recreate_machine();
    events_.clear();
}

void DebugSession::load_binary(const std::string& path,
                               uint64_t addr,
                               BackendKind backend_kind,
                               const char* disk_image,
                               bool disk_ready,
                               bool disk_magic_valid) {
    config_.image_kind = ImageKind::Binary;
    config_.image_path = path;
    config_.binary_addr = addr;
    config_.backend_kind = backend_kind;
    config_.disk_attached = disk_image != nullptr && *disk_image != '\0';
    config_.disk_image = config_.disk_attached ? disk_image : "";
    config_.disk_ready = disk_ready;
    config_.disk_magic_valid = disk_magic_valid;
    recreate_machine();
    events_.clear();
}

void DebugSession::reset() {
    ensure_loaded();
    recreate_machine();
    events_.clear();
}

void DebugSession::step_cycle() {
    ensure_loaded();
    const DebugSnapshot before = collect_snapshot();
    machine().step();
    const DebugSnapshot after = collect_snapshot();
    record_step_events(before, after);
}

void DebugSession::step_commit() {
    ensure_loaded();
    const uint64_t instret_before = machine().cpu().core().instret();
    for (uint64_t i = 0; i < DebugBudget::kStepCommitCycleBudget; ++i) {
        step_cycle();
        if (machine().cpu().core().halted() || machine().cpu().core().instret() != instret_before) {
            return;
        }
    }
    throw std::runtime_error("step_commit exceeded cycle budget without reaching a commit boundary");
}

void DebugSession::run_until_uart_contains(std::string_view text,
                                           uint64_t max_steps) {
    ensure_loaded();

    const std::string needle(text);
    if (machine().uart().output().find(needle) != std::string::npos) {
        return;
    }

    for (uint64_t i = 0; i < max_steps; ++i) {
        machine().step();
        if (machine().uart().output().find(needle) != std::string::npos) {
            return;
        }
        if (machine().cpu().core().halted()) {
            throw std::runtime_error("guest halted before requested UART text appeared");
        }
    }

    throw std::runtime_error("run_until_uart_contains exceeded step budget");
}

void DebugSession::run_until_halt(uint64_t max_steps) {
    ensure_loaded();
    if (machine().cpu().core().halted()) {
        return;
    }

    for (uint64_t i = 0; i < max_steps; ++i) {
        machine().step();
        if (machine().cpu().core().halted()) {
            return;
        }
    }

    throw std::runtime_error("run_until_halt exceeded step budget");
}

void DebugSession::uart_input(std::string_view text) {
    ensure_loaded();
    machine().uart().inject_input(text);
}

DebugSession::UartOutputChunk DebugSession::uart_output(size_t offset) const {
    ensure_loaded();

    const std::string& output = machine().uart().output();
    const size_t start = offset < output.size() ? offset : output.size();
    return UartOutputChunk{
        .offset = start,
        .next_offset = output.size(),
        .text = output.substr(start),
    };
}

DebugSnapshot DebugSession::snapshot() const {
    ensure_loaded();
    DebugSnapshot snapshot = collect_snapshot();
    snapshot.events = events_;
    return snapshot;
}

void DebugSession::recreate_machine() {
    machine_ = std::make_unique<Machine>();
    machine_->set_backend_kind(config_.backend_kind);
    if (config_.disk_attached) {
        machine_->attach_storage_image(config_.disk_image, config_.disk_ready, config_.disk_magic_valid);
    }
    machine_->uart().set_mirror_stdout(false);

    switch (config_.image_kind) {
    case ImageKind::Elf:
        machine_->load_elf(config_.image_path);
        break;
    case ImageKind::Binary:
        machine_->load_binary(config_.image_path, config_.binary_addr);
        break;
    case ImageKind::None:
        throw std::runtime_error("debug session image not configured");
    }
}

DebugSnapshot DebugSession::collect_snapshot() const {
    DebugSnapshot snapshot;

    const CPU& cpu = machine().cpu();
    const CoreState& core = cpu.core();
    const BackendDebugSnapshot backend_snapshot = machine().backend().debug_snapshot();
    snapshot.summary.cycle = core.cycle();
    snapshot.summary.instret = core.instret();
    snapshot.summary.pc = core.pc();
    snapshot.summary.halted = core.halted();
    snapshot.summary.privilege = core.privilege_mode();
    snapshot.summary.backend = backend_snapshot.backend_name.empty() ? machine().backend().name() : backend_snapshot.backend_name;
    snapshot.pipeline = backend_snapshot.pipeline;
    for (size_t i = 0; i < snapshot.gpr.size(); ++i) {
        snapshot.gpr[i] = core.read_gpr(static_cast<uint32_t>(i));
    }

    snapshot.csrs.mstatus = cpu.csr().read(CSR_MSTATUS, core);
    snapshot.csrs.sstatus = cpu.csr().read(CSR_SSTATUS, core);
    snapshot.csrs.mepc = cpu.csr().read(CSR_MEPC, core);
    snapshot.csrs.sepc = cpu.csr().read(CSR_SEPC, core);
    snapshot.csrs.mcause = cpu.csr().read(CSR_MCAUSE, core);
    snapshot.csrs.scause = cpu.csr().read(CSR_SCAUSE, core);
    snapshot.csrs.mtval = cpu.csr().read(CSR_MTVAL, core);
    snapshot.csrs.stval = cpu.csr().read(CSR_STVAL, core);
    snapshot.csrs.mie = cpu.csr().read(CSR_MIE, core);
    snapshot.csrs.mip = cpu.csr().read(CSR_MIP, core);
    snapshot.csrs.sie = cpu.csr().read(CSR_SIE, core);
    snapshot.csrs.sip = cpu.csr().read(CSR_SIP, core);
    snapshot.csrs.mtvec = cpu.csr().read(CSR_MTVEC, core);
    snapshot.csrs.stvec = cpu.csr().read(CSR_STVEC, core);
    snapshot.csrs.satp = cpu.csr().read(CSR_SATP, core);

    snapshot.bus = machine().bus().last_access();

    snapshot.devices.uart.ier = machine().uart().ier();
    snapshot.devices.uart.thre_interrupt_asserted = machine().uart().thre_interrupt_asserted();
    snapshot.devices.uart.output_size = machine().uart().output_size();
    snapshot.devices.uart.recent_output = recent_tail(machine().uart().output(), 64);

    snapshot.devices.clint.mtime = machine().clint().mtime();
    snapshot.devices.clint.mtimecmp = machine().clint().mtimecmp();
    snapshot.devices.clint.timer_interrupt_pending = machine().clint().timer_interrupt_pending();

    snapshot.devices.plic.priority = machine().plic().priority(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.level = machine().plic().source_level(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.pending = machine().plic().source_pending(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.claimed = machine().plic().source_claimed(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.machine_enables = machine().plic().machine_enables();
    snapshot.devices.plic.supervisor_enables = machine().plic().supervisor_enables();
    snapshot.devices.plic.machine_threshold = machine().plic().machine_threshold();
    snapshot.devices.plic.supervisor_threshold = machine().plic().supervisor_threshold();
    snapshot.devices.plic.machine_has_pending = machine().plic().machine_has_pending();
    snapshot.devices.plic.supervisor_has_pending = machine().plic().supervisor_has_pending();

    snapshot.devices.storage.attached = machine().storage().attached();
    snapshot.devices.storage.status = machine().storage().status();
    snapshot.devices.storage.capacity_blocks = machine().storage().capacity_blocks();
    snapshot.devices.storage.lba = machine().storage().lba();
    snapshot.devices.storage.block_count = machine().storage().block_count();
    snapshot.devices.storage.error_code = machine().storage().error_code();

    return snapshot;
}

void DebugSession::append_event(const char* kind, const std::string& detail) {
    events_.push_back(DebugEvent{
        .cycle = machine().cpu().core().cycle(),
        .kind = kind,
        .detail = detail,
    });
    constexpr size_t kMaxEvents = 64;
    if (events_.size() > kMaxEvents) {
        events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() - kMaxEvents));
    }
}

void DebugSession::record_step_events(const DebugSnapshot& before, const DebugSnapshot& after) {
    if (after.pipeline.stalled) {
        append_event("stall", "decode stalled on a load-use hazard");
    }
    if (after.pipeline.redirected) {
        append_event("redirect", "redirect to " + hex_u64(after.pipeline.redirect_target));
    }
    if (after.pipeline.pending_fetch_fault) {
        append_event("fetch-fault", "pending fetch fault at " + hex_u64(after.summary.pc));
    }
    if (after.pipeline.trap_flush) {
        append_event("flush", "pipeline flush after trap or trap-return commit");
    }
    if (after.summary.instret != before.summary.instret) {
        append_event("commit", "instret advanced to " + std::to_string(after.summary.instret));
    }
    if (after.bus.valid &&
        (!before.bus.valid || before.bus.addr != after.bus.addr || before.bus.value != after.bus.value ||
         before.bus.write != after.bus.write || before.bus.device != after.bus.device ||
         before.bus.success != after.bus.success || before.bus.detail != after.bus.detail)) {
        std::string detail =
            after.bus.device + " " + (after.bus.write ? "write " : "read ") + hex_u64(after.bus.addr);
        if (!after.bus.success && !after.bus.detail.empty()) {
            detail += " failed: " + after.bus.detail;
        }
        append_event(after.bus.write ? "store" : "load", detail);
    }
    if (after.summary.halted && !before.summary.halted) {
        append_event("halt", "program halted");
    }
    if (after.csrs.mcause != before.csrs.mcause || after.csrs.scause != before.csrs.scause) {
        append_event("trap", "trap state changed");
    }
}

void DebugSession::ensure_loaded() const {
    if (!machine_ || !machine_->loaded()) {
        throw std::runtime_error("debug session image not loaded");
    }
}

Machine& DebugSession::machine() {
    if (!machine_) {
        throw std::runtime_error("debug machine not initialized");
    }
    return *machine_;
}

const Machine& DebugSession::machine() const {
    if (!machine_) {
        throw std::runtime_error("debug machine not initialized");
    }
    return *machine_;
}

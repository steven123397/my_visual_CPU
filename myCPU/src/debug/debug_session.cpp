#include "debug_session.h"

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

void DebugSession::load_elf(const std::string& path, BackendKind backend_kind, const char* disk_image) {
    machine_.set_backend_kind(backend_kind);
    if (disk_image && *disk_image != '\0') {
        machine_.attach_storage_image(disk_image);
    } else {
        machine_.clear_storage_image();
    }
    machine_.uart().set_mirror_stdout(false);
    machine_.load_elf(path);
    events_.clear();
}

void DebugSession::load_binary(const std::string& path, uint64_t addr, BackendKind backend_kind, const char* disk_image) {
    machine_.set_backend_kind(backend_kind);
    if (disk_image && *disk_image != '\0') {
        machine_.attach_storage_image(disk_image);
    } else {
        machine_.clear_storage_image();
    }
    machine_.uart().set_mirror_stdout(false);
    machine_.load_binary(path, addr);
    events_.clear();
}

void DebugSession::reset() {
    ensure_loaded();
    machine_.reset_loaded_image();
    machine_.uart().set_mirror_stdout(false);
    events_.clear();
}

void DebugSession::step_cycle() {
    ensure_loaded();
    const DebugSnapshot before = collect_snapshot();
    machine_.step();
    const DebugSnapshot after = collect_snapshot();
    record_step_events(before, after);
}

void DebugSession::step_commit() {
    ensure_loaded();
    const uint64_t instret_before = machine_.cpu().core().instret();
    for (int i = 0; i < 4096; ++i) {
        step_cycle();
        if (machine_.cpu().core().halted() || machine_.cpu().core().instret() != instret_before) {
            return;
        }
    }
    throw std::runtime_error("step_commit exceeded cycle budget without reaching a commit boundary");
}

DebugSnapshot DebugSession::snapshot() const {
    DebugSnapshot snapshot = collect_snapshot();
    snapshot.events = events_;
    return snapshot;
}

DebugSnapshot DebugSession::collect_snapshot() const {
    DebugSnapshot snapshot;

    const CPU& cpu = machine_.cpu();
    const CoreState& core = cpu.core();
    snapshot.summary.cycle = core.cycle();
    snapshot.summary.instret = core.instret();
    snapshot.summary.pc = core.pc();
    snapshot.summary.halted = core.halted();
    snapshot.summary.privilege = core.privilege_mode();
    snapshot.summary.backend = machine_.backend().name();
    snapshot.pipeline = machine_.backend().debug_snapshot().pipeline;
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

    snapshot.bus = machine_.bus().last_access();

    snapshot.devices.uart.ier = machine_.uart().ier();
    snapshot.devices.uart.thre_interrupt_asserted = machine_.uart().thre_interrupt_asserted();
    snapshot.devices.uart.output_size = machine_.uart().output_size();
    snapshot.devices.uart.recent_output = recent_tail(machine_.uart().output(), 64);

    snapshot.devices.clint.mtime = machine_.clint().mtime();
    snapshot.devices.clint.mtimecmp = machine_.clint().mtimecmp();
    snapshot.devices.clint.timer_interrupt_pending = machine_.clint().timer_interrupt_pending();

    snapshot.devices.plic.priority = machine_.plic().priority(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.level = machine_.plic().source_level(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.pending = machine_.plic().source_pending(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.claimed = machine_.plic().source_claimed(Plic::UART_SOURCE_ID);
    snapshot.devices.plic.machine_enables = machine_.plic().machine_enables();
    snapshot.devices.plic.supervisor_enables = machine_.plic().supervisor_enables();
    snapshot.devices.plic.machine_threshold = machine_.plic().machine_threshold();
    snapshot.devices.plic.supervisor_threshold = machine_.plic().supervisor_threshold();
    snapshot.devices.plic.machine_has_pending = machine_.plic().machine_has_pending();
    snapshot.devices.plic.supervisor_has_pending = machine_.plic().supervisor_has_pending();

    snapshot.devices.storage.attached = machine_.storage().attached();
    snapshot.devices.storage.status = machine_.storage().status();
    snapshot.devices.storage.capacity_blocks = machine_.storage().capacity_blocks();
    snapshot.devices.storage.lba = machine_.storage().lba();
    snapshot.devices.storage.block_count = machine_.storage().block_count();
    snapshot.devices.storage.error_code = machine_.storage().error_code();

    return snapshot;
}

void DebugSession::append_event(const char* kind, const std::string& detail) {
    events_.push_back(DebugEvent{
        .cycle = machine_.cpu().core().cycle(),
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
            before.bus.write != after.bus.write || before.bus.device != after.bus.device)) {
        append_event(
            after.bus.write ? "store" : "load",
            after.bus.device + " " + (after.bus.write ? "write " : "read ") + hex_u64(after.bus.addr));
    }
    if (after.summary.halted && !before.summary.halted) {
        append_event("halt", "program halted");
    }
    if (after.csrs.mcause != before.csrs.mcause || after.csrs.scause != before.csrs.scause) {
        append_event("trap", "trap state changed");
    }
}

void DebugSession::ensure_loaded() const {
    if (!machine_.loaded()) {
        throw std::runtime_error("debug session image not loaded");
    }
}

#include "debug_session.h"
#include "debug_budget.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "../arch/csr_file.h"
#include "../exec/interpreter_dbt_prototype.h"
#include "../mem/memory_region.h"

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

std::string stall_event_detail(const std::string& stall_reason) {
    if (stall_reason.empty() || stall_reason == "none") {
        return "pipeline stalled";
    }
    return "pipeline stalled: " + stall_reason;
}

uint64_t parse_u64_text(std::string_view text) {
    if (text.empty()) {
        throw std::runtime_error("expected unsigned integer");
    }
    if (text.front() == '-') {
        throw std::runtime_error("negative numbers are not supported");
    }

    const std::string copy(text);
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(copy.c_str(), &end, 0);
    if (errno == ERANGE || parsed > std::numeric_limits<uint64_t>::max()) {
        throw std::runtime_error("unsigned integer out of range");
    }
    if (end == copy.c_str() || *end != '\0') {
        throw std::runtime_error("invalid unsigned integer");
    }
    return static_cast<uint64_t>(parsed);
}

uint32_t parse_csr_addr(std::string_view csr_name) {
    if (csr_name == "sstatus") {
        return CSR_SSTATUS;
    }
    if (csr_name == "sie") {
        return CSR_SIE;
    }
    if (csr_name == "stvec") {
        return CSR_STVEC;
    }
    if (csr_name == "sepc") {
        return CSR_SEPC;
    }
    if (csr_name == "scause") {
        return CSR_SCAUSE;
    }
    if (csr_name == "stval") {
        return CSR_STVAL;
    }
    if (csr_name == "sip") {
        return CSR_SIP;
    }
    if (csr_name == "satp") {
        return CSR_SATP;
    }
    if (csr_name == "mstatus") {
        return CSR_MSTATUS;
    }
    if (csr_name == "misa") {
        return CSR_MISA;
    }
    if (csr_name == "medeleg") {
        return CSR_MEDELEG;
    }
    if (csr_name == "mideleg") {
        return CSR_MIDELEG;
    }
    if (csr_name == "mie") {
        return CSR_MIE;
    }
    if (csr_name == "mtvec") {
        return CSR_MTVEC;
    }
    if (csr_name == "mscratch") {
        return CSR_MSCRATCH;
    }
    if (csr_name == "mepc") {
        return CSR_MEPC;
    }
    if (csr_name == "mcause") {
        return CSR_MCAUSE;
    }
    if (csr_name == "mtval") {
        return CSR_MTVAL;
    }
    if (csr_name == "mip") {
        return CSR_MIP;
    }
    if (csr_name == "mhartid") {
        return CSR_MHARTID;
    }
    return static_cast<uint32_t>(parse_u64_text(csr_name) & 0xfffU);
}

bool csr_is_read_only(uint32_t addr) {
    return (addr & 0xc00U) == 0xc00U || addr == CSR_MISA || addr == CSR_MHARTID;
}

const char* region_kind_name(PhysicalRegionKind kind) {
    switch (kind) {
    case PhysicalRegionKind::Ram:
        return "ram";
    case PhysicalRegionKind::Mmio:
        return "mmio";
    case PhysicalRegionKind::Unmapped:
        return "unmapped";
    }
    return "unknown";
}

void validate_memory_size(uint32_t size) {
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        throw std::runtime_error("set_memory size must be 1, 2, 4, or 8 bytes");
    }
}

}  // namespace

void DebugSession::load_elf(const std::string& path,
                            BackendKind backend_kind,
                            BlockTransport block_transport,
                            const char* disk_image,
                            bool disk_ready,
                            bool disk_magic_valid,
                            bool l1d_enabled) {
    config_.image_kind = ImageKind::Elf;
    config_.image_path = path;
    config_.binary_addr = MEM_BASE;
    config_.backend_kind = backend_kind;
    config_.block_transport = block_transport;
    config_.disk_attached = disk_image != nullptr && *disk_image != '\0';
    config_.disk_image = config_.disk_attached ? disk_image : "";
    config_.disk_ready = disk_ready;
    config_.disk_magic_valid = disk_magic_valid;
    config_.l1d_enabled = l1d_enabled;
    config_.post_load_actions.clear();
    recreate_machine();
    events_.clear();
    breakpoints_.clear();
}

void DebugSession::load_binary(const std::string& path,
                               uint64_t addr,
                               BackendKind backend_kind,
                               BlockTransport block_transport,
                               const char* disk_image,
                               bool disk_ready,
                               bool disk_magic_valid,
                               bool l1d_enabled) {
    config_.image_kind = ImageKind::Binary;
    config_.image_path = path;
    config_.binary_addr = addr;
    config_.backend_kind = backend_kind;
    config_.block_transport = block_transport;
    config_.disk_attached = disk_image != nullptr && *disk_image != '\0';
    config_.disk_image = config_.disk_attached ? disk_image : "";
    config_.disk_ready = disk_ready;
    config_.disk_magic_valid = disk_magic_valid;
    config_.l1d_enabled = l1d_enabled;
    config_.post_load_actions.clear();
    recreate_machine();
    events_.clear();
    breakpoints_.clear();
}

void DebugSession::load_binary_payload(const std::string& path, uint64_t addr) {
    ensure_loaded();
    machine().load_binary_payload(path, addr);
    config_.post_load_actions.push_back(PostLoadAction{
        .kind = PostLoadAction::Kind::Payload,
        .text = path,
        .value = addr,
    });
}

void DebugSession::set_gpr(std::string_view reg_name, uint64_t value) {
    ensure_loaded();
    machine().set_gpr(std::string(reg_name), value);
    config_.post_load_actions.push_back(PostLoadAction{
        .kind = PostLoadAction::Kind::SetGpr,
        .text = std::string(reg_name),
        .value = value,
    });
}

void DebugSession::set_memory(uint64_t addr, uint64_t value, uint32_t size, bool virtual_address) {
    ensure_loaded();
    validate_memory_size(size);

    uint64_t physical_addr = addr;
    if (virtual_address) {
        const AddressSpace::TranslateResult translated =
            machine().cpu().address_space().translate_result(
                machine().bus(),
                addr,
                AccessType::Store,
                false);
        if (!translated.ok) {
            throw std::runtime_error("set_memory virtual address translation failed at " + hex_u64(addr));
        }
        physical_addr = translated.paddr;
    }

    const PhysicalSpanInfo span = machine().bus().describe_span(physical_addr, size);
    if (!span.ok) {
        throw std::runtime_error("set_memory target is not fully mapped at " + hex_u64(physical_addr));
    }
    if (span.region.kind != PhysicalRegionKind::Ram || span.region.has_side_effect) {
        throw std::runtime_error(std::string("set_memory supports RAM only; target region is ") +
                                 region_kind_name(span.region.kind) + ":" + span.region.label);
    }

    if (virtual_address) {
        const AddressSpace::AccessResult stored =
            machine().cpu().address_space().store_result(machine().bus(), addr, value, static_cast<int>(size));
        if (!stored.ok) {
            throw std::runtime_error("set_memory virtual store failed at " + hex_u64(addr));
        }
    } else if (!machine().bus().try_store_observed(
                   physical_addr,
                   value,
                   static_cast<int>(size),
                   "debug",
                   "set-memory")) {
        const DebugBusAccess& access = machine().bus().last_access();
        throw std::runtime_error(access.detail.empty() ? "set_memory physical store failed" : access.detail);
    }
}

void DebugSession::set_csr(std::string_view csr_name, uint64_t value) {
    ensure_loaded();
    const uint32_t addr = parse_csr_addr(csr_name);
    if (!machine().cpu().csr().is_implemented(addr)) {
        throw std::runtime_error("set_csr target is not implemented: " + hex_u64(addr));
    }
    if (csr_is_read_only(addr)) {
        throw std::runtime_error("set_csr target is read-only: " + hex_u64(addr));
    }
    machine().cpu().csr().write(addr, value, machine().cpu().core());
}

void DebugSession::break_at(uint64_t addr) {
    ensure_loaded();
    if (!is_breakpoint_pc(addr)) {
        breakpoints_.push_back(addr);
    }
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
    record_breakpoint_hit(after.summary.pc);
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
    const std::string& output = machine().uart().output();
    if (output.find(needle) != std::string::npos) {
        return;
    }
    if (record_current_breakpoint_hit()) {
        return;
    }

    const size_t overlap = needle.empty() ? 0 : needle.size() - 1;
    // Preserve just enough suffix to catch a match that straddles old/new UART bytes.
    size_t search_from = output.size() > overlap ? output.size() - overlap : 0;

    for (uint64_t i = 0; i < max_steps; ++i) {
        machine().step();
        if (record_current_breakpoint_hit()) {
            return;
        }
        const std::string& updated_output = machine().uart().output();
        if (updated_output.find(needle, search_from) != std::string::npos) {
            return;
        }
        if (machine().cpu().core().halted()) {
            throw std::runtime_error("guest halted before requested UART text appeared");
        }
        search_from = updated_output.size() > overlap ? updated_output.size() - overlap : 0;
    }

    throw std::runtime_error("run_until_uart_contains exceeded step budget");
}

DebugSession::UartOutputChunk DebugSession::run_until_new_uart_contains(size_t offset,
                                                                        std::string_view text,
                                                                        uint64_t max_steps) {
    ensure_loaded();

    const std::string needle(text);
    auto make_chunk = [this, offset]() {
        const std::string& output = machine().uart().output();
        const size_t start = offset < output.size() ? offset : output.size();
        return UartOutputChunk{
            .offset = start,
            .next_offset = output.size(),
            .text = output.substr(start),
        };
    };

    UartOutputChunk chunk = make_chunk();
    if (chunk.text.find(needle) != std::string::npos) {
        return chunk;
    }
    if (record_current_breakpoint_hit()) {
        return chunk;
    }

    for (uint64_t i = 0; i < max_steps; ++i) {
        machine().step();
        if (record_current_breakpoint_hit()) {
            return chunk;
        }
        chunk = make_chunk();
        if (chunk.text.find(needle) != std::string::npos) {
            return chunk;
        }
        if (machine().cpu().core().halted()) {
            throw std::runtime_error("guest halted before requested UART text appeared");
        }
    }

    throw std::runtime_error("run_until_new_uart_contains exceeded step budget");
}

void DebugSession::run_until_halt(uint64_t max_steps) {
    ensure_loaded();
    if (machine().cpu().core().halted()) {
        return;
    }
    if (record_current_breakpoint_hit()) {
        return;
    }

    for (uint64_t i = 0; i < max_steps; ++i) {
        machine().step();
        if (machine().cpu().core().halted()) {
            return;
        }
        if (record_current_breakpoint_hit()) {
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

void DebugSession::debug_step_raw() {
    ensure_loaded();
    machine().step();
}

bool DebugSession::debug_halted() const {
    ensure_loaded();
    return machine().cpu().core().halted();
}

bool DebugSession::debug_try_load_guest_memory(uint64_t addr,
                                               int size,
                                               uint64_t& value) {
    ensure_loaded();
    return machine().bus().try_load(addr, size, value);
}

DebugSession::TranslationPlanSnapshot DebugSession::translation_plan() {
    ensure_loaded();
    const BackendDebugSnapshot backend_snapshot = machine().backend().debug_snapshot();
    const InterpreterDbtPrototypePlan plan =
        plan_interpreter_dbt_prototype_hot_path(machine().cpu(),
                                               machine().bus(),
                                               backend_snapshot.profile);

    const bool none = plan.fallback_reason == "no-hot-paths" ||
                      plan.fallback_reason == "insufficient-repetition" ||
                      plan.fallback_reason == "empty-hot-path";
    return TranslationPlanSnapshot{
        .candidate = !none,
        .inlineable = plan.ok,
        .start_pc = plan.start_pc,
        .end_pc = plan.end_pc,
        .executions = plan.candidate_executions,
        .retired_instructions = plan.candidate_retired_instructions,
        .inlineable_instructions = plan.inlineable_instructions,
        .fallback_pc = plan.fallback_pc,
        .reason = plan.fallback_reason,
        .boundary_kind = plan.boundary_kind,
    };
}

DbtJitDryRunSummary DebugSession::jit_dispatch() {
    ensure_loaded();
    const BackendDebugSnapshot backend_snapshot = machine().backend().debug_snapshot();

    DbtJitEngineDryRun engine;
    const DbtJitDryRunResult result =
        engine.dry_run_hot_path(machine().cpu(), machine().bus(), backend_snapshot.profile);
    return summarize_dbt_jit_dry_run(result);
}

DebugSnapshot DebugSession::snapshot() const {
    ensure_loaded();
    DebugSnapshot snapshot = collect_snapshot();
    snapshot.events = events_;
    return snapshot;
}

size_t DebugSession::post_load_action_count() const {
    return config_.post_load_actions.size();
}

void DebugSession::recreate_machine() {
    machine_ = std::make_unique<Machine>();
    machine_->set_backend_kind(config_.backend_kind);
    machine_->set_block_transport(config_.block_transport);
    machine_->set_l1_data_cache_enabled(config_.l1d_enabled);
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

    for (const PostLoadAction& action : config_.post_load_actions) {
        switch (action.kind) {
        case PostLoadAction::Kind::Payload:
            machine_->load_binary_payload(action.text, action.value);
            break;
        case PostLoadAction::Kind::SetGpr:
            machine_->set_gpr(action.text, action.value);
            break;
        }
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
    snapshot.profile = backend_snapshot.profile;
    const SimpleL1DataCache& l1d = cpu.l1_data_cache();
    const SimpleL1DataCacheStats& l1d_stats = l1d.stats();
    snapshot.l1_data_cache.enabled = l1d.enabled();
    snapshot.l1_data_cache.line_size_bytes = l1d.line_size_bytes();
    snapshot.l1_data_cache.capacity_lines = static_cast<uint64_t>(l1d.capacity_lines());
    snapshot.l1_data_cache.loads = l1d_stats.loads;
    snapshot.l1_data_cache.stores = l1d_stats.stores;
    snapshot.l1_data_cache.hits = l1d_stats.hits;
    snapshot.l1_data_cache.misses = l1d_stats.misses;
    snapshot.l1_data_cache.evictions = l1d_stats.evictions;
    snapshot.l1_data_cache.bypasses = l1d_stats.bypasses;
    snapshot.l1_data_cache.write_through_stores = l1d_stats.write_through_stores;
    for (size_t i = 0; i < snapshot.gpr.size(); ++i) {
        snapshot.gpr[i] = core.read_gpr(static_cast<uint32_t>(i));
    }
    snapshot.vector.sew_bytes = core.vector().sew_bytes();
    snapshot.vector.vl = core.vector().vl();
    for (size_t i = 0; i < snapshot.vector.registers.size(); ++i) {
        snapshot.vector.registers[i] = core.vector().read_reg(static_cast<uint32_t>(i));
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
    snapshot.guest_bus = machine().bus().last_guest_access();

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

    snapshot.devices.ai_accelerator = machine().ai_accelerator().debug_snapshot();

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
        append_event("stall", stall_event_detail(after.pipeline.stall_reason));
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
    const DebugBusAccess& before_access = before.guest_bus.valid ? before.guest_bus : before.bus;
    const DebugBusAccess& after_access = after.guest_bus.valid ? after.guest_bus : after.bus;
    if (after_access.valid &&
        (!before_access.valid || before_access.addr != after_access.addr || before_access.value != after_access.value ||
         before_access.write != after_access.write || before_access.device != after_access.device ||
         before_access.success != after_access.success || before_access.detail != after_access.detail ||
         before_access.source != after_access.source || before_access.kind != after_access.kind)) {
        std::string detail =
            after_access.device + " " + (after_access.write ? "write " : "read ") + hex_u64(after_access.addr);
        if (!after_access.success && !after_access.detail.empty()) {
            detail += " failed: " + after_access.detail;
        }
        append_event(after_access.write ? "store" : "load", detail);
    }
    if (after.summary.halted && !before.summary.halted) {
        append_event("halt", "program halted");
    }
    if (after.csrs.mcause != before.csrs.mcause || after.csrs.scause != before.csrs.scause) {
        append_event("trap", "trap state changed");
    }
}

bool DebugSession::is_breakpoint_pc(uint64_t pc) const {
    for (uint64_t breakpoint : breakpoints_) {
        if (breakpoint == pc) {
            return true;
        }
    }
    return false;
}

bool DebugSession::record_breakpoint_hit(uint64_t pc) {
    if (!is_breakpoint_pc(pc)) {
        return false;
    }
    append_event("breakpoint", "hit breakpoint at " + hex_u64(pc));
    return true;
}

bool DebugSession::record_current_breakpoint_hit() {
    return record_breakpoint_hit(machine().cpu().core().pc());
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

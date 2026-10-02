#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

#include "../../src/debug/debug_protocol.h"
#include "../../src/debug/debug_session.h"
#include "../../src/arch/csr_file.h"
#include "../../src/mem/ram.h"

namespace {
constexpr uint64_t kCode = 0x80000000;
constexpr uint64_t kSignature = 0x80001000;
constexpr uint64_t kBudget = 1000;
constexpr unsigned kQueries = 100;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Binary {
    std::string path;
    template <typename Words> explicit Binary(const Words& words) {
        char name[] = "/tmp/system_software_observation_XXXXXX";
        int fd = mkstemp(name);
        require(fd >= 0, "mkstemp failed");
        path = name;
        for (uint32_t word : words) {
            unsigned char bytes[4] = {static_cast<unsigned char>(word),
                static_cast<unsigned char>(word >> 8), static_cast<unsigned char>(word >> 16),
                static_cast<unsigned char>(word >> 24)};
            if (::write(fd, bytes, 4) != 4) {
                ::close(fd);
                ::unlink(path.c_str());
                throw std::runtime_error("binary write failed");
            }
        }
        ::close(fd);
        std::cout << "{\"probe\":\"guest_image\",\"words\":[";
        bool first = true;
        for (uint32_t word : words) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << word;
        }
        std::cout << "]}\n";
    }
    ~Binary() { ::unlink(path.c_str()); }
};

uint32_t store(unsigned rs, unsigned base, unsigned funct3) {
    return (rs << 20) | (base << 15) | (funct3 << 12) | 0x23;
}
uint32_t load(unsigned rd, unsigned base, unsigned funct3) {
    return (base << 15) | (funct3 << 12) | (rd << 7) | 3;
}

uint32_t addi(unsigned rd, unsigned rs, uint32_t imm) {
    return ((imm & 0xfff) << 20) | (rs << 15) | (rd << 7) | 0x13;
}

void address(std::vector<uint32_t>& code, unsigned rd, uint32_t value) {
    code.push_back(((value + 0x800U) & 0xfffff000U) | (rd << 7) | 0x37);
    code.push_back(addi(rd, rd, value));
    // LUI sign extends on RV64; explicitly construct the positive physical address.
    code.push_back((32U << 20) | (rd << 15) | (1U << 12) | (rd << 7) | 0x13);
    code.push_back((32U << 20) | (rd << 15) | (5U << 12) | (rd << 7) | 0x13);
}

void finish(DebugSession& session) {
    for (uint64_t i = 0; i < kBudget && !session.debug_halted(); ++i)
        session.debug_step_raw();
    require(session.debug_halted(), "guest exceeded 1000-step budget");
}

struct Result { uint64_t signature; std::string output; DebugSnapshot state; };

Result paired(const Binary& binary, BackendKind backend, unsigned mode) {
    DebugSession session;
    session.load_binary(binary.path, kCode, backend);
    uint64_t steps = 0;
    while (!session.debug_halted() && steps < kBudget) {
        if (mode == 1) session.step_cycle();
        else session.debug_step_raw();
        if (mode == 2 && steps == 3)
            for (unsigned n = 0; n < kQueries; ++n) (void)session.snapshot();
        ++steps;
    }
    require(session.debug_halted(), "paired guest exceeded budget");
    // All modes sample memory after execution, with the same physical read.
    uint64_t signature = 0;
    require(session.debug_try_load_guest_memory(kSignature, 8, signature), "signature read failed");
    Result result{signature, session.uart_output(0).text, session.snapshot()};
    require(signature == 65 && result.output == "A", "independent guest output/signature mismatch");
    std::cout << "{\"probe\":\"paired\",\"mode\":" << mode
              << ",\"step_budget\":" << kBudget << ",\"repeat_queries\":" << (mode == 2 ? kQueries : 0)
              << ",\"signature\":" << signature << ",\"uart\":\"A\",\"snapshot\":"
              << debug_snapshot_json(result.state) << "}\n";
    return result;
}

void compare(const Result& baseline, const Result& observed) {
    const auto& a = baseline.state;
    const auto& b = observed.state;
    require(baseline.signature == observed.signature && baseline.output == observed.output,
            "observation changed independent guest results");
    require(a.gpr == b.gpr && a.csrs.satp == b.csrs.satp && a.summary.pc == b.summary.pc &&
            a.summary.cycle == b.summary.cycle && a.summary.instret == b.summary.instret &&
            a.devices.clint.mtime == b.devices.clint.mtime &&
            a.profile.total_retirements == b.profile.total_retirements &&
            a.profile.total_memory_observations == b.profile.total_memory_observations &&
            a.profile.total_traps == b.profile.total_traps &&
            a.profile.shadow_cache.hits == b.profile.shadow_cache.hits &&
            a.profile.shadow_cache.misses == b.profile.shadow_cache.misses &&
            a.l1_data_cache.loads == b.l1_data_cache.loads &&
            a.l1_data_cache.stores == b.l1_data_cache.stores,
            "observation changed counters/registers");
}

void device_helper(BackendKind backend) {
    std::vector<uint32_t> rx_code;
    address(rx_code, 8, 0x10000000);
    address(rx_code, 9, kSignature);
    rx_code.insert(rx_code.end(), {load(6, 8, 4), store(6, 9, 3), 0x05d00893, 0x00000073});
    const Binary rx(rx_code);
    DebugSession session;
    session.load_binary(rx.path, kCode, backend);
    session.uart_input("AB");
    uint64_t consumed = 0, signature = 0;
    require(session.debug_try_load_guest_memory(0x10000000, 1, consumed), "RX helper failed");
    finish(session);
    require(session.debug_try_load_guest_memory(kSignature, 8, signature), "RX signature failed");
    require(consumed == 65 && signature == 66, "RX helper/next guest read evidence mismatch");
    std::cout << "{\"probe\":\"uart_helper\",\"backend\":\""
              << session.snapshot().summary.backend << "\",\"helper_consumed\":" << consumed
              << ",\"next_guest_read_signature\":" << signature << "}\n";

    // Guest configures UART RDI, PLIC priority and enable, then reads claim.
    std::vector<uint32_t> claim_code;
    address(claim_code, 8, 0x10000001);
    address(claim_code, 9, 0x0c000000 + Plic::UART_SOURCE_ID * 4);
    address(claim_code, 10, 0x0c002000);
    address(claim_code, 11, 0x0c200004);
    address(claim_code, 12, kSignature);
    claim_code.push_back(addi(5, 0, 1U << Plic::UART_SOURCE_ID));
    claim_code.push_back(addi(6, 0, 1));
    claim_code.insert(claim_code.end(), {store(6, 8, 0), store(6, 9, 2),
        store(5, 10, 2), load(7, 11, 2), store(7, 12, 3), 0x05d00893, 0x00000073});
    const Binary claim(claim_code);
    DebugSession plic;
    plic.load_binary(claim.path, kCode, backend);
    for (unsigned i = 0; i < 25; ++i) plic.step_commit();
    plic.uart_input("A");
    auto before = plic.snapshot().devices.plic;
    require(plic.debug_try_load_guest_memory(0x0c200004, 4, consumed), "claim helper failed");
    auto after = plic.snapshot().devices.plic;
    finish(plic);
    require(plic.debug_try_load_guest_memory(kSignature, 8, signature), "claim signature failed");
    require(before.pending && !before.claimed && after.claimed && !after.pending &&
            consumed == Plic::UART_SOURCE_ID && signature == 0, "claim side effect mismatch");
    std::cout << "{\"probe\":\"plic_helper\",\"backend\":\""
              << plic.snapshot().summary.backend << "\",\"before_pending\":true,"
              << "\"after_claimed\":true,\"helper_claim\":" << consumed
              << ",\"next_guest_claim_signature\":" << signature << "}\n";
}

void debug_gpr(BackendKind backend) {
    std::vector<uint32_t> code{addi(6, 5, 1)};
    address(code, 8, kSignature);
    code.insert(code.end(), {store(6, 8, 3), 0x05d00893, 0x00000073});
    const Binary binary(code);
    DebugSession session;
    session.load_binary(binary.path, kCode, backend);
    session.set_gpr("x5", 41);
    auto before = session.snapshot();
    finish(session);
    uint64_t actual = 0;
    require(session.debug_try_load_guest_memory(kSignature, 8, actual), "set_gpr signature failed");
    std::cout << "{\"probe\":\"debug_set_gpr\",\"backend\":\""
              << before.summary.backend << "\",\"snapshot_x5_before\":" << before.gpr[5]
              << ",\"expected_guest_signature\":42,\"actual_guest_signature\":" << actual
              << ",\"contract_matches\":" << (actual == 42 ? "true" : "false") << "}\n";
}

void translation(bool query, bool flush_before_load = false) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kCode);
    constexpr uint64_t root = 0x80002000, l1 = 0x80003000, l0 = 0x80004000;
    constexpr uint64_t page_a = 0x80005000, page_b = 0x80006000, va = 0x2000;
    auto pte = [](uint64_t pa) { return ((pa >> 12) << 10) | 7; };
    ram.store(root, ((l1 >> 12) << 10) | 1, 8);
    ram.store(l1, ((l0 >> 12) << 10) | 1, 8);
    ram.store(l0 + 16, pte(page_a), 8);
    ram.store(page_a, 0x11, 8);
    ram.store(page_b, 0x22, 8);
    cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
    cpu.csr().write(CSR_SATP, (8ULL << 60) | (root >> 12), cpu.core());
    uint64_t ignored = 0;
    bus.try_load(kSignature, 8, ignored);
    if (query) {
        auto translated = cpu.address_space().translate_result(bus, va, AccessType::Load, false);
        require(translated.ok && translated.paddr == page_a, "translation query failed");
        require(ram.load(l0 + 16, 8) == pte(page_a), "false translation changed A/D");
        require(bus.last_access().source == "page-walk", "query did not expose page-walk access");
    }
    // Old cached translation and A/D writeback replacing a new PPN are separate observations.
    ram.store(l0 + 16, pte(page_b), 8);
    const uint64_t remapped_pte = ram.load(l0 + 16, 8);
    if (flush_before_load) cpu.address_space().flush_tlb();
    auto actual = cpu.address_space().load_result(bus, va, 8);
    const uint64_t after_load_pte = ram.load(l0 + 16, 8);
    cpu.address_space().flush_tlb();
    auto refreshed = cpu.address_space().load_result(bus, va, 8);
    const uint64_t after_flush_load_pte = ram.load(l0 + 16, 8);
    auto ppn = [](uint64_t entry) { return (entry >> 10) & ((1ULL << 44) - 1); };
    std::cout << "{\"probe\":\"translate_false\",\"query\":" << (query ? "true" : "false")
              << ",\"flush_before_load\":" << (flush_before_load ? "true" : "false")
              << ",\"query_ad_unchanged\":" << (query ? "true" : "null")
              << ",\"query_bus_source\":" << (query ? "\"page-walk\"" : "null")
              << ",\"remapped_pte\":" << remapped_pte << ",\"remapped_ppn\":" << ppn(remapped_pte)
              << ",\"next_load_ok\":" << (actual.ok ? "true" : "false")
              << ",\"next_load_without_sfence\":" << actual.value
              << ",\"after_load_pte\":" << after_load_pte << ",\"after_load_ppn\":" << ppn(after_load_pte)
              << ",\"mapping_ppn_preserved\":" << (ppn(after_load_pte) == ppn(remapped_pte) ? "true" : "false")
              << ",\"load_after_flush_ok\":" << (refreshed.ok ? "true" : "false")
              << ",\"load_after_flush\":" << refreshed.value
              << ",\"after_flush_load_pte\":" << after_flush_load_pte
              << ",\"after_flush_load_ppn\":" << ppn(after_flush_load_pte) << "}\n";
}

void planner(BackendKind backend, bool jit) {
    const Binary loop(std::array<uint32_t, 3>{0x00108093, 0x00208113, 0xfe000ce3});
    DebugSession session;
    session.load_binary(loop.path, kCode, backend);
    for (unsigned n = 0; n < 100; ++n) session.debug_step_raw();
    session.set_memory(kSignature, 0x55, 8, false);
    auto before = session.snapshot();
    bool candidate = false;
    if (jit) {
        auto result = session.jit_dispatch();
        candidate = result.candidate_executions >= 2;
        require(!result.executed_guest_code, "dry run unexpectedly executed guest code");
    } else candidate = session.translation_plan().candidate;
    auto after = session.snapshot();
    require(candidate, "planner candidate unavailable");
    require(before.bus.source == "debug" && after.bus.source == "fetch", "planner bus overwrite missing");
    require(before.summary.cycle == after.summary.cycle && before.summary.instret == after.summary.instret &&
            before.gpr == after.gpr, "planner changed execution registers/counters");
    std::cout << "{\"probe\":\"" << (jit ? "jit_dispatch" : "translation_plan")
              << "\",\"backend\":\"" << after.summary.backend
              << "\",\"candidate\":true,\"before_bus_source\":\"debug\","
              << "\"after_bus_source\":\"fetch\",\"counters_unchanged\":true,"
              << "\"code_pte_ad\":\"unverified-bare-mode\"}\n";
}
}  // namespace

int main() {
    try {
        std::vector<uint32_t> signature_code;
        address(signature_code, 8, kSignature);
        address(signature_code, 9, 0x10000000);
        signature_code.insert(signature_code.end(), {0x04100293, store(5, 8, 3),
            store(5, 9, 0), 0x05d00893, 0x00000073});
        const Binary signature(signature_code);
        for (auto backend : {BackendKind::Functional, BackendKind::Pipeline}) {
            debug_gpr(backend);
            auto baseline = paired(signature, backend, 0);
            compare(baseline, paired(signature, backend, 1));
            compare(baseline, paired(signature, backend, 2));
            device_helper(backend);
            planner(backend, false);
            planner(backend, true);
        }
        translation(false);
        translation(true);
        translation(true, true);
        std::cout << "{\"probe\":\"complete\",\"status\":\"completed\"}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

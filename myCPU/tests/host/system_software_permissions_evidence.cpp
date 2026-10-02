#include <cstdio>
#include <memory>
#include <stdexcept>

#include "../../src/cpu.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {
constexpr uint64_t root = MEM_BASE + 0x10000;
constexpr uint64_t l1 = root + 0x1000;
constexpr uint64_t l0 = root + 0x2000;
constexpr uint64_t code = root + 0x3000;
constexpr uint64_t data = root + 0x4000;
constexpr uint64_t code_va = 0x4000;
constexpr uint64_t data_va = 0x6000;
constexpr uint64_t marker = 0x1122334455667788ULL;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

uint64_t pte(uint64_t pa, uint64_t flags) { return ((pa >> 12) << 10) | flags; }

void map(Ram& ram, uint64_t flags) {
    ram.store(root, pte(l1, 1), 8);
    ram.store(l1, pte(l0, 1), 8);
    ram.store(l0 + 4 * 8, pte(code, 0x5b), 8); // User RX, A pre-set.
    ram.store(l0 + 6 * 8, pte(data, flags), 8);
}

void run(const char* backend_name, bool pipeline, const char* name, uint64_t flags,
         uint64_t expected_cause, bool writable) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, code_va);
    map(ram, flags);
    ram.store(code, 0x00553023, 4); // sd x5, 0(x10)
    ram.store(code + 4, 0x00600313, 4); // addi x6, x0, 6
    ram.store(code + 8, 0x00000073, 4); // ecall -> M trap handler
    ram.store(MEM_BASE + 0x100, 0x05d00893, 4);
    ram.store(MEM_BASE + 0x104, 0x00000073, 4);
    cpu.csr().write(CSR_MTVEC, MEM_BASE + 0x100, cpu.core());
    cpu.csr().write(CSR_SATP, (8ULL << 60) | (root >> 12), cpu.core());
    cpu.core().set_privilege_mode(PrivilegeMode::User);
    cpu.core().write_gpr(5, marker);
    cpu.core().write_gpr(10, data_va);
    std::unique_ptr<ExecutionBackend> backend;
    if (pipeline) backend = std::make_unique<PipelineBackend>(cpu, bus);
    else backend = std::make_unique<FunctionalBackend>(cpu, bus);
    unsigned steps = 0;
    while (!cpu.core().halted() && steps < 128) { backend->step(); ++steps; }
    const auto cause = cpu.csr().read(CSR_MCAUSE, cpu.core());
    const auto tval = cpu.csr().read(CSR_MTVAL, cpu.core());
    const auto value = ram.load(data, 8);
    require(cpu.core().halted(), "128-step stop budget exhausted");
    require(cause == expected_cause, "unexpected trap cause");
    require(value == (writable ? marker : 0), "store visibility differs from page permission");
    require(cpu.core().read_gpr(6) == (writable ? 6 : 0), "younger GPR visibility differs");
    if (!writable) require(tval == data_va, "store page-fault tval differs");
    std::printf("{\"scenario\":\"%s\",\"backend\":\"%s\",\"steps\":%u,"
                "\"cycle\":%llu,\"instret\":%llu,\"mcause\":%llu,\"mtval\":%llu,"
                "\"va\":%llu,\"pa\":%llu,\"memory\":%llu,\"x6\":%llu}\n",
                name, backend_name, steps, (unsigned long long)cpu.core().cycle(),
                (unsigned long long)cpu.core().instret(), (unsigned long long)cause,
                (unsigned long long)tval, (unsigned long long)data_va,
                (unsigned long long)data, (unsigned long long)value,
                (unsigned long long)cpu.core().read_gpr(6));
}
}

int main() {
    try {
        for (bool pipeline : {false, true}) {
            const char* name = pipeline ? "pipeline" : "functional";
            run(name, pipeline, "user-rw-store", 0xd7, 8, true);
            run(name, pipeline, "user-readonly-store", 0x53, 15, false);
            run(name, pipeline, "user-unmapped-store", 0, 15, false);
        }
        // A misaligned cross-page store may expose the earlier chunk before a later fault.
        // This is an implementation boundary probe, not an all-or-nothing assertion.
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, code_va);
        map(ram, 0xd7);
        cpu.csr().write(CSR_SATP, (8ULL << 60) | (root >> 12), cpu.core());
        cpu.core().set_privilege_mode(PrivilegeMode::User);
        const auto result = cpu.address_space().store_result(bus, data_va + 4092, marker, 8);
        const auto prefix = ram.load(data + 4092, 4);
        require(!result.ok && result.fault.cause == 15, "cross-page second chunk must fault");
        require(prefix == 0x55667788, "cross-page observed prefix differs from implementation boundary");
        std::printf("{\"scenario\":\"cross-page-result-api\",\"backend\":\"direct-address-space\","
                    "\"ok\":false,\"cause\":%llu,\"tval\":%llu,\"written_prefix\":%llu}\n",
                    (unsigned long long)result.fault.cause, (unsigned long long)result.fault.tval,
                    (unsigned long long)prefix);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}

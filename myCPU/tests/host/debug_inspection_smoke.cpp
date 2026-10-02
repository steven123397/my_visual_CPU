#include <cassert>
#include <limits>
#include "../../src/platform/machine.h"

int main() {
    for (auto backend : {BackendKind::Functional, BackendKind::Pipeline}) {
        for (bool cache : {false, true}) {
            Machine m;
            m.set_backend_kind(backend);
            m.set_l1_data_cache_enabled(cache);
            m.bus().try_store(MEM_BASE, 0x807f0100, 4);
            m.uart().inject_input("AB");
            m.plic().store(PLIC_BASE + 4 * Plic::UART_SOURCE_ID, 1, 4);
            m.plic().store(PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_MACHINE),
                           1U << Plic::UART_SOURCE_ID, 4);
            m.plic().set_source_level(Plic::UART_SOURCE_ID, true);
            const auto before = m.bus().last_access();
            const auto cycle = m.cpu().core().cycle();
            auto bytes = m.inspect_memory(MEM_BASE, 4);
            assert((bytes == std::vector<uint8_t>{0, 1, 127, 128}));
            assert(m.inspect_memory(MEM_BASE + MEM_SIZE - 1, 1).size() == 1);
            assert(m.inspect_memory(MEM_BASE, 4096).size() == 4096);
            for (auto request : {std::pair<uint64_t, size_t>{MEM_BASE, 0},
                                 {MEM_BASE, 4097}, {MEM_BASE + MEM_SIZE - 1, 2},
                                 {UINT64_MAX, 2}, {UART_BASE, 1}, {0, 1}}) {
                bool rejected = false;
                try { m.inspect_memory(request.first, request.second); }
                catch (const std::exception&) { rejected = true; }
                assert(rejected);
            }
            assert(m.bus().last_access().addr == before.addr);
            assert(m.bus().last_access().value == before.value);
            assert(m.cpu().core().cycle() == cycle);
            assert(m.plic().source_pending(Plic::UART_SOURCE_ID));
            assert(m.plic().load(PLIC_BASE + PLIC_CONTEXT_OFFSET(PLIC_CONTEXT_MACHINE) + 4, 4)
                   == Plic::UART_SOURCE_ID);
            assert(m.plic().load(PLIC_BASE + PLIC_CONTEXT_OFFSET(PLIC_CONTEXT_MACHINE) + 4, 4) == 0);
            assert(m.uart().load(UART_BASE, 1) == 'A');
            assert(m.uart().load(UART_BASE, 1) == 'B');
        }
    }
}

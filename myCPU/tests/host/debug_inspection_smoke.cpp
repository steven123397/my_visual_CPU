#include <cassert>
#include <limits>
#include "../../src/platform/machine.h"

int main() {
    {
        Machine m;
        auto& c = m.cpu();
        const uint64_t root = MEM_BASE + 0x1000;
        const uint64_t satp = (8ULL << 60) | (root >> 12);
        c.core().set_privilege_mode(PrivilegeMode::Supervisor);
        c.csr().write(CSR_SATP, satp, c.core());
        m.bus().try_store(root, (MEM_BASE >> 12 << 10) | 0xc7, 8);
        assert(m.inspect_page(0x123, AccessType::Load).paddr == MEM_BASE + 0x123);
        m.bus().try_store(root, (0x40000000ULL >> 12 << 10) | 0xc7, 8);
        assert(c.address_space().translate_result(m.bus(), 0x123, AccessType::Load).paddr == 0x40000123);
    }
    {
        Machine m;
        auto& c = m.cpu();
        const uint64_t root = MEM_BASE + 0x1000;
        const uint64_t satp = (8ULL << 60) | (root >> 12);
        m.bus().try_store(root, (MEM_BASE >> 12 << 10) | 0x7, 8);
        PageInspectionContext ctx{satp, PrivilegeMode::Supervisor, false, false};
        auto page = m.inspect_page(0x123, AccessType::Store, ctx);
        assert(page.ok && page.paddr == MEM_BASE + 0x123 && page.page_size == (1ULL << 30));
        assert(page.ad_update_needed && page.entries.size() == 1);
        auto bytes = m.inspect_memory(root, 8);
        assert(bytes[0] == 7);
        c.core().set_privilege_mode(PrivilegeMode::Supervisor);
        c.csr().write(CSR_SATP, satp, c.core());
        assert(c.address_space().translate_result(m.bus(), 0x123, AccessType::Load).paddr == MEM_BASE + 0x123);
        m.bus().try_store(root, (0x40000000ULL >> 12 << 10) | 0xc7, 8);
        assert(m.inspect_page(0x123, AccessType::Load).paddr == 0x40000123);
        assert(c.address_space().translate_result(m.bus(), 0x123, AccessType::Load).paddr == MEM_BASE + 0x123);
        assert(!m.inspect_page(1ULL << 39, AccessType::Load).ok);
        const uint64_t l1 = root + 4096, l0 = l1 + 4096;
        m.bus().try_store(root, (l1 >> 12 << 10) | 1, 8);
        m.bus().try_store(l1, (MEM_BASE >> 12 << 10) | 0x19, 8);
        auto denied = m.inspect_page(0x123, AccessType::Load, ctx);
        assert(denied.mapped && !denied.permission_allowed && denied.leaf_level == 1);
        ctx.sum = ctx.mxr = true;
        assert(m.inspect_page(0x123, AccessType::Load, ctx).ok);
        assert(!m.inspect_page(0x123, AccessType::Instruction, ctx).ok);
        m.bus().try_store(l1, (l0 >> 12 << 10) | 1, 8);
        m.bus().try_store(l0, (MEM_BASE >> 12 << 10) | 0x17, 8);
        ctx.effective_privilege = PrivilegeMode::User;
        auto small = m.inspect_page(0x123, AccessType::Store, ctx);
        assert(small.ok && small.page_size == 4096 && small.entries.size() == 3);
        m.bus().try_store(l0, (1ULL << 63) | 7, 8);
        assert(m.inspect_page(0, AccessType::Load, ctx).error == "invalid-pte");
        ctx.satp = (8ULL << 60) | (UART_BASE >> 12);
        assert(m.inspect_page(0, AccessType::Load, ctx).error == "non-ram-page-table");
        ctx.satp = 9ULL << 60;
        assert(m.inspect_page(0, AccessType::Load, ctx).error == "unsupported-mode");
        ctx.satp = 0;
        assert(m.inspect_page(UINT64_MAX, AccessType::Load, ctx).paddr == UINT64_MAX);
        c.core().set_privilege_mode(PrivilegeMode::Machine);
        c.csr().write(CSR_MSTATUS, MSTATUS_MPRV | (1ULL << MSTATUS_MPP_SHIFT), c.core());
        assert(c.address_space().inspection_context(AccessType::Load).effective_privilege == PrivilegeMode::Supervisor);
        assert(c.address_space().inspection_context(AccessType::Instruction).effective_privilege == PrivilegeMode::Machine);
    }
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

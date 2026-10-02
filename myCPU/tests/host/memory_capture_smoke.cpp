#include <cassert>
#include <vector>
#include "../../src/debug/memory_capture.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/platform/machine.h"

struct Receiver : MemoryRecordReceiver {
    std::vector<MemoryRecord> records;
    uint64_t unsupported_count = 0;
    void receive(MemoryRecord record) override { records.push_back(record); }
    void unsupported() override { ++unsupported_count; }
};

int main() {
    for (bool cache : {false, true}) {
        Machine m;
        m.set_l1_data_cache_enabled(cache);
        auto& c = m.cpu();
        cpu_init(c, MEM_BASE);
        // ld x3,0(x1); ld x3,0(x1); sd x2,0(x1); sc.d x3,x2,(x1)
        const uint32_t code[] = {0x0000b183, 0x0000b183, 0x0020b023, 0x1820b1af};
        for (size_t i = 0; i < 4; ++i) m.bus().try_store(MEM_BASE + 4*i, code[i], 4);
        m.bus().try_store(MEM_BASE + 4096, 42, 8);
        c.core().write_gpr(1, MEM_BASE + 4096);
        c.core().write_gpr(2, 17);
        Receiver r;
        FunctionalBackend backend(c, m.bus());
        backend.set_memory_receiver(&r);
        for (int i = 0; i < 4; ++i) backend.step();
        assert(r.records.size() == 4);
        assert(r.records[0].segments[0].value == 42);
        assert(r.records[1].segments[0].value == 42);
        assert(r.records[2].segments[0].value == 17);
        assert(r.records[3].sc_result == 1 && r.records[3].segments[0].outcome == MemoryOutcome::NoAccess);
        for (size_t i = 0; i < 4; ++i) {
            assert(r.records[i].pc == MEM_BASE + i*4 && r.records[i].retired);
            assert(r.records[i].end_cycle == i+1 && r.records[i].end_instret == i+1);
        }
    }
    {
        Machine m;
        auto& c = m.cpu();
        cpu_init(c, MEM_BASE);
        m.bus().try_store(MEM_BASE, 0x0020b023, 4);
        c.core().write_gpr(1, MEM_BASE + MEM_SIZE - 4);
        c.core().write_gpr(2, 0x8877665544332211ULL);
        Receiver r;
        FunctionalBackend backend(c, m.bus());
        backend.set_memory_receiver(&r);
        backend.step();
        const auto& record = r.records.at(0);
        assert(!record.retired && record.trap.cause == 7);
        assert(record.pc == MEM_BASE && record.privilege == PrivilegeMode::Machine);
        assert(record.segment_count == 2 && record.segments[0].value == 0x44332211);
        assert(record.segments[1].outcome == MemoryOutcome::Fault);
        assert(m.inspect_memory(MEM_BASE + MEM_SIZE - 4, 4)[0] == 0x11);
    }
    for (bool enabled : {false, true}) {
        Machine m;
        auto& c = m.cpu();
        cpu_init(c, MEM_BASE);
        // LR.D; successful SC.D with rd=x0; AMOADD.D; UART load; FP load.
        const uint32_t code[] = {0x1000b1af, 0x1820b02f, 0x0020b1af, 0x00024183, 0x0000b207};
        for (size_t i = 0; i < 5; ++i) m.bus().try_store(MEM_BASE + i*4, code[i], 4);
        m.bus().try_store(MEM_BASE + 4096, 42, 8);
        c.core().write_gpr(1, MEM_BASE + 4096);
        c.core().write_gpr(2, 17);
        c.core().write_gpr(4, UART_BASE);
        c.csr().write(CSR_MSTATUS, 1ULL << 13, c.core());
        m.uart().inject_input("AB");
        Receiver r;
        FunctionalBackend backend(c, m.bus());
        if (enabled) backend.set_memory_receiver(&r);
        for (int i = 0; i < 5; ++i) backend.step();
        assert(c.core().cycle() == 5 && c.core().instret() == 5);
        assert(c.core().read_gpr(3) == 'A' && m.uart().load(UART_BASE, 1) == 'B');
        assert(m.inspect_memory(MEM_BASE + 4096, 1)[0] == 34);
        if (enabled) {
            assert(r.records.size() == 4 && r.unsupported_count == 1);
            assert(r.records[1].sc_result == 0);
            assert(r.records[2].segment_count == 2);
            assert(r.records[2].segments[0].value == 17 && r.records[2].segments[1].value == 34);
        }
    }
}

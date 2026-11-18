#include <cassert>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include "../../src/debug/debug_session.h"
#include "../../src/debug/debug_protocol.h"
#include "../../src/debug/memory_capture.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/platform/machine.h"

struct Receiver : MemoryRecordReceiver {
    std::vector<MemoryRecord> records;
    uint64_t unsupported_count = 0;
    void receive(MemoryRecord record) override { records.push_back(record); }
    void unsupported() override { ++unsupported_count; }
};

template<class F> void rejected(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}

int main() {
    {
        Machine m;
        auto& c = m.cpu();
        cpu_init(c, MEM_BASE);
        // c.ld x9,0(x8); c.sd x9,0(x8); vector byte load/store.
        m.bus().try_store(MEM_BASE, 0x6004, 2);
        m.bus().try_store(MEM_BASE + 2, 0xe004, 2);
        m.bus().try_store(MEM_BASE + 4, 0x00040087, 4);
        m.bus().try_store(MEM_BASE + 8, 0x00140027, 4);
        m.bus().try_store(MEM_BASE + 4096, 42, 8);
        c.core().write_gpr(8, MEM_BASE + 4096);
        c.core().vector().set_config(1, 1);
        Receiver r;
        FunctionalBackend backend(c, m.bus());
        backend.set_memory_receiver(&r);
        for (int i = 0; i < 4; ++i) backend.step();
        assert(r.records.size() == 2 && r.unsupported_count == 2);
        assert(r.records[0].instruction_bytes == 2 && r.records[0].raw == 0x6004);
        assert(r.records[1].instruction_bytes == 2 && r.records[1].raw == 0xe004);
        assert(r.records[0].segments[0].value == 42 && r.records[1].segments[0].value == 42);
    }
    {
        MemoryCapture capture;
        CaptureConfig config;
        config.capacity = 1;
        config.operation = MemoryOperation::Store;
        capture.start(config, 7);
        MemoryRecord r;
        r.operation = MemoryOperation::Load;
        capture.receive(r);
        r.operation = MemoryOperation::Store;
        capture.receive(r);
        capture.receive(r);
        capture.unsupported();
        capture.stop();
        const auto meta = capture.metadata();
        assert(meta.attempted == 3 && meta.filtered == 1 && meta.recorded == 1 && meta.dropped == 1 && meta.unsupported == 1);
        auto page = capture.read({meta.capture_id, 0, {}}, 1);
        assert(page.records[0].instruction_id == 2 && page.records[0].machine_generation == 7);
        assert(capture.read({meta.capture_id, 0, {}}, 1).records[0].instruction_id == 2);
        capture.clear();
        bool stale = false;
        try { capture.read(page.next, 1); } catch (const std::exception&) { stale = true; }
        assert(stale);
        capture.start({}, 8);
        assert(capture.metadata().capture_id != meta.capture_id);
        capture.stop();
    }
    {
        char image[] = "/tmp/capture_reset_XXXXXX", payload[] = "/tmp/capture_payload_XXXXXX";
        const uint32_t word = 0x00000013;
        for (char* name : {image, payload}) {
            const int fd = mkstemp(name);
            assert(write(fd, &word, sizeof(word)) == sizeof(word));
            close(fd);
        }
        DebugSession s;
        s.load_binary(image, MEM_BASE, BackendKind::Functional);
        s.load_binary_payload(payload, MEM_BASE + 4096);
        s.set_gpr("x5", 123);
        s.step_cycle();
        s.capture_start(); s.capture_stop();
        const auto generation = s.capture_metadata().machine_generation;
        const auto before = debug_snapshot_json(s.snapshot());
        unlink(payload);
        rejected([&] { s.reset(); });
        assert(debug_snapshot_json(s.snapshot()) == before);
        s.capture_clear(); s.capture_start();
        assert(s.capture_metadata().machine_generation == generation);
        s.capture_stop(); s.capture_clear();
        rejected([&] { s.load_binary("/missing/capture-image", MEM_BASE, BackendKind::Pipeline); });
        assert(debug_snapshot_json(s.snapshot()) == before);
        s.capture_start();
        assert(s.capture_metadata().machine_generation == generation);
        s.capture_stop();
        unlink(image);
    }
    {
        std::vector<uint64_t> baseline;
        for (bool enabled : {false, true}) {
            Machine m;
            auto& c = m.cpu();
            cpu_init(c, 0x1000);
            const uint64_t root = MEM_BASE + 4096, l1 = root + 4096, l0 = l1 + 4096;
            const uint64_t code = MEM_BASE + 0x4000, handler = code + 4096, data = handler + 4096;
            const uint64_t satp = (8ULL << 60) | (root >> 12);
            m.bus().try_store(root, (l1 >> 12 << 10) | 1, 8);
            m.bus().try_store(l1, (l0 >> 12 << 10) | 1, 8);
            m.bus().try_store(l0 + 8, (code >> 12 << 10) | 0x5b, 8);
            m.bus().try_store(l0 + 16, (handler >> 12 << 10) | 0x4b, 8);
            m.bus().try_store(l0 + 24, (data >> 12 << 10) | 0xc7, 8);
            m.bus().try_store(code, 0x0000b183, 4);
            m.bus().try_store(handler, 0x00013283, 4);
            m.bus().try_store(data, 0x12345678, 8);
            c.csr().write(CSR_SATP, satp, c.core());
            c.csr().write(CSR_MEDELEG, 1ULL << 13, c.core());
            c.csr().write(CSR_STVEC, 0x2000, c.core());
            c.core().set_privilege_mode(PrivilegeMode::User);
            c.core().write_gpr(1, 0x6000);
            c.core().write_gpr(2, 0x3000);
            Receiver r;
            FunctionalBackend backend(c, m.bus());
            if (enabled) backend.set_memory_receiver(&r);
            backend.step(); backend.step();
            assert(c.core().read_gpr(5) == 0x12345678);
            assert(c.core().pc() == 0x2004 && c.core().cycle() == 2 && c.core().instret() == 1);
            std::vector<uint64_t> state;
            for (uint32_t i = 0; i < 32; ++i) state.push_back(c.core().read_gpr(i));
            for (uint32_t csr : {CSR_SATP, CSR_MSTATUS, CSR_SCAUSE, CSR_STVAL, CSR_SEPC}) state.push_back(c.csr().read(csr, c.core()));
            state.insert(state.end(), {c.core().pc(), c.core().cycle(), c.core().instret(), m.clint().mtime(),
                m.bus().last_access().addr, m.bus().last_access().value,
                backend.debug_snapshot().profile.total_memory_observations, backend.debug_snapshot().profile.total_traps});
            if (!enabled) baseline = state; else assert(state == baseline);
            if (enabled) {
                assert(r.records.size() == 2);
                const auto& fault = r.records[0];
                assert(fault.pc == 0x1000 && fault.raw == 0x0000b183 && fault.satp == satp);
                assert(fault.privilege == PrivilegeMode::User && fault.effective_privilege == PrivilegeMode::User);
                assert(!fault.retired && fault.trap.cause == 13 && fault.trap.tval == 0x6000);
                assert(fault.segment_count == 1 && !fault.segments[0].paddr && fault.segments[0].outcome == MemoryOutcome::Fault);
                const auto& next = r.records[1];
                assert(next.pc == 0x2000 && next.raw == 0x00013283 && next.satp == satp);
                assert(next.privilege == PrivilegeMode::Supervisor && next.effective_privilege == PrivilegeMode::Supervisor);
                assert(next.retired && !next.trap.valid && next.segment_count == 1);
                assert(next.segments[0].paddr == data && next.segments[0].value == 0x12345678);
            }
        }
    }
    {
        MemoryCapture capture;
        CaptureConfig cfg;
        cfg.capacity = 0;
        rejected([&] { capture.start(cfg, 1); });
        cfg.capacity = 65537;
        rejected([&] { capture.start(cfg, 1); });
        cfg.capacity = 4;
        cfg.va_begin = 4096;
        cfg.va_end = 4097;
        cfg.satp = 0x8000000000000123ULL;
        cfg.effective_privilege = PrivilegeMode::User;
        capture.start(cfg, 1);
        rejected([&] { capture.clear(); });
        MemoryRecord r;
        r.vaddr = 4092; r.bytes = 8; r.satp = *cfg.satp; r.effective_privilege = PrivilegeMode::User;
        capture.receive(r);
        auto first = capture.read({capture.metadata().capture_id, 0, {}}, 1);
        capture.receive(r);
        assert(capture.read(first.next).records.empty());
        assert(capture.read({first.next.capture_id, 1, {}}).records.size() == 1);
        r.satp = 0;
        capture.receive(r);
        assert(capture.metadata().filtered == 1);
        rejected([&] { capture.read({first.next.capture_id, 3, {}}); });
        capture.stop();
        capture.stop();
        rejected([&] { capture.start({}, 2); });
    }
    {
        char path[] = "/tmp/capture_smoke_XXXXXX";
        int fd = mkstemp(path);
        const uint32_t code[] = {0x0000b183, 0x0000b183, 0x0000006f};
        assert(write(fd, code, sizeof(code)) == sizeof(code));
        close(fd);
        DebugSession s;
        s.load_binary(path, MEM_BASE, BackendKind::Functional);
        s.set_gpr("x1", MEM_BASE);
        CaptureConfig cfg; cfg.capacity = 1;
        s.capture_start(cfg);
        const auto before = s.snapshot();
        rejected([&] { s.set_gpr("x1", 1); });
        rejected([&] { s.set_csr("satp", 1); });
        rejected([&] { s.set_memory(MEM_BASE, 0, 4, false); });
        rejected([&] { s.reset(); });
        rejected([&] { s.load_binary(path, MEM_BASE, BackendKind::Pipeline); });
        rejected([&] { s.load_binary_payload(path, MEM_BASE); });
        rejected([&] { s.uart_input("A"); });
        rejected([&] { s.translation_plan(); });
        rejected([&] { s.jit_dispatch(); });
        assert(s.snapshot().gpr == before.gpr && s.snapshot().summary.cycle == before.summary.cycle);
        s.inspect_memory(MEM_BASE, 4);
        s.inspect_page(MEM_BASE, AccessType::Load);
        s.step_cycle(); s.step_cycle(); s.step_cycle();
        s.capture_stop();
        auto meta = s.capture_metadata();
        assert(meta.recorded == 1 && meta.dropped == 1 && meta.attempted == 2);
        s.reset();
        assert(s.capture_read({meta.capture_id, 0, {}}).records[0].machine_generation == meta.machine_generation);
        s.capture_clear();
        s.capture_start();
        assert(s.capture_metadata().machine_generation > meta.machine_generation);
        s.capture_stop(); s.capture_clear();
        s.load_binary(path, MEM_BASE, BackendKind::Pipeline);
        rejected([&] { s.capture_start(); });
        assert(s.capture_metadata().state == CaptureState::Idle);
        unlink(path);
    }
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

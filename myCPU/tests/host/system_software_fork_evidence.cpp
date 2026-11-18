#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../../src/platform/machine.h"
#include "../../src/debug/debug_protocol_response.h"

namespace {
using U64 = uint64_t;
constexpr U64 initial[2] = {0x11223344, 0x55667788};
constexpr U64 changed[2] = {0xaabbccdd, 0xeeff0011};
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

struct Probe {
    Machine machine;
    std::map<std::string, U64> layout;
    std::array<U64, 2> child_pa{};
    std::array<unsigned, 2> stores{};
    U64 child_slot = 0, parent_pid = 0, child_pid = 0, va = 0, child_root = 0;
    bool zombie = false;
    bool capture_enabled = true;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    U64 at(const char* key) const { return layout.at(key); }
    U64 current_proc() { return read(at("cpus") + at("cpu_proc")); }
    U64 read(U64 addr, int size = 8) {
        const auto bytes = machine.inspect_memory(addr, size);
        U64 result = 0;
        for (size_t i = 0; i < bytes.size(); ++i) result |= U64(bytes[i]) << (8 * i);
        return result;
    }
    U64 find_proc(U64 pid) {
        for (U64 i = 0; i < at("nproc"); ++i) {
            U64 slot = at("proc") + i * at("proc_size");
            if (read(slot + at("pid"), 4) == pid) return slot;
        }
        throw std::runtime_error("PID not found in proc array");
    }
    std::pair<U64, U64> translate(U64 root, U64 address) {
        const auto page = machine.inspect_page(address, AccessType::Store,
            {(8ULL << 60) | (root >> 12), PrivilegeMode::User, false, false});
        require(page.ok && page.leaf_level == 0, "expected user R/W level-zero leaf");
        return {page.paddr, page.entries.back().value};
    }
    void step() {
        const auto& core = machine.cpu().core();
        const U64 pc = core.pc();
        if (child_slot && !zombie && pc == at("sched") && current_proc() == child_slot &&
            read(child_slot + at("state"), 4) == at("zombie")) {
            require(read(child_slot + at("xstate"), 4) == 37, "zombie exit status mismatch");
            zombie = true;
            std::printf("{\"event\":\"zombie\",\"pid\":%llu,\"xstate\":37,\"pc\":%llu,\"cycle\":%llu}\n",
                        (unsigned long long)child_pid, (unsigned long long)pc,
                        (unsigned long long)core.cycle());
        }
        machine.step();
    }
    void start_window() {
        if (!capture_enabled) return;
        CaptureConfig cfg;
        cfg.capacity = 16;
        cfg.operation = MemoryOperation::Store;
        cfg.effective_privilege = PrivilegeMode::User;
        cfg.satp = (8ULL << 60) | (child_root >> 12);
        cfg.va_begin = va;
        cfg.va_end = va + 4096 + 8;
        machine.capture_start(cfg);
    }
    void stop_window() {
        if (!capture_enabled) return;
        const U64 cycle = machine.cpu().core().cycle();
        machine.capture_stop();
        const auto meta = machine.capture_metadata();
        require(!meta.dropped && !meta.unsupported && meta.recorded == 2, "incomplete child store capture");
        const auto page = machine.capture_read({meta.capture_id, 0, {}});
        require(page.done, "unexpected capture pagination");
        require(machine.capture_read({meta.capture_id, 0, {}}).records.size() == page.records.size(), "export consumed records");
        std::puts(debug_capture_metadata_json(meta).c_str());
        for (const auto& record : page.records) {
            require(record.retired && !record.trap.valid && record.privilege == PrivilegeMode::User &&
                    record.satp == ((8ULL << 60) | (child_root >> 12)) && record.segment_count == 1,
                    "incorrect store association");
            for (unsigned i = 0; i < 2; ++i) {
                if (record.vaddr != va + i*4096) continue;
                const auto& segment = record.segments[0];
                require(segment.paddr == child_pa[i] && segment.bytes == 8 && segment.write &&
                        segment.outcome == MemoryOutcome::Success && segment.value == changed[i], "incorrect child store facts");
                ++stores[i];
            }
            std::puts(debug_memory_record_json(record).c_str());
        }
        require(stores[0] == 1 && stores[1] == 1, "missing child store");
        require(machine.cpu().core().cycle() == cycle, "export advanced guest time");
    }
    size_t until(size_t offset, const std::string& marker, U64 budget) {
        size_t last_size = 0;
        for (U64 i = 0; i < budget; ++i) {
            step();
            const auto& output = machine.uart().output();
            if (output.size() != last_size) {
                last_size = output.size();
                const auto found = output.find(marker, offset);
                if (found != std::string::npos) return found;
                require(output.find("EVIDENCE FAIL", offset) == std::string::npos, "guest assertion failed");
            }
            if (i % 1000000 == 0) {
                require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1200),
                        "1200 second wall budget exhausted");
                require(!machine.cpu().core().halted(), "machine halted");
            }
        }
        throw std::runtime_error("guest step budget exhausted waiting for " + marker);
    }
    size_t marker(size_t offset, const char* stage) {
        const std::string prefix = std::string("EVIDENCE ") + stage + " ";
        size_t pos = until(offset, prefix, 100000000);
        until(pos, "\n", 1000000);
        std::istringstream line(machine.uart().output().substr(pos));
        std::string tag, phase, address;
        U64 parent, child;
        require(bool(line >> tag >> phase >> parent >> child >> address), "bad guest marker");
        const U64 address_value = std::stoull(address, nullptr, 16);
        if (!child_pid) { parent_pid = parent; child_pid = child; va = address_value; }
        require(parent == parent_pid && child == child_pid && address_value == va,
                "guest identity changed");
        require(va % 4096 == 0, "fixture data is not page aligned");
        return machine.uart().output_size();
    }
    void capture(const char* phase, bool modified) {
        const U64 parent = find_proc(parent_pid), child = find_proc(child_pid);
        child_slot = child;
        require(read(child + at("parent")) == parent, "proc parent relation mismatch");
        const U64 parent_root = read(parent + at("pagetable"));
        child_root = read(child + at("pagetable"));
        require(parent_root != child_root, "parent and child share page table");
        require(current_proc() == child, "marker not emitted by child");
        for (unsigned i = 0; i < 2; ++i) {
            const auto p = translate(parent_root, va + i * 4096);
            const auto c = translate(child_root, va + i * 4096);
            if (capture_enabled) {
                require(translate(child_root, va + i*4096) == c, "repeated page query changed mapping");
                require(read(c.first) == read(c.first), "repeated memory query changed data");
            }
            require(p.first != c.first, "parent and child share physical data page");
            require(read(p.first) == initial[i], "parent data changed");
            require(read(c.first) == (modified ? changed[i] : initial[i]), "child data mismatch");
            if (modified) require(child_pa[i] == c.first, "child mapping changed after write");
            child_pa[i] = c.first;
            std::printf("{\"event\":\"mapping\",\"phase\":\"%s\",\"parent_pid\":%llu,\"child_pid\":%llu,"
                        "\"va\":%llu,\"parent_root\":%llu,\"child_root\":%llu,\"parent_pa\":%llu,"
                        "\"child_pa\":%llu,\"parent_pte\":%llu,\"child_pte\":%llu,"
                        "\"parent_value\":%llu,\"child_value\":%llu,\"cycle\":%llu,\"instret\":%llu}\n",
                        phase, (unsigned long long)parent_pid, (unsigned long long)child_pid,
                        (unsigned long long)(va + i * 4096), (unsigned long long)parent_root,
                        (unsigned long long)child_root, (unsigned long long)p.first,
                        (unsigned long long)c.first, (unsigned long long)p.second,
                        (unsigned long long)c.second, (unsigned long long)initial[i],
                        (unsigned long long)(modified ? changed[i] : initial[i]),
                        (unsigned long long)machine.cpu().core().cycle(),
                        (unsigned long long)machine.cpu().core().instret());
        }
        std::fflush(stdout);
    }
    void reclaimed() {
        require(zombie, "child zombie transition not observed");
        require(read(child_slot + at("state"), 4) == at("unused") &&
                read(child_slot + at("pid"), 4) == 0 &&
                read(child_slot + at("pagetable")) == 0, "child proc was not reclaimed");
        std::set<U64> free_pages;
        U64 next = read(at("kmem") + at("freelist"));
        while (next) {
            require(next % 4096 == 0 && free_pages.size() < 32768 && free_pages.insert(next).second,
                    "invalid or cyclic freelist");
            next = read(next + at("run_next"));
        }
        for (unsigned i = 0; i < 2; ++i) {
            if (capture_enabled) require(stores[i] == 1, "missing or repeated observed child store");
            require(free_pages.count(child_pa[i]), "child data page absent from freelist");
            std::printf("{\"event\":\"reclaimed\",\"pa\":%llu,\"freelist_member\":true,\"proc_unused\":true}\n",
                        (unsigned long long)child_pa[i]);
        }
    }
};
}

int main(int argc, char** argv) {
    try {
        require(argc == 5, "usage: fork-evidence kernel fs.img layout.txt capture|off");
        Probe p;
        require(std::string(argv[4]) == "capture" || std::string(argv[4]) == "off", "invalid capture mode");
        p.capture_enabled = std::string(argv[4]) == "capture";
        std::ifstream file(argv[3]);
        std::string key;
        U64 value;
        while (file >> key >> value) require(p.layout.emplace(key, value).second, "duplicate layout key");
        require(file.eof() && p.at("nproc") > 0, "invalid layout file");
        p.machine.set_backend_kind(BackendKind::Functional);
        p.machine.set_block_transport(BlockTransport::VirtioBlk);
        p.machine.load_elf(argv[1]);
        p.machine.attach_storage_image(argv[2]);
        p.machine.uart().set_mirror_stdout(false);
        p.until(0, "$ ", 500000000);
        size_t offset = p.machine.uart().output_size();
        p.machine.uart().inject_input("forkev\r");
        offset = p.marker(offset, "BEFORE");
        p.capture("before", false);
        p.start_window();
        offset = p.marker(offset, "AFTER");
        p.stop_window();
        p.capture("after", true);
        offset = p.marker(offset, "REAP");
        p.reclaimed();
        p.until(offset, "$ ", 20000000);
        std::puts(debug_protocol_uart_output_json({0, p.machine.uart().output_size(), p.machine.uart().output()}).c_str());
        offset = p.machine.uart().output_size();
        p.machine.uart().inject_input("forktest\r");
        p.until(offset, "fork test OK", 50000000);
        offset = p.machine.uart().output_size();
        p.until(offset, "$ ", 20000000);
        std::printf("{\"event\":\"result\",\"passed\":true,\"backend\":\"functional\","
                    "\"existing_forktest\":true,\"wall_seconds\":%.3f,\"manual_checks\":[],"
                    "\"scope\":\"standalone test probe; two touched user pages; ordinary fork\"}\n",
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - p.start).count());
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fork evidence failed: %s\n", ex.what());
        return 1;
    }
}

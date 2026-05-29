#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

#include "../../src/devices/plic.h"
#include "../../src/devices/virtio_blk.h"
#include "../../src/devices/virtio_mmio.h"
#include "../../src/mem/memory_region.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

struct PackedDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct PackedBlkReqHeader {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

class DmaFailWindow final : public Device {
public:
    DmaFailWindow(uint64_t base, size_t size, size_t fail_after_writes)
        : Device(base, size),
          bytes_(size, 0),
          fail_after_writes_(fail_after_writes) {}

    uint64_t load(uint64_t addr, int size) override {
        if (size != 1) {
            invalid_access(addr, size);
        }
        return bytes_.at(static_cast<size_t>(addr - base()));
    }

    void store(uint64_t addr, uint64_t value, int size) override {
        if (size != 1) {
            invalid_access(addr, size);
        }
        if (write_count_ >= fail_after_writes_) {
            throw std::runtime_error("forced virtio DMA write failure");
        }
        bytes_.at(static_cast<size_t>(addr - base())) =
            static_cast<uint8_t>(value & 0xffU);
        ++write_count_;
    }

    PhysicalRegionInfo region_info() const override {
        return {
            .kind = PhysicalRegionKind::Ram,
            .cacheable = false,
            .dma_visible = true,
            .has_side_effect = false,
            .supports_burst = true,
            .label = "virtio-dma-fail-window",
        };
    }

    const char* debug_name() const override {
        return "virtio_dma_fail_window";
    }

private:
    std::vector<uint8_t> bytes_{};
    size_t fail_after_writes_{0};
    size_t write_count_{0};
};

constexpr uint64_t kDescAddr = MEM_BASE + 0x1000;
constexpr uint64_t kAvailAddr = MEM_BASE + 0x2000;
constexpr uint64_t kUsedAddr = MEM_BASE + 0x3000;
constexpr uint64_t kHeaderAddr = MEM_BASE + 0x4000;
constexpr uint64_t kDataAddr = MEM_BASE + 0x5000;
constexpr uint64_t kStatusAddr = MEM_BASE + 0x6000;
constexpr uint64_t kDmaFailAddr = 0x90000000;

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

std::string create_temp_path(const char* prefix) {
    char path_template[64];
    std::snprintf(path_template, sizeof(path_template), "/tmp/%s_XXXXXX", prefix);
    const int fd = mkstemp(path_template);
    if (fd < 0) {
        throw std::runtime_error("failed to create temp virtio image");
    }
    close(fd);
    return path_template;
}

std::string write_temp_image() {
    const std::string path = create_temp_path("mycpu_virtio_img");
    std::array<uint8_t, VIRTIO_BLK_SECTOR_SIZE> bytes{};
    bytes[0] = 'S';
    bytes[1] = 't';
    bytes[2] = 'o';
    bytes[3] = 'r';
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("failed to open temp virtio image for writing");
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    return path;
}

void write_bytes(Ram& ram, uint64_t addr, const void* data, size_t size) {
    ram.write_bytes(addr, data, size);
}

void write_u16(Ram& ram, uint64_t addr, uint16_t value) {
    write_bytes(ram, addr, &value, sizeof(value));
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool store_reg(Bus& bus, uint32_t reg, uint32_t value, const char* message) {
    if (!bus.try_store(VIRTIO_MMIO_BASE + reg, value, 4)) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool load_reg(Bus& bus, uint32_t reg, uint32_t expected, const char* message) {
    uint64_t value = 0;
    if (!bus.try_load(VIRTIO_MMIO_BASE + reg, 4, value) || value != expected) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void program_queue(Ram& ram, Bus& bus) {
    const PackedDesc desc0{kHeaderAddr, sizeof(PackedBlkReqHeader), VIRTQ_DESC_F_NEXT, 1};
    const PackedDesc desc1{kDataAddr, VIRTIO_BLK_SECTOR_SIZE, VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE, 2};
    const PackedDesc desc2{kStatusAddr, 1, VIRTQ_DESC_F_WRITE, 0};
    write_bytes(ram, kDescAddr + 0 * sizeof(PackedDesc), &desc0, sizeof(desc0));
    write_bytes(ram, kDescAddr + 1 * sizeof(PackedDesc), &desc1, sizeof(desc1));
    write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &desc2, sizeof(desc2));
    write_u16(ram, kAvailAddr + 2, 1);
    write_u16(ram, kAvailAddr + 4, 0);

    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_SEL, 0, "queue select");
    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NUM, 8, "queue num");
    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_DESC_LOW, static_cast<uint32_t>(kDescAddr), "queue desc");
    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW, static_cast<uint32_t>(kAvailAddr), "queue driver");
    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW, static_cast<uint32_t>(kUsedAddr), "queue device");
    store_reg(bus, VIRTIO_MMIO_REG_QUEUE_READY, 1, "queue ready");
}

}  // namespace

int main() {
    try {
        const std::string image_path = write_temp_image();

        Ram ram;
        Bus bus(ram);
        Plic plic;
        VirtioBlk blk;
        blk.load_image(image_path.c_str());
        VirtioMmio mmio(plic, VIRTIO_MMIO_PLIC_SOURCE, blk);
        mmio.bind_bus(bus);

        bus.attach(mmio);
        bus.attach(plic);

        if (!bus.try_store(PLIC_BASE + PLIC_PRIORITY_OFFSET(VIRTIO_MMIO_PLIC_SOURCE), 1, 4) ||
            !bus.try_store(PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                           (1U << VIRTIO_MMIO_PLIC_SOURCE),
                           4) ||
            !bus.try_store(PLIC_BASE + PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR), 0, 4)) {
            std::remove(image_path.c_str());
            return fail("failed to configure PLIC");
        }

        if (!store_reg(bus,
                       VIRTIO_MMIO_REG_STATUS,
                       VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                           VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK,
                       "status")) {
            std::remove(image_path.c_str());
            return 1;
        }

        program_queue(ram, bus);

        PackedBlkReqHeader read_header{VIRTIO_BLK_T_IN, 0, 0};
        write_bytes(ram, kHeaderAddr, &read_header, sizeof(read_header));
        ram.fill(kDataAddr, 0, VIRTIO_BLK_SECTOR_SIZE);
        ram.fill(kStatusAddr, 0xFF, 1);

        if (!store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, "queue notify")) {
            std::remove(image_path.c_str());
            return 1;
        }

        uint64_t first = 0;
        uint64_t second = 0;
        uint64_t third = 0;
        uint64_t fourth = 0;
        uint64_t status = 0;
        uint64_t used_idx = 0;
        uint64_t claimed = 0;

        if (!expect(bus.try_load(kDataAddr + 0, 1, first) && first == 'S', "expected first disk byte") ||
            !expect(bus.try_load(kDataAddr + 1, 1, second) && second == 't', "expected second disk byte") ||
            !expect(bus.try_load(kDataAddr + 2, 1, third) && third == 'o', "expected third disk byte") ||
            !expect(bus.try_load(kDataAddr + 3, 1, fourth) && fourth == 'r', "expected fourth disk byte") ||
            !expect(bus.try_load(kStatusAddr, 1, status) && status == VIRTIO_BLK_S_OK,
                    "expected read request status ok") ||
            !expect(bus.try_load(kUsedAddr + 2, 2, used_idx) && used_idx == 1,
                    "expected used idx increment after read") ||
            !load_reg(bus,
                      VIRTIO_MMIO_REG_INTERRUPT_STATUS,
                      VIRTIO_MMIO_INTERRUPT_USED_BUFFER,
                      "expected used-buffer interrupt") ||
            !expect(plic.supervisor_has_pending(), "expected supervisor interrupt pending") ||
            !expect(bus.try_load(PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR), 4, claimed) &&
                        claimed == VIRTIO_MMIO_PLIC_SOURCE,
                    "expected supervisor claim to return virtio source")) {
            std::remove(image_path.c_str());
            return 1;
        }

        if (!store_reg(bus,
                       VIRTIO_MMIO_REG_INTERRUPT_ACK,
                       VIRTIO_MMIO_INTERRUPT_USED_BUFFER,
                       "interrupt ack") ||
            !bus.try_store(PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                           VIRTIO_MMIO_PLIC_SOURCE,
                           4) ||
            !load_reg(bus, VIRTIO_MMIO_REG_INTERRUPT_STATUS, 0, "expected interrupt clear") ||
            !expect(!plic.supervisor_has_pending(), "expected PLIC pending to clear after ack+complete")) {
            std::remove(image_path.c_str());
            return 1;
        }

        const PackedDesc write_desc0{kHeaderAddr, sizeof(PackedBlkReqHeader), VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc write_desc1{kDataAddr, VIRTIO_BLK_SECTOR_SIZE, VIRTQ_DESC_F_NEXT, 2};
        const PackedDesc write_desc2{kStatusAddr, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, kDescAddr + 0 * sizeof(PackedDesc), &write_desc0, sizeof(write_desc0));
        write_bytes(ram, kDescAddr + 1 * sizeof(PackedDesc), &write_desc1, sizeof(write_desc1));
        write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &write_desc2, sizeof(write_desc2));
        write_u16(ram, kAvailAddr + 2, 2);
        write_u16(ram, kAvailAddr + 6, 0);

        PackedBlkReqHeader write_header{VIRTIO_BLK_T_OUT, 0, 0};
        write_bytes(ram, kHeaderAddr, &write_header, sizeof(write_header));
        std::array<uint8_t, VIRTIO_BLK_SECTOR_SIZE> pattern{};
        pattern[0] = 'W';
        pattern[1] = 'r';
        pattern[2] = 'i';
        pattern[3] = 't';
        write_bytes(ram, kDataAddr, pattern.data(), pattern.size());
        ram.fill(kStatusAddr, 0xEE, 1);

        if (!store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, "queue notify write")) {
            std::remove(image_path.c_str());
            return 1;
        }

        if (!expect(bus.try_load(kStatusAddr, 1, status) && status == VIRTIO_BLK_S_OK,
                    "expected write request status ok")) {
            std::remove(image_path.c_str());
            return 1;
        }

                const PackedDesc reread_desc0{kHeaderAddr, sizeof(PackedBlkReqHeader), VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc reread_desc1{kDataAddr, VIRTIO_BLK_SECTOR_SIZE, VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE, 2};
        const PackedDesc reread_desc2{kStatusAddr, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, kDescAddr + 0 * sizeof(PackedDesc), &reread_desc0, sizeof(reread_desc0));
        write_bytes(ram, kDescAddr + 1 * sizeof(PackedDesc), &reread_desc1, sizeof(reread_desc1));
        write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &reread_desc2, sizeof(reread_desc2));
        write_u16(ram, kAvailAddr + 2, 3);
        write_u16(ram, kAvailAddr + 8, 0);
        write_bytes(ram, kHeaderAddr, &read_header, sizeof(read_header));
        ram.fill(kDataAddr, 0, VIRTIO_BLK_SECTOR_SIZE);
        ram.fill(kStatusAddr, 0xDD, 1);

        if (!store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, "queue notify reread") ||
            !expect(bus.try_load(kDataAddr + 0, 1, first) && first == 'W', "expected persisted first byte") ||
            !expect(bus.try_load(kDataAddr + 1, 1, second) && second == 'r', "expected persisted second byte") ||
            !expect(bus.try_load(kDataAddr + 2, 1, third) && third == 'i', "expected persisted third byte") ||
            !expect(bus.try_load(kDataAddr + 3, 1, fourth) && fourth == 't', "expected persisted fourth byte")) {
            std::remove(image_path.c_str());
            return 1;
        }

        DmaFailWindow dma_fail(kDmaFailAddr, VIRTIO_BLK_SECTOR_SIZE, 0);
        bus.attach(dma_fail);
        const PackedDesc dma_fault_desc0{kHeaderAddr, sizeof(PackedBlkReqHeader), VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc dma_fault_desc1{kDmaFailAddr,
                                         VIRTIO_BLK_SECTOR_SIZE,
                                         VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE,
                                         2};
        const PackedDesc dma_fault_desc2{kStatusAddr, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, kDescAddr + 0 * sizeof(PackedDesc), &dma_fault_desc0, sizeof(dma_fault_desc0));
        write_bytes(ram, kDescAddr + 1 * sizeof(PackedDesc), &dma_fault_desc1, sizeof(dma_fault_desc1));
        write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &dma_fault_desc2, sizeof(dma_fault_desc2));
        write_u16(ram, kAvailAddr + 2, 4);
        write_u16(ram, kAvailAddr + 10, 0);
        write_bytes(ram, kHeaderAddr, &read_header, sizeof(read_header));
        ram.fill(kStatusAddr, 0xCC, 1);

        if (!store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, "queue notify dma fault") ||
            !expect(bus.try_load(kStatusAddr, 1, status) && status == VIRTIO_BLK_S_IOERR,
                    "expected DMA read payload fault to write IOERR status") ||
            !expect(bus.try_load(kUsedAddr + 2, 2, used_idx) && used_idx == 4,
                    "expected used idx increment after DMA fault IOERR") ||
            !load_reg(bus,
                      VIRTIO_MMIO_REG_INTERRUPT_STATUS,
                      VIRTIO_MMIO_INTERRUPT_USED_BUFFER,
                      "expected interrupt after DMA fault IOERR")) {
            std::remove(image_path.c_str());
            return 1;
        }

        if (!store_reg(bus,
                       VIRTIO_MMIO_REG_INTERRUPT_ACK,
                       VIRTIO_MMIO_INTERRUPT_USED_BUFFER,
                       "interrupt ack after DMA fault")) {
            std::remove(image_path.c_str());
            return 1;
        }

        const PackedDesc bad_status_desc0{kHeaderAddr, sizeof(PackedBlkReqHeader), VIRTQ_DESC_F_NEXT, 1};
        const PackedDesc bad_status_desc1{kDataAddr, VIRTIO_BLK_SECTOR_SIZE, VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE, 2};
        const PackedDesc bad_status_desc2{UART_BASE, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, kDescAddr + 0 * sizeof(PackedDesc), &bad_status_desc0, sizeof(bad_status_desc0));
        write_bytes(ram, kDescAddr + 1 * sizeof(PackedDesc), &bad_status_desc1, sizeof(bad_status_desc1));
        write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &bad_status_desc2, sizeof(bad_status_desc2));
        write_u16(ram, kAvailAddr + 2, 5);
        write_u16(ram, kAvailAddr + 12, 0);
        write_bytes(ram, kHeaderAddr, &read_header, sizeof(read_header));

        if (bus.try_store(VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, 4)) {
            std::remove(image_path.c_str());
            return fail("expected bad status descriptor notify to fail");
        }
        if (!expect(bus.try_load(kUsedAddr + 2, 2, used_idx) && used_idx == 4,
                    "expected used idx unchanged when status writeback fails")) {
            std::remove(image_path.c_str());
            return 1;
        }
        const PackedDesc good_status_desc2{kStatusAddr, 1, VIRTQ_DESC_F_WRITE, 0};
        write_bytes(ram, kDescAddr + 2 * sizeof(PackedDesc), &good_status_desc2, sizeof(good_status_desc2));
        ram.fill(kStatusAddr, 0xBB, 1);
        if (!store_reg(bus, VIRTIO_MMIO_REG_QUEUE_NOTIFY, 0, "queue notify bad status retry") ||
            !expect(bus.try_load(kStatusAddr, 1, status) && status == VIRTIO_BLK_S_OK,
                    "expected repaired status descriptor to complete") ||
            !expect(bus.try_load(kUsedAddr + 2, 2, used_idx) && used_idx == 5,
                    "expected used idx increment after repaired status descriptor")) {
            std::remove(image_path.c_str());
            return 1;
        }

        std::remove(image_path.c_str());
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

#include <cstdio>
#include <cstring>
#include <exception>

#include "../../src/devices/clint.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/simple_storage.h"
#include "../../src/devices/uart16550.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        Plic plic;
        Uart16550 uart(plic);
        SimpleStorage storage;

        bus.attach(uart);
        bus.attach(clint);
        bus.attach(plic);
        bus.attach(storage);

        const auto ram_region = bus.describe_region(MEM_BASE, 4);
        if (ram_region.kind != PhysicalRegionKind::Ram || !ram_region.cacheable ||
            !ram_region.dma_visible || !ram_region.supports_burst || ram_region.has_side_effect ||
            std::strcmp(ram_region.label, "ram") != 0) {
            return fail("expected RAM region properties");
        }

        const auto uart_region = bus.describe_region(UART_BASE + UART_REG_THR, 1);
        if (uart_region.kind != PhysicalRegionKind::Mmio || uart_region.cacheable ||
            uart_region.dma_visible || uart_region.supports_burst ||
            !uart_region.has_side_effect || std::strcmp(uart_region.label, "uart") != 0) {
            return fail("expected UART MMIO region properties");
        }

        const auto clint_region = bus.describe_region(CLINT_BASE + CLINT_REG_MTIME, 8);
        if (clint_region.kind != PhysicalRegionKind::Mmio || clint_region.cacheable ||
            !clint_region.has_side_effect) {
            return fail("expected CLINT MMIO region properties");
        }

        const auto plic_region =
            bus.describe_region(PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE), 4);
        if (plic_region.kind != PhysicalRegionKind::Mmio || plic_region.cacheable ||
            !plic_region.has_side_effect) {
            return fail("expected PLIC MMIO region properties");
        }

        const auto storage_region = bus.describe_region(STORAGE_BASE + STORAGE_REG_MAGIC, 8);
        if (storage_region.kind != PhysicalRegionKind::Mmio || storage_region.cacheable ||
            !storage_region.has_side_effect) {
            return fail("expected storage MMIO region properties");
        }

        const auto unmapped_region = bus.describe_region(0x40000000, 4);
        if (unmapped_region.kind != PhysicalRegionKind::Unmapped || unmapped_region.cacheable ||
            unmapped_region.dma_visible || unmapped_region.supports_burst ||
            unmapped_region.has_side_effect || std::strcmp(unmapped_region.label, "unmapped") != 0) {
            return fail("expected unmapped region properties");
        }

        const auto ram_span = bus.describe_span(MEM_BASE + 0x100, 16);
        if (!ram_span.ok || ram_span.region.kind != PhysicalRegionKind::Ram) {
            return fail("expected RAM span query to succeed");
        }

        const auto uart_span = bus.describe_span(UART_BASE, UART_SIZE);
        if (!uart_span.ok || uart_span.region.kind != PhysicalRegionKind::Mmio) {
            return fail("expected UART span query to succeed");
        }

        const auto boundary_cross = bus.describe_span(UART_BASE + UART_SIZE - 1, 2);
        if (boundary_cross.ok) {
            return fail("expected span crossing device boundary to fail");
        }

        const auto unmapped_cross = bus.describe_span(MEM_BASE + MEM_SIZE - 1, 2);
        if (unmapped_cross.ok) {
            return fail("expected span crossing into unmapped space to fail");
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

#include <cstdio>
#include <exception>
#include <stdexcept>

#include "../../src/devices/simple_storage.h"
#include "../../src/platform/address_map.h"

namespace {

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}  // namespace

int main() {
    try {
        {
            SimpleStorage storage;

            storage.load_image("tests/data/storage_basic.txt");
            storage.set_ready(false);

            if (storage.load(STORAGE_BASE + STORAGE_REG_CAPACITY_BLOCKS, 8) == 0) {
                return fail("expected attached storage to keep non-zero capacity");
            }

            if (storage.load(STORAGE_BASE + STORAGE_REG_STATUS, 8) !=
                STORAGE_STATUS_ATTACHED) {
                return fail("expected attached-not-ready storage status");
            }

            storage.store(STORAGE_BASE + STORAGE_REG_LBA, 0, 8);
            storage.store(STORAGE_BASE + STORAGE_REG_BLOCK_COUNT, 1, 8);
            storage.store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_READ, 8);

            if ((storage.load(STORAGE_BASE + STORAGE_REG_STATUS, 8) &
                 STORAGE_STATUS_ERROR) == 0) {
                return fail("expected read against not-ready storage to latch error");
            }
            if (storage.load(STORAGE_BASE + STORAGE_REG_ERROR, 8) !=
                STORAGE_ERR_NOT_READY) {
                return fail("expected attached-not-ready read to report NOT_READY");
            }
        }

        {
            SimpleStorage storage;

            storage.load_image("tests/data/storage_basic.txt");
            storage.set_magic_valid(false);

            if (storage.load(STORAGE_BASE + STORAGE_REG_MAGIC, 8) != 0) {
                return fail("expected bad-magic storage to report invalid magic");
            }

            if (storage.load(STORAGE_BASE + STORAGE_REG_STATUS, 8) !=
                (STORAGE_STATUS_ATTACHED | STORAGE_STATUS_READY)) {
                return fail("expected bad-magic storage to stay attached and ready");
            }

            storage.store(STORAGE_BASE + STORAGE_REG_LBA, 0, 8);
            storage.store(STORAGE_BASE + STORAGE_REG_BLOCK_COUNT, 1, 8);
            storage.store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_READ, 8);

            if (storage.load(STORAGE_BASE + STORAGE_REG_ERROR, 8) !=
                STORAGE_ERR_NONE) {
                return fail("expected bad magic to affect probe only, not data path");
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}

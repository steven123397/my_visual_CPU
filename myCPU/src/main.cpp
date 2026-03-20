#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include "platform/address_map.h"
#include "platform/machine.h"

static void usage(const char* prog) {
    std::fprintf(stderr, "Usage: %s [-b addr] <image>\n", prog);
    std::fprintf(stderr, "  -b addr   load flat binary at hex address (default: 0x80000000)\n");
    std::fprintf(stderr, "  image     ELF or flat binary\n");
    std::exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
    }

    bool flat = false;
    uint64_t load_addr = MEM_BASE;
    const char* image = nullptr;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-b") == 0) {
            flat = true;
            if (++i >= argc) {
                usage(argv[0]);
            }
            load_addr = std::strtoull(argv[i], nullptr, 16);
        } else {
            image = argv[i];
        }
    }

    if (!image) {
        usage(argv[0]);
    }

    try {
        Machine machine;
        if (flat) {
            machine.load_binary(image, load_addr);
        } else {
            machine.load_elf(image);
        }
        machine.run();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }

    return 0;
}

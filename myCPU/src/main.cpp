#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "debug/debug_protocol.h"
#include "platform/address_map.h"
#include "platform/machine.h"

static void usage(const char* prog) {
    std::fprintf(stderr, "Usage: %s [--debug-cli] [-b addr] [-d image] [--backend kind] <image>\n", prog);
    std::fprintf(stderr, "  --debug-cli  run JSON line based debug session protocol on stdin/stdout\n");
    std::fprintf(stderr, "  -b addr   load flat binary at hex address (default: 0x80000000)\n");
    std::fprintf(stderr, "  -d image  attach host-backed storage image to the simple MMIO storage device\n");
    std::fprintf(stderr, "  --backend kind  select execution backend: functional or pipeline\n");
    std::fprintf(stderr, "  image     ELF or flat binary\n");
    std::exit(1);
}

static BackendKind parse_backend_kind(const char* name) {
    if (std::strcmp(name, "functional") == 0) {
        return BackendKind::Functional;
    }
    if (std::strcmp(name, "pipeline") == 0) {
        return BackendKind::Pipeline;
    }
    throw std::runtime_error(std::string("unknown backend: ") + name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
    }

    bool debug_cli = false;
    bool flat = false;
    uint64_t load_addr = MEM_BASE;
    const char* disk_image = nullptr;
    const char* backend_name = "functional";
    const char* image = nullptr;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-b") == 0) {
            flat = true;
            if (++i >= argc) {
                usage(argv[0]);
            }
            load_addr = std::strtoull(argv[i], nullptr, 16);
        } else if (std::strcmp(argv[i], "-d") == 0 || std::strcmp(argv[i], "--disk") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            disk_image = argv[i];
        } else if (std::strcmp(argv[i], "--backend") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            backend_name = argv[i];
        } else if (std::strcmp(argv[i], "--debug-cli") == 0) {
            debug_cli = true;
        } else {
            image = argv[i];
        }
    }

    if (debug_cli) {
        return run_debug_cli(std::cin, std::cout, std::cerr);
    }

    if (!image) {
        usage(argv[0]);
    }

    try {
        Machine machine;
        machine.set_backend_kind(parse_backend_kind(backend_name));
        if (disk_image) {
            machine.attach_storage_image(disk_image);
        }
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

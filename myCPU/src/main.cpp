#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>

#include "debug/debug_protocol.h"
#include "platform/address_map.h"
#include "platform/machine.h"

static BackendKind parse_backend_kind(const char* value) {
    if (std::strcmp(value, "functional") == 0) {
        return BackendKind::Functional;
    }
    if (std::strcmp(value, "pipeline") == 0) {
        return BackendKind::Pipeline;
    }
    throw std::runtime_error("unknown backend");
}

static void usage(const char* prog) {
    std::fprintf(stderr,
                 "Usage: %s [--debug-cli] [--backend kind] [-b addr] [-d image] [--disk-not-ready image] [--disk-bad-magic image] <image>\n",
                 prog);
    std::fprintf(stderr, "  --debug-cli     run JSON line debug protocol on stdin/stdout\n");
    std::fprintf(stderr, "  --backend kind  select execution backend: functional or pipeline\n");
    std::fprintf(stderr, "  -b addr   load flat binary at hex address (default: 0x80000000)\n");
    std::fprintf(stderr, "  -d image  attach host-backed storage image to the simple MMIO storage device\n");
    std::fprintf(stderr,
                 "  --disk-not-ready image  attach storage image but leave READY deasserted\n");
    std::fprintf(stderr,
                 "  --disk-bad-magic image  attach storage image but corrupt the probe MAGIC register\n");
    std::fprintf(stderr, "  image     ELF or flat binary\n");
    std::exit(1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
    }

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug-cli") == 0) {
            return run_debug_cli(std::cin, std::cout, std::cerr);
        }
    }

    bool flat = false;
    uint64_t load_addr = MEM_BASE;
    const char* disk_image = nullptr;
    bool disk_ready = true;
    bool disk_magic_valid = true;
    BackendKind backend_kind = BackendKind::Functional;
    const char* image = nullptr;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--backend") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            backend_kind = parse_backend_kind(argv[i]);
        } else if (std::strcmp(argv[i], "-b") == 0) {
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
            disk_ready = true;
            disk_magic_valid = true;
        } else if (std::strcmp(argv[i], "--disk-not-ready") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            disk_image = argv[i];
            disk_ready = false;
            disk_magic_valid = true;
        } else if (std::strcmp(argv[i], "--disk-bad-magic") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            disk_image = argv[i];
            disk_ready = true;
            disk_magic_valid = false;
        } else {
            image = argv[i];
        }
    }

    if (!image) {
        usage(argv[0]);
    }

    try {
        Machine machine;
        machine.set_backend_kind(backend_kind);
        if (disk_image) {
            machine.attach_storage_image(disk_image, disk_ready, disk_magic_valid);
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

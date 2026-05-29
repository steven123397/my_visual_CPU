#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <vector>

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
                 "Usage: %s [--debug-cli] [--backend kind] [--block-transport kind] [-b addr] [--payload image addr] [--set-reg reg value] [-d image|--disk image] [--disk-not-ready image] [--disk-bad-magic image] <image>\n",
                 prog);
    std::fprintf(stderr, "       %s --ai-profile-manifest manifest\n", prog);
    std::fprintf(stderr, "  --debug-cli     run JSON line debug protocol on stdin/stdout\n");
    std::fprintf(stderr, "  --ai-profile-manifest manifest  run a packaged AI accelerator profile workload\n");
    std::fprintf(stderr, "  --backend kind  select execution backend: functional or pipeline\n");
    std::fprintf(stderr,
                 "  --block-transport kind  select block transport: simple_storage or virtio-blk\n");
    std::fprintf(stderr, "  -b addr   load flat binary at hex address (default: 0x80000000)\n");
    std::fprintf(stderr, "  --payload image addr  load an extra flat payload into RAM after the main image\n");
    std::fprintf(stderr, "  --set-reg reg value  seed a general-purpose register after image load\n");
    std::fprintf(stderr,
                 "  -d, --disk image  attach host-backed storage image to the selected block transport\n");
    std::fprintf(stderr,
                 "  --disk-not-ready image  attach storage image but leave READY deasserted (simple_storage only)\n");
    std::fprintf(stderr,
                 "  --disk-bad-magic image  attach storage image but corrupt the probe MAGIC register (simple_storage only)\n");
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
    BlockTransport block_transport = BlockTransport::SimpleStorage;
    const char* ai_profile_manifest = nullptr;
    const char* image = nullptr;
    struct PayloadArg {
        const char* image;
        uint64_t addr;
    };
    struct GprSeedArg {
        const char* reg_name;
        uint64_t value;
    };
    std::vector<PayloadArg> payloads;
    std::vector<GprSeedArg> gpr_seeds;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--backend") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            backend_kind = parse_backend_kind(argv[i]);
        } else if (std::strcmp(argv[i], "--block-transport") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            block_transport = parse_block_transport(argv[i]);
        } else if (std::strcmp(argv[i], "-b") == 0) {
            flat = true;
            if (++i >= argc) {
                usage(argv[0]);
            }
            load_addr = std::strtoull(argv[i], nullptr, 16);
        } else if (std::strcmp(argv[i], "--payload") == 0) {
            if (i + 2 >= argc) {
                usage(argv[0]);
            }
            const char* payload_image = argv[++i];
            const uint64_t payload_addr = std::strtoull(argv[++i], nullptr, 16);
            payloads.push_back(PayloadArg{payload_image, payload_addr});
        } else if (std::strcmp(argv[i], "--set-reg") == 0) {
            if (i + 2 >= argc) {
                usage(argv[0]);
            }
            const char* reg_name = argv[++i];
            const uint64_t reg_value = std::strtoull(argv[++i], nullptr, 0);
            gpr_seeds.push_back(GprSeedArg{reg_name, reg_value});
        } else if (std::strcmp(argv[i], "--ai-profile-manifest") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
            }
            ai_profile_manifest = argv[i];
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

    if (ai_profile_manifest != nullptr) {
        if (image != nullptr || flat || disk_image != nullptr || !payloads.empty() || !gpr_seeds.empty()) {
            std::fprintf(stderr, "--ai-profile-manifest cannot be combined with guest image loading options\n");
            return 1;
        }

        try {
            Machine machine;
            machine.set_backend_kind(backend_kind);
            const Machine::AiProfileRunResult result =
                machine.run_ai_profile_manifest(ai_profile_manifest);
            const char* progress = "timeout";
            if (result.completed) {
                progress = result.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS ? "completed"
                                                                                        : "fault";
            }
            std::cout << "ai_profile"
                      << " name=" << result.workload_name
                      << " progress=" << progress
                      << " schema=ai_profile_v1"
                      << " baseline=none"
                      << " manifest=" << result.manifest_path
                      << " graph_package=" << result.graph_package_path
                      << " graph_package_bytes=" << result.graph_package_bytes
                      << " shape_mode=" << result.shape_mode
                      << " runtime_shapes=" << result.runtime_shapes
                      << " ticks=" << result.ticks
                      << " completion_status=" << result.completion_status
                      << " fault_code=" << result.fault_code
                      << " source_tag=" << result.source_tag
                      << " bytes_moved=" << result.bytes_moved
                      << " retired_ops=" << result.retired_ops
                      << " device_cycles=" << result.device_cycles
                      << " dma_cycles=" << result.dma_cycles
                      << " compute_cycles=" << result.compute_cycles
                      << " stall_cycles=" << result.stall_cycles
                      << " busy_cycles=" << result.busy_cycles
                      << " queue_cycles=" << result.queue_cycles
                      << " completion_cycles=" << result.completion_cycles
                      << " effective_ops_per_cycle=" << result.effective_ops_per_cycle
                      << " utilization=" << result.utilization
                      << '\n';
            std::cout << "ai_profile_aggregate"
                      << " tile_count=" << result.tile_count
                      << " scratchpad_peak_bytes=" << result.scratchpad_peak_bytes
                      << " op_count=" << result.op_summaries.size()
                      << '\n';
            for (const AiAcceleratorOpProfileSummary& summary : result.op_summaries) {
                std::cout << "ai_profile_op"
                          << " op_index=" << summary.op_index
                          << " opcode=" << ai_opcode_name(summary.opcode)
                          << " retired_ops=" << summary.retired_ops
                          << " compute_cycles=" << summary.compute_cycles
                          << " stall_cycles=" << summary.stall_cycles
                          << " tile_count=" << summary.tile_count
                          << '\n';
            }
            return result.completed &&
                           result.completion_status == AI_ACCEL_COMPLETION_STATUS_SUCCESS
                       ? 0
                       : 1;
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "%s\n", ex.what());
            return 1;
        }
    }

    if (!image) {
        usage(argv[0]);
    }

    try {
        Machine machine;
        machine.set_backend_kind(backend_kind);
        machine.set_block_transport(block_transport);
        if (disk_image) {
            machine.attach_storage_image(disk_image, disk_ready, disk_magic_valid);
        }
        if (flat) {
            machine.load_binary(image, load_addr);
        } else {
            machine.load_elf(image);
        }
        for (const PayloadArg& payload : payloads) {
            machine.load_binary_payload(payload.image, payload.addr);
        }
        for (const GprSeedArg& seed : gpr_seeds) {
            machine.set_gpr(seed.reg_name, seed.value);
        }
        machine.run();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "memory.h"

// Global memory pointer (used by cpu.c)
Memory *g_mem;

extern uint64_t elf_load(Memory *mem, const char *path);

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-b addr] <image>\n", prog);
    fprintf(stderr, "  -b addr   load flat binary at hex address (default: 0x80000000)\n");
    fprintf(stderr, "  image     ELF or flat binary\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) usage(argv[0]);

    int flat = 0;
    uint64_t load_addr = MEM_BASE;
    uint64_t entry = MEM_BASE;
    const char *image = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0) {
            flat = 1;
            if (++i >= argc) usage(argv[0]);
            load_addr = strtoull(argv[i], NULL, 16);
            entry = load_addr;
        } else {
            image = argv[i];
        }
    }
    if (!image) usage(argv[0]);

    g_mem = malloc(sizeof(Memory));
    mem_init(g_mem);

    if (flat) {
        mem_load_binary(g_mem, image, load_addr);
    } else {
        entry = elf_load(g_mem, image);
    }

    CPU cpu;
    cpu_init(&cpu, entry);

    // Simple run loop: stop on EBREAK to tohost (riscv-tests convention)
    // tohost is typically at a known symbol; we just run until halted
    while (!cpu.halted) {
        cpu_step(&cpu);
    }

    mem_free(g_mem);
    free(g_mem);
    return 0;
}

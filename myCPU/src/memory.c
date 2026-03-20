#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mem_init(Memory *mem) {
    mem->data = calloc(MEM_SIZE, 1);
    if (!mem->data) { perror("mem_init"); exit(1); }
}

void mem_free(Memory *mem) { free(mem->data); }

uint64_t mem_read(Memory *mem, uint64_t addr, int size) {
    if (addr < MEM_BASE || addr + size > MEM_BASE + MEM_SIZE) {
        fprintf(stderr, "mem_read: out of range 0x%lx\n", addr);
        return 0;
    }
    uint64_t off = addr - MEM_BASE;
    uint64_t val = 0;
    memcpy(&val, mem->data + off, size);
    return val;
}

void mem_write(Memory *mem, uint64_t addr, uint64_t val, int size) {
    if (addr < MEM_BASE || addr + size > MEM_BASE + MEM_SIZE) {
        fprintf(stderr, "mem_write: out of range 0x%lx\n", addr);
        return;
    }
    uint64_t off = addr - MEM_BASE;
    memcpy(mem->data + off, &val, size);
}

#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mem_range_valid(uint64_t addr, size_t size) {
    if (addr < MEM_BASE) {
        return 0;
    }

    const uint64_t offset = addr - MEM_BASE;
    if (offset > MEM_SIZE) {
        return 0;
    }

    return size <= (size_t)(MEM_SIZE - offset);
}

void mem_init(Memory *mem) {
    mem->data = calloc(MEM_SIZE, 1);
    if (!mem->data) { perror("mem_init"); exit(1); }
}

void mem_free(Memory *mem) { free(mem->data); }

uint64_t mem_read(Memory *mem, uint64_t addr, int size) {
    if (!mem_range_valid(addr, (size_t)size)) {
        fprintf(stderr, "mem_read: out of range 0x%lx\n", addr);
        return 0;
    }
    uint64_t off = addr - MEM_BASE;
    uint64_t val = 0;
    memcpy(&val, mem->data + off, size);
    return val;
}

void mem_write(Memory *mem, uint64_t addr, uint64_t val, int size) {
    if (!mem_range_valid(addr, (size_t)size)) {
        fprintf(stderr, "mem_write: out of range 0x%lx\n", addr);
        return;
    }
    uint64_t off = addr - MEM_BASE;
    memcpy(mem->data + off, &val, size);
}

void mem_write_range(Memory *mem, uint64_t addr, const void *data, size_t size) {
    if (!mem_range_valid(addr, size)) {
        fprintf(stderr, "mem_write_range: out of range 0x%lx\n", addr);
        exit(1);
    }

    memcpy(mem->data + (addr - MEM_BASE), data, size);
}

void mem_fill_range(Memory *mem, uint64_t addr, uint8_t value, size_t size) {
    if (!mem_range_valid(addr, size)) {
        fprintf(stderr, "mem_fill_range: out of range 0x%lx\n", addr);
        exit(1);
    }

    memset(mem->data + (addr - MEM_BASE), value, size);
}

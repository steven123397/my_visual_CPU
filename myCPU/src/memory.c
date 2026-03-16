#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// UART registers (16550 compatible, minimal)
#define UART_RBR 0  // RX buffer / TX holding
#define UART_LSR 5  // Line status
#define UART_LSR_THRE 0x20  // TX holding register empty

static uint64_t uart_read(uint32_t offset) {
    if (offset == UART_LSR) return UART_LSR_THRE;  // always ready
    return 0;
}

static void uart_write(uint32_t offset, uint8_t val) {
    if (offset == UART_RBR) { putchar(val); fflush(stdout); }
}

void mem_init(Memory *mem) {
    mem->data = calloc(MEM_SIZE, 1);
    if (!mem->data) { perror("mem_init"); exit(1); }
    mem->mtime = 0;
    mem->mtimecmp = UINT64_MAX;
}

void mem_free(Memory *mem) { free(mem->data); }

uint64_t mem_read(Memory *mem, uint64_t addr, int size) {
    if (addr >= UART_BASE && addr < UART_BASE + 8)
        return uart_read(addr - UART_BASE);

    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE) {
        uint64_t off = addr - CLINT_BASE;
        if (off == 0xBFF8) return mem->mtime;
        if (off == 0x4000) return mem->mtimecmp;
        return 0;
    }

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
    if (addr >= UART_BASE && addr < UART_BASE + 8) {
        uart_write(addr - UART_BASE, val & 0xFF);
        return;
    }

    if (addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE) {
        uint64_t off = addr - CLINT_BASE;
        if (off == 0xBFF8) mem->mtime = val;
        if (off == 0x4000) mem->mtimecmp = val;
        return;
    }

    if (addr < MEM_BASE || addr + size > MEM_BASE + MEM_SIZE) {
        fprintf(stderr, "mem_write: out of range 0x%lx\n", addr);
        return;
    }
    uint64_t off = addr - MEM_BASE;
    memcpy(mem->data + off, &val, size);
}

void mem_load_binary(Memory *mem, const char *path, uint64_t addr) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (addr < MEM_BASE || addr + sz > MEM_BASE + MEM_SIZE) {
        fprintf(stderr, "binary too large\n"); exit(1);
    }
    size_t nread = fread(mem->data + (addr - MEM_BASE), 1, sz, f);
    if (nread != (size_t)sz) {
        fprintf(stderr, "short read while loading binary: %s\n", path);
        exit(1);
    }
    fclose(f);
}

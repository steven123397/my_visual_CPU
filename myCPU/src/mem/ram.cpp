#include "ram.h"

Ram::Ram() : Device(MEM_BASE, MEM_SIZE) {
    mem_init(&mem_);
}

Ram::~Ram() {
    mem_free(&mem_);
}

uint64_t Ram::load(uint64_t addr, int size) {
    return mem_read(&mem_, addr, size);
}

void Ram::store(uint64_t addr, uint64_t value, int size) {
    mem_write(&mem_, addr, value, size);
}

void Ram::write_bytes(uint64_t addr, const void* data, size_t size) {
    mem_write_range(&mem_, addr, data, size);
}

void Ram::fill(uint64_t addr, uint8_t value, size_t size) {
    mem_fill_range(&mem_, addr, value, size);
}

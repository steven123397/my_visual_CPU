#include "ram.h"

Ram::Ram() {
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

void Ram::load_binary(const char* path, uint64_t addr) {
    mem_load_binary(&mem_, path, addr);
}

Memory* Ram::raw_memory() {
    return &mem_;
}

const Memory* Ram::raw_memory() const {
    return &mem_;
}

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

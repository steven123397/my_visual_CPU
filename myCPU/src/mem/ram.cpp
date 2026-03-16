#include "ram.h"

Ram::Ram() {
    mem_init(&mem_);
}

Ram::~Ram() {
    mem_free(&mem_);
}

Memory* Ram::raw() {
    return &mem_;
}

const Memory* Ram::raw() const {
    return &mem_;
}

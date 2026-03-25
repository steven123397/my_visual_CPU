#include "execution_context.h"

#include "../cpu.h"
#include "../mem/bus.h"

ExecutionContext::ExecutionContext(CPU& cpu, Bus& bus) : cpu_(cpu), bus_(bus) {}

CPU& ExecutionContext::cpu() {
    return cpu_;
}

CoreState& ExecutionContext::core() {
    return cpu_.core();
}

CsrFile& ExecutionContext::csr() {
    return cpu_.csr();
}

AddressSpace& ExecutionContext::address_space() {
    return cpu_.address_space();
}

Bus& ExecutionContext::bus() {
    return bus_;
}

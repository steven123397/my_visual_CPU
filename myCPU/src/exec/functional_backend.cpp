#include "functional_backend.h"

#include "../cpu.h"
#include "../mem/bus.h"

FunctionalBackend::FunctionalBackend(CPU& cpu, Bus& bus) : cpu_(cpu), bus_(bus) {}

void FunctionalBackend::step() {
    cpu_step(cpu_, bus_);
}

BackendDebugSnapshot FunctionalBackend::debug_snapshot() const {
    BackendDebugSnapshot snapshot;
    snapshot.backend_name = name();
    snapshot.pipeline.empty = true;
    return snapshot;
}

const char* FunctionalBackend::name() const {
    return "functional";
}

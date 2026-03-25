#pragma once

class CPU;
class Bus;
class CoreState;
class CsrFile;
class AddressSpace;

class ExecutionContext {
public:
    ExecutionContext(CPU& cpu, Bus& bus);

    CPU& cpu();
    CoreState& core();
    CsrFile& csr();
    AddressSpace& address_space();
    Bus& bus();

private:
    CPU& cpu_;
    Bus& bus_;
};

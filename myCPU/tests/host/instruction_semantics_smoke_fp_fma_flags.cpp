#include "instruction_semantics_smoke_common.inc"

int main() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, 0x80000000);
    ExecutionContext ctx(cpu, bus);

#include "instruction_semantics_smoke_fp_fma_flags.inc"

    return 0;
}

#include "instruction_semantics_smoke_common.inc"

int main() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, 0x80000000);
    cpu.csr().write(CSR_MSTATUS, MSTATUS_FS_INITIAL, cpu.core());
    ExecutionContext ctx(cpu, bus);

#include "instruction_semantics_smoke_fp_compare_convert.inc"

    return 0;
}

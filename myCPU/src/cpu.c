#include "cpu.h"
#include "memory.h"
#include "decode.h"
#include "trap.h"
#include <stdio.h>
#include <string.h>

#define CAUSE_ILLEGAL_INSN  2
#define CAUSE_BREAKPOINT    3
#define CAUSE_ECALL_M       11
#define CAUSE_TIMER_INT     (0x8000000000000007UL)

void cpu_init(CPU *cpu, uint64_t entry) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->pc = entry;
    cpu->csr[CSR_MISA] = (2UL << 62) | (1 << 8) | (1 << 12) | (1 << 0) | (1 << 2);
}

uint64_t csr_read(CPU *cpu, uint32_t addr) {
    if (addr == CSR_CYCLE || addr == CSR_TIME) return cpu->cycle;
    return cpu->csr[addr & 0xFFF];
}

void csr_write(CPU *cpu, uint32_t addr, uint64_t val) {
    addr &= 0xFFF;
    if (addr == 0xC00 || addr == 0xC01) return;
    cpu->csr[addr] = val;
}

static inline void set_rd(CPU *cpu, uint8_t rd, uint64_t val) {
    if (rd) cpu->x[rd] = val;
}

static void execute(CPU *cpu, Memory *mem, Insn *in) {
    uint64_t pc   = cpu->pc;
    uint64_t rs1v = cpu->x[in->rs1];
    uint64_t rs2v = cpu->x[in->rs2];
    int64_t  imm  = in->imm;
    uint64_t next_pc = pc + 4;

    switch (in->opcode) {
    case 0x37: set_rd(cpu, in->rd, (uint64_t)imm); break;
    case 0x17: set_rd(cpu, in->rd, pc + (uint64_t)imm); break;
    case 0x6F:
        set_rd(cpu, in->rd, next_pc);
        next_pc = pc + (uint64_t)imm;
        break;
    case 0x67:
        set_rd(cpu, in->rd, next_pc);
        next_pc = (rs1v + (uint64_t)imm) & ~1UL;
        break;
    case 0x63: {
        int taken = 0;
        switch (in->funct3) {
        case 0: taken = rs1v == rs2v; break;
        case 1: taken = rs1v != rs2v; break;
        case 4: taken = (int64_t)rs1v <  (int64_t)rs2v; break;
        case 5: taken = (int64_t)rs1v >= (int64_t)rs2v; break;
        case 6: taken = rs1v <  rs2v; break;
        case 7: taken = rs1v >= rs2v; break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        if (taken) next_pc = pc + (uint64_t)imm;
        break;
    }
    case 0x03: {
        uint64_t addr = rs1v + (uint64_t)imm;
        uint64_t val;
        switch (in->funct3) {
        case 0: val = (int64_t)(int8_t) mem_read(mem, addr, 1); break;
        case 1: val = (int64_t)(int16_t)mem_read(mem, addr, 2); break;
        case 2: val = (int64_t)(int32_t)mem_read(mem, addr, 4); break;
        case 3: val =                   mem_read(mem, addr, 8); break;
        case 4: val =          (uint8_t)mem_read(mem, addr, 1); break;
        case 5: val =         (uint16_t)mem_read(mem, addr, 2); break;
        case 6: val =         (uint32_t)mem_read(mem, addr, 4); break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x23: {
        uint64_t addr = rs1v + (uint64_t)imm;
        switch (in->funct3) {
        case 0: mem_write(mem, addr, rs2v, 1); break;
        case 1: mem_write(mem, addr, rs2v, 2); break;
        case 2: mem_write(mem, addr, rs2v, 4); break;
        case 3: mem_write(mem, addr, rs2v, 8); break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        break;
    }
    case 0x13: {
        uint64_t val;
        uint8_t shamt = in->rs2 | ((in->funct7 & 1) << 5);
        switch (in->funct3) {
        case 0: val = rs1v + (uint64_t)imm; break;
        case 1: val = rs1v << shamt; break;
        case 2: val = (int64_t)rs1v < imm; break;
        case 3: val = rs1v < (uint64_t)imm; break;
        case 4: val = rs1v ^ (uint64_t)imm; break;
        case 5: val = (in->funct7 & 0x20) ? (uint64_t)((int64_t)rs1v >> shamt) : rs1v >> shamt; break;
        case 6: val = rs1v | (uint64_t)imm; break;
        case 7: val = rs1v & (uint64_t)imm; break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x1B: {
        uint64_t val;
        uint8_t shamt = in->rs2;
        switch (in->funct3) {
        case 0: val = (int64_t)(int32_t)(rs1v + (uint64_t)imm); break;
        case 1: val = (int64_t)(int32_t)((uint32_t)rs1v << shamt); break;
        case 5: val = (in->funct7 & 0x20)
                    ? (int64_t)(int32_t)((int32_t)rs1v >> shamt)
                    : (int64_t)(int32_t)((uint32_t)rs1v >> shamt); break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x33: {
        uint64_t val;
        uint8_t shamt = rs2v & 63;
        if (in->funct7 == 1) {
            switch (in->funct3) {
            case 0: val = rs1v * rs2v; break;
            case 1: val = (uint64_t)((__int128)(int64_t)rs1v * (int64_t)rs2v >> 64); break;
            case 2: val = (uint64_t)((__int128)(int64_t)rs1v * (uint64_t)rs2v >> 64); break;
            case 3: val = (uint64_t)((__uint128_t)rs1v * rs2v >> 64); break;
            case 4: val = rs2v ? (uint64_t)((int64_t)rs1v / (int64_t)rs2v) : ~0UL; break;
            case 5: val = rs2v ? rs1v / rs2v : ~0UL; break;
            case 6: val = rs2v ? (uint64_t)((int64_t)rs1v % (int64_t)rs2v) : rs1v; break;
            case 7: val = rs2v ? rs1v % rs2v : rs1v; break;
            default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
            }
        } else {
            switch (in->funct3) {
            case 0: val = (in->funct7 & 0x20) ? rs1v - rs2v : rs1v + rs2v; break;
            case 1: val = rs1v << shamt; break;
            case 2: val = (int64_t)rs1v < (int64_t)rs2v; break;
            case 3: val = rs1v < rs2v; break;
            case 4: val = rs1v ^ rs2v; break;
            case 5: val = (in->funct7 & 0x20) ? (uint64_t)((int64_t)rs1v >> shamt) : rs1v >> shamt; break;
            case 6: val = rs1v | rs2v; break;
            case 7: val = rs1v & rs2v; break;
            default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
            }
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x3B: {
        uint64_t val;
        uint8_t shamt = rs2v & 31;
        if (in->funct7 == 1) {
            switch (in->funct3) {
            case 0: val = (int64_t)(int32_t)(rs1v * rs2v); break;
            case 4: val = rs2v ? (uint64_t)(int64_t)((int32_t)rs1v / (int32_t)rs2v) : UINT64_MAX; break;
            case 5: val = rs2v ? (uint64_t)(int64_t)(int32_t)((uint32_t)rs1v / (uint32_t)rs2v) : UINT64_MAX; break;
            case 6: val = rs2v ? (int64_t)((int32_t)rs1v % (int32_t)rs2v) : (int64_t)(int32_t)rs1v; break;
            case 7: val = rs2v ? (int64_t)(int32_t)((uint32_t)rs1v % (uint32_t)rs2v) : (int64_t)(int32_t)rs1v; break;
            default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
            }
        } else {
            switch (in->funct3) {
            case 0: val = (int64_t)(int32_t)((in->funct7 & 0x20) ? rs1v - rs2v : rs1v + rs2v); break;
            case 1: val = (int64_t)(int32_t)((uint32_t)rs1v << shamt); break;
            case 5: val = (in->funct7 & 0x20)
                        ? (int64_t)(int32_t)((int32_t)rs1v >> shamt)
                        : (int64_t)(int32_t)((uint32_t)rs1v >> shamt); break;
            default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
            }
        }
        set_rd(cpu, in->rd, val);
        break;
    }
    case 0x73: {
        uint32_t csr_addr = in->raw >> 20;
        uint64_t old;
        switch (in->funct3) {
        case 0:
            if (in->raw == 0x00000073) {
                // a7=93 (exit): halt simulator
                if (cpu->x[17] == 93) { cpu->halted = 1; return; }
                trap_enter(cpu, CAUSE_ECALL_M, 0); return;
            }
            if (in->raw == 0x00100073) { trap_enter(cpu, CAUSE_BREAKPOINT, pc); return; }
            if (in->raw == 0x30200073) { trap_return(cpu); return; }
            // WFI = NOP
            break;
        case 1: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, rs1v); break;
        case 2: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, old | rs1v); break;
        case 3: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, old & ~rs1v); break;
        case 5: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, in->rs1); break;
        case 6: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, old | in->rs1); break;
        case 7: old = csr_read(cpu, csr_addr); set_rd(cpu, in->rd, old); csr_write(cpu, csr_addr, old & ~(uint64_t)in->rs1); break;
        default: trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw); return;
        }
        break;
    }
    case 0x0F: break; // FENCE = NOP
    default:
        trap_enter(cpu, CAUSE_ILLEGAL_INSN, in->raw);
        return;
    }

    cpu->pc = next_pc;
}

static void check_interrupts(CPU *cpu) {
    if (!(csr_read(cpu, CSR_MSTATUS) & MSTATUS_MIE)) return;
    uint64_t mie = csr_read(cpu, CSR_MIE);
    uint64_t mip = csr_read(cpu, CSR_MIP);
    if ((mie & MIE_MTIE) && (mip & MIE_MTIE)) {
        csr_write(cpu, CSR_MIP, mip & ~MIE_MTIE);
        trap_enter(cpu, CAUSE_TIMER_INT, 0);
    }
}

void cpu_step(CPU *cpu, Memory *mem) {
    mem->mtime++;
    if (mem->mtime >= mem->mtimecmp) {
        csr_write(cpu, CSR_MIP, csr_read(cpu, CSR_MIP) | MIE_MTIE);
    }
    check_interrupts(cpu);

    uint32_t raw = (uint32_t)mem_read(mem, cpu->pc, 4);
    Insn insn;
    decode(raw, &insn);
    execute(cpu, mem, &insn);

    cpu->x[0] = 0;
    cpu->cycle++;
}

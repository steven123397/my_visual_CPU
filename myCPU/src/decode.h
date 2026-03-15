#pragma once
#include <stdint.h>

typedef struct {
    uint32_t raw;
    uint8_t opcode, rd, funct3, rs1, rs2, funct7;
    int64_t imm;
} Insn;

void decode(uint32_t raw, Insn *insn);

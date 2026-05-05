#include "decode.h"

static int64_t sign_extend(uint64_t value, unsigned bits) {
    return (int64_t)(value << (64U - bits)) >> (64U - bits);
}

static uint8_t compressed_reg(uint16_t raw, unsigned shift) {
    return (uint8_t)(8U + ((raw >> shift) & 0x7U));
}

static void clear_insn(Insn* insn, uint32_t raw, uint8_t size) {
    insn->raw = raw;
    insn->size = size;
    insn->opcode = 0;
    insn->rd = 0;
    insn->funct3 = 0;
    insn->rs1 = 0;
    insn->rs2 = 0;
    insn->funct7 = 0;
    insn->funct5 = 0;
    insn->aq = 0;
    insn->rl = 0;
    insn->imm = 0;
}

static void set_i_insn(Insn* insn,
                       uint32_t raw,
                       uint8_t rd,
                       uint8_t funct3,
                       uint8_t rs1,
                       int64_t imm,
                       uint8_t opcode) {
    clear_insn(insn, raw, 2);
    insn->opcode = opcode;
    insn->rd = rd;
    insn->funct3 = funct3;
    insn->rs1 = rs1;
    insn->imm = imm;
}

static void set_shift_i_insn(Insn* insn,
                             uint32_t raw,
                             uint8_t rd,
                             uint8_t funct3,
                             uint8_t rs1,
                             uint8_t shamt,
                             uint8_t funct7) {
    set_i_insn(insn, raw, rd, funct3, rs1, 0, 0x13);
    insn->rs2 = shamt & 0x1F;
    insn->funct7 = funct7;
}

static void set_r_insn(Insn* insn,
                       uint32_t raw,
                       uint8_t rd,
                       uint8_t funct3,
                       uint8_t rs1,
                       uint8_t rs2,
                       uint8_t funct7,
                       uint8_t opcode) {
    clear_insn(insn, raw, 2);
    insn->opcode = opcode;
    insn->rd = rd;
    insn->funct3 = funct3;
    insn->rs1 = rs1;
    insn->rs2 = rs2;
    insn->funct7 = funct7;
}

static void set_s_insn(Insn* insn,
                       uint32_t raw,
                       uint8_t funct3,
                       uint8_t rs1,
                       uint8_t rs2,
                       int64_t imm,
                       uint8_t opcode) {
    clear_insn(insn, raw, 2);
    insn->opcode = opcode;
    insn->funct3 = funct3;
    insn->rs1 = rs1;
    insn->rs2 = rs2;
    insn->imm = imm;
}

static void set_b_insn(Insn* insn,
                       uint32_t raw,
                       uint8_t funct3,
                       uint8_t rs1,
                       uint8_t rs2,
                       int64_t imm) {
    clear_insn(insn, raw, 2);
    insn->opcode = 0x63;
    insn->funct3 = funct3;
    insn->rs1 = rs1;
    insn->rs2 = rs2;
    insn->imm = imm;
}

static void set_u_insn(Insn* insn, uint32_t raw, uint8_t rd, int64_t imm, uint8_t opcode) {
    clear_insn(insn, raw, 2);
    insn->opcode = opcode;
    insn->rd = rd;
    insn->imm = imm;
}

static void set_j_insn(Insn* insn, uint32_t raw, uint8_t rd, int64_t imm) {
    clear_insn(insn, raw, 2);
    insn->opcode = 0x6F;
    insn->rd = rd;
    insn->imm = imm;
}

static void set_system_insn(Insn* insn, uint32_t raw, int64_t imm) {
    clear_insn(insn, raw, 2);
    insn->opcode = 0x73;
    insn->imm = imm;
}

static int64_t ci_immediate(uint16_t raw) {
    return sign_extend(((uint64_t)((raw >> 2) & 0x1FU)) | (((uint64_t)raw >> 12) & 0x1U) << 5, 6);
}

static uint8_t ci_shamt(uint16_t raw) {
    return (uint8_t)(((raw >> 2) & 0x1FU) | (((raw >> 12) & 0x1U) << 5));
}

static void decode_standard(uint32_t raw, Insn* insn) {
    clear_insn(insn, raw, 4);
    insn->opcode = raw & 0x7F;
    insn->rd = (raw >> 7)  & 0x1F;
    insn->funct3 = (raw >> 12) & 0x07;
    insn->rs1 = (raw >> 15) & 0x1F;
    insn->rs2 = (raw >> 20) & 0x1F;
    insn->funct7 = (raw >> 25) & 0x7F;
    insn->funct5 = (raw >> 27) & 0x1F;
    insn->aq = (raw >> 26) & 0x01;
    insn->rl = (raw >> 25) & 0x01;

    switch (insn->opcode) {
    case 0x03:
    case 0x07:
    case 0x13:
    case 0x1B:
    case 0x67:
    case 0x73:
        insn->imm = (int64_t)(int32_t)(raw & 0xFFF00000) >> 20;
        break;
    case 0x23:
    case 0x27:
        insn->imm = (int64_t)(int32_t)(
            ((raw & 0xFE000000)) | ((raw & 0xF80) << 13)) >> 20;
        break;
    case 0x63:
        insn->imm = (int64_t)(int32_t)(
            ((raw & 0x80000000)) |
            ((raw & 0x80) << 23) |
            ((raw & 0x7E000000) >> 1) |
            ((raw & 0xF00) << 12)) >> 19;
        break;
    case 0x37:
    case 0x17:
        insn->imm = (int64_t)(int32_t)(raw & 0xFFFFF000);
        break;
    case 0x6F:
        insn->imm =
            ((int64_t)(int32_t)(raw & 0x80000000) >> 11) |
            (int64_t)(raw & 0x000FF000) |
            ((int64_t)(raw & 0x00100000) >> 9) |
            ((int64_t)(raw & 0x7FE00000) >> 20);
        break;
    default:
        break;
    }
}

static void decode_compressed(uint16_t raw, Insn* insn) {
    clear_insn(insn, raw, 2);

    const uint8_t quadrant = raw & 0x3U;
    const uint8_t funct3 = (raw >> 13) & 0x7U;
    const uint8_t rd_rs1 = (raw >> 7) & 0x1FU;
    const uint8_t rs2 = (raw >> 2) & 0x1FU;

    switch (quadrant) {
    case 0:
        switch (funct3) {
        case 0: {
            const uint64_t imm =
                (((uint64_t)(raw >> 7) & 0xFU) << 6) |
                (((uint64_t)(raw >> 11) & 0x3U) << 4) |
                (((uint64_t)(raw >> 5) & 0x1U) << 3) |
                (((uint64_t)(raw >> 6) & 0x1U) << 2);
            if (imm != 0) {
                set_i_insn(insn, raw, compressed_reg(raw, 2), 0, 2, (int64_t)imm, 0x13);
            }
            return;
        }
        case 1: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6);
            set_i_insn(insn, raw, compressed_reg(raw, 2), 3, compressed_reg(raw, 7), (int64_t)imm, 0x07);
            return;
        }
        case 2: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 6) & 0x1U) << 2) |
                (((uint64_t)(raw >> 5) & 0x1U) << 6);
            set_i_insn(insn, raw, compressed_reg(raw, 2), 2, compressed_reg(raw, 7), (int64_t)imm, 0x03);
            return;
        }
        case 3: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6);
            set_i_insn(insn, raw, compressed_reg(raw, 2), 3, compressed_reg(raw, 7), (int64_t)imm, 0x03);
            return;
        }
        case 5: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6);
            set_s_insn(insn, raw, 3, compressed_reg(raw, 7), compressed_reg(raw, 2), (int64_t)imm, 0x27);
            return;
        }
        case 6: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 6) & 0x1U) << 2) |
                (((uint64_t)(raw >> 5) & 0x1U) << 6);
            set_s_insn(insn, raw, 2, compressed_reg(raw, 7), compressed_reg(raw, 2), (int64_t)imm, 0x23);
            return;
        }
        case 7: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6);
            set_s_insn(insn, raw, 3, compressed_reg(raw, 7), compressed_reg(raw, 2), (int64_t)imm, 0x23);
            return;
        }
        default:
            return;
        }
    case 1:
        switch (funct3) {
        case 0:
            set_i_insn(insn, raw, rd_rs1, 0, rd_rs1, ci_immediate(raw), 0x13);
            return;
        case 1:
            if (rd_rs1 != 0) {
                set_i_insn(insn, raw, rd_rs1, 0, rd_rs1, ci_immediate(raw), 0x1B);
            }
            return;
        case 2:
            set_i_insn(insn, raw, rd_rs1, 0, 0, ci_immediate(raw), 0x13);
            return;
        case 3:
            if (rd_rs1 == 2) {
                const uint64_t imm =
                    (((uint64_t)(raw >> 6) & 0x1U) << 4) |
                    (((uint64_t)(raw >> 2) & 0x1U) << 5) |
                    (((uint64_t)(raw >> 5) & 0x1U) << 6) |
                    (((uint64_t)(raw >> 3) & 0x3U) << 7) |
                    (((uint64_t)(raw >> 12) & 0x1U) << 9);
                if (imm != 0) {
                    set_i_insn(insn, raw, 2, 0, 2, sign_extend(imm, 10), 0x13);
                }
                return;
            }
            if (rd_rs1 != 0) {
                const int64_t imm = sign_extend((((uint64_t)(raw >> 2) & 0x1FU) |
                                                 (((uint64_t)raw >> 12) & 0x1U) << 5),
                                                6) << 12;
                if (imm != 0) {
                    set_u_insn(insn, raw, rd_rs1, imm, 0x37);
                }
            }
            return;
        case 4: {
            const uint8_t subop = (raw >> 10) & 0x3U;
            if (subop != 3) {
                if (subop == 0) {
                    set_shift_i_insn(insn,
                                     raw,
                                     compressed_reg(raw, 7),
                                     5,
                                     compressed_reg(raw, 7),
                                     ci_shamt(raw),
                                     (uint8_t)(ci_shamt(raw) >> 5));
                    return;
                }
                if (subop == 1) {
                    set_shift_i_insn(insn,
                                     raw,
                                     compressed_reg(raw, 7),
                                     5,
                                     compressed_reg(raw, 7),
                                     ci_shamt(raw),
                                     (uint8_t)(0x20U | (ci_shamt(raw) >> 5)));
                    return;
                }
                set_i_insn(insn, raw, compressed_reg(raw, 7), 7, compressed_reg(raw, 7), ci_immediate(raw), 0x13);
                return;
            }

            switch ((raw >> 5) & 0x3U) {
            case 0:
                if (((raw >> 12) & 0x1U) == 0) {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 0, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x20, 0x33);
                } else {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 0, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x20, 0x3B);
                }
                return;
            case 1:
                if (((raw >> 12) & 0x1U) == 0) {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 4, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x00, 0x33);
                } else {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 0, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x00, 0x3B);
                }
                return;
            case 2:
                if (((raw >> 12) & 0x1U) == 0) {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 6, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x00, 0x33);
                }
                return;
            case 3:
                if (((raw >> 12) & 0x1U) == 0) {
                    set_r_insn(insn, raw, compressed_reg(raw, 7), 7, compressed_reg(raw, 7), compressed_reg(raw, 2), 0x00, 0x33);
                }
                return;
            default:
                return;
            }
        }
        case 5: {
            const uint64_t imm =
                (((uint64_t)(raw >> 3) & 0x7U) << 1) |
                (((uint64_t)(raw >> 11) & 0x1U) << 4) |
                (((uint64_t)(raw >> 2) & 0x1U) << 5) |
                (((uint64_t)(raw >> 7) & 0x1U) << 6) |
                (((uint64_t)(raw >> 6) & 0x1U) << 7) |
                (((uint64_t)(raw >> 9) & 0x3U) << 8) |
                (((uint64_t)(raw >> 8) & 0x1U) << 10) |
                (((uint64_t)(raw >> 12) & 0x1U) << 11);
            set_j_insn(insn, raw, 0, sign_extend(imm, 12));
            return;
        }
        case 6: {
            const uint64_t imm =
                (((uint64_t)(raw >> 3) & 0x3U) << 1) |
                (((uint64_t)(raw >> 10) & 0x3U) << 3) |
                (((uint64_t)(raw >> 2) & 0x1U) << 5) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6) |
                (((uint64_t)(raw >> 12) & 0x1U) << 8);
            set_b_insn(insn, raw, 0, compressed_reg(raw, 7), 0, sign_extend(imm, 9));
            return;
        }
        case 7: {
            const uint64_t imm =
                (((uint64_t)(raw >> 3) & 0x3U) << 1) |
                (((uint64_t)(raw >> 10) & 0x3U) << 3) |
                (((uint64_t)(raw >> 2) & 0x1U) << 5) |
                (((uint64_t)(raw >> 5) & 0x3U) << 6) |
                (((uint64_t)(raw >> 12) & 0x1U) << 8);
            set_b_insn(insn, raw, 1, compressed_reg(raw, 7), 0, sign_extend(imm, 9));
            return;
        }
        default:
            return;
        }
    case 2:
        switch (funct3) {
        case 0:
            set_shift_i_insn(insn,
                             raw,
                             rd_rs1,
                             1,
                             rd_rs1,
                             ci_shamt(raw),
                             (uint8_t)(ci_shamt(raw) >> 5));
            return;
        case 1: {
            if (rd_rs1 == 0) {
                return;
            }
            const uint64_t imm =
                (((uint64_t)(raw >> 5) & 0x3U) << 3) |
                (((uint64_t)(raw >> 12) & 0x1U) << 5) |
                (((uint64_t)(raw >> 2) & 0x7U) << 6);
            set_i_insn(insn, raw, rd_rs1, 3, 2, (int64_t)imm, 0x07);
            return;
        }
        case 2: {
            if (rd_rs1 == 0) {
                return;
            }
            const uint64_t imm =
                (((uint64_t)(raw >> 4) & 0x7U) << 2) |
                (((uint64_t)(raw >> 12) & 0x1U) << 5) |
                (((uint64_t)(raw >> 2) & 0x3U) << 6);
            set_i_insn(insn, raw, rd_rs1, 2, 2, (int64_t)imm, 0x03);
            return;
        }
        case 3: {
            if (rd_rs1 == 0) {
                return;
            }
            const uint64_t imm =
                (((uint64_t)(raw >> 5) & 0x3U) << 3) |
                (((uint64_t)(raw >> 12) & 0x1U) << 5) |
                (((uint64_t)(raw >> 2) & 0x7U) << 6);
            set_i_insn(insn, raw, rd_rs1, 3, 2, (int64_t)imm, 0x03);
            return;
        }
        case 4:
            if (((raw >> 12) & 0x1U) == 0) {
                if (rs2 == 0) {
                    if (rd_rs1 != 0) {
                        set_i_insn(insn, raw, 0, 0, rd_rs1, 0, 0x67);
                    }
                } else {
                    set_r_insn(insn, raw, rd_rs1, 0, 0, rs2, 0x00, 0x33);
                }
                return;
            }
            if (rs2 == 0) {
                if (rd_rs1 == 0) {
                    set_system_insn(insn, raw, 1);
                } else {
                    set_i_insn(insn, raw, 1, 0, rd_rs1, 0, 0x67);
                }
            } else {
                set_r_insn(insn, raw, rd_rs1, 0, rd_rs1, rs2, 0x00, 0x33);
            }
            return;
        case 5: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 7) & 0x7U) << 6);
            set_s_insn(insn, raw, 3, 2, rs2, (int64_t)imm, 0x27);
            return;
        }
        case 6: {
            const uint64_t imm =
                (((uint64_t)(raw >> 9) & 0xFU) << 2) |
                (((uint64_t)(raw >> 7) & 0x3U) << 6);
            set_s_insn(insn, raw, 2, 2, rs2, (int64_t)imm, 0x23);
            return;
        }
        case 7: {
            const uint64_t imm =
                (((uint64_t)(raw >> 10) & 0x7U) << 3) |
                (((uint64_t)(raw >> 7) & 0x7U) << 6);
            set_s_insn(insn, raw, 3, 2, rs2, (int64_t)imm, 0x23);
            return;
        }
        default:
            return;
        }
    default:
        return;
    }
}

void decode(uint32_t raw, Insn *insn) {
    if ((raw & 0x3U) != 0x3U) {
        decode_compressed((uint16_t)raw, insn);
        return;
    }
    decode_standard(raw, insn);
}

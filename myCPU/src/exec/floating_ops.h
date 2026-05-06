#pragma once

#include <cstdint>

struct InsnEffects;

extern "C" {
#include "../decode.h"
}

bool is_fmv_d_x(const Insn& insn);
bool is_fmv_x_d(const Insn& insn);
bool is_fmv_w_x(const Insn& insn);
bool is_fmv_x_w(const Insn& insn);
bool is_fmv_d(const Insn& insn);
bool is_fneg_d(const Insn& insn);
bool is_fsgnj_d(const Insn& insn);
bool is_fsgnjn_d(const Insn& insn);
bool is_fsgnjx_d(const Insn& insn);
bool is_fsgnj_s(const Insn& insn);
bool is_fsgnjn_s(const Insn& insn);
bool is_fsgnjx_s(const Insn& insn);
bool is_fadd_s(const Insn& insn);
bool is_fsub_s(const Insn& insn);
bool is_fmul_s(const Insn& insn);
bool is_fdiv_s(const Insn& insn);
bool is_fadd_d(const Insn& insn);
bool is_fsub_d(const Insn& insn);
bool is_fmul_d(const Insn& insn);
bool is_fdiv_d(const Insn& insn);
bool is_fmax_d(const Insn& insn);
bool is_fmin_d(const Insn& insn);
bool is_fmax_s(const Insn& insn);
bool is_fmin_s(const Insn& insn);
bool is_fmadd_s(const Insn& insn);
bool is_fmsub_s(const Insn& insn);
bool is_fnmsub_s(const Insn& insn);
bool is_fnmadd_s(const Insn& insn);
bool is_fmadd_d(const Insn& insn);
bool is_fmsub_d(const Insn& insn);
bool is_fnmsub_d(const Insn& insn);
bool is_fnmadd_d(const Insn& insn);
bool is_fsqrt_s(const Insn& insn);
bool is_fsqrt_d(const Insn& insn);
bool is_fcvt_w_d(const Insn& insn);
bool is_fcvt_wu_d(const Insn& insn);
bool is_fcvt_l_d(const Insn& insn);
bool is_fcvt_lu_d(const Insn& insn);
bool is_fcvt_d_w(const Insn& insn);
bool is_fcvt_d_wu(const Insn& insn);
bool is_fcvt_d_l(const Insn& insn);
bool is_fcvt_d_lu(const Insn& insn);
bool is_fcvt_w_s(const Insn& insn);
bool is_fcvt_wu_s(const Insn& insn);
bool is_fcvt_l_s(const Insn& insn);
bool is_fcvt_lu_s(const Insn& insn);
bool is_fcvt_s_w(const Insn& insn);
bool is_fcvt_s_wu(const Insn& insn);
bool is_fcvt_s_l(const Insn& insn);
bool is_fcvt_s_lu(const Insn& insn);
bool is_fcvt_d_s(const Insn& insn);
bool is_fcvt_s_d(const Insn& insn);
bool is_fclass_s(const Insn& insn);
bool is_fclass_d(const Insn& insn);
bool is_feq_s(const Insn& insn);
bool is_flt_s(const Insn& insn);
bool is_fle_s(const Insn& insn);
bool is_feq_d(const Insn& insn);
bool is_flt_d(const Insn& insn);
bool is_fle_d(const Insn& insn);
bool floating_rs1_from_fpr(const Insn& insn);
bool floating_rs2_from_fpr(const Insn& insn);
bool floating_rs3_from_fpr(const Insn& insn);
InsnEffects build_floating_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t rs3v, uint64_t fcsr);

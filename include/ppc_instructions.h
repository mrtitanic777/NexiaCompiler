#pragma once
// NexiaCompiler v2.0 — PPC Instruction Encoder
// Ported from PpcInstructions.cs
// Encodes PowerPC instructions for Xbox 360 (Xenon).

#include <cstdint>

namespace nexia {

enum class PpcRegister : uint8_t {
    R0 = 0,  R1,  R2,  R3,  R4,  R5,  R6,  R7,
    R8,  R9,  R10, R11, R12, R13, R14, R15,
    R16, R17, R18, R19, R20, R21, R22, R23,
    R24, R25, R26, R27, R28, R29, R30, R31,
    SP = 1,   // Stack Pointer = R1
    TOC = 2,  // Table of Contents = R2
    LR_SAVE = 0 // R0 used for LR save/restore
};

enum class PpcFloatRegister : uint8_t {
    F0 = 0,  F1,  F2,  F3,  F4,  F5,  F6,  F7,
    F8,  F9,  F10, F11, F12, F13, F14, F15,
    F16, F17, F18, F19, F20, F21, F22, F23,
    F24, F25, F26, F27, F28, F29, F30, F31
};

enum class PpcCondition : uint8_t {
    LT = 0, GT = 1, EQ = 2, SO = 3, // CR bit indices
    GE = 4, LE = 5, NE = 6, NS = 7  // Inverted
};

namespace ppc {

// Integer arithmetic
uint32_t addi(PpcRegister rd, PpcRegister ra, int16_t imm);
uint32_t addis(PpcRegister rd, PpcRegister ra, int16_t imm);
uint32_t add(PpcRegister rd, PpcRegister ra, PpcRegister rb);
uint32_t sub(PpcRegister rd, PpcRegister ra, PpcRegister rb);
uint32_t mullw(PpcRegister rd, PpcRegister ra, PpcRegister rb);
uint32_t divw(PpcRegister rd, PpcRegister ra, PpcRegister rb);
uint32_t divwu(PpcRegister rd, PpcRegister ra, PpcRegister rb);
uint32_t neg(PpcRegister rd, PpcRegister ra);

// Logical
uint32_t and_op(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t or_op(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t xor_op(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t nor(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t andi(PpcRegister ra, PpcRegister rs, uint16_t imm);
uint32_t ori(PpcRegister ra, PpcRegister rs, uint16_t imm);
uint32_t xori(PpcRegister ra, PpcRegister rs, uint16_t imm);

// Shifts
uint32_t slw(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t srw(PpcRegister ra, PpcRegister rs, PpcRegister rb);
uint32_t sraw(PpcRegister ra, PpcRegister rs, PpcRegister rb);

// Compare
uint32_t cmpw(int cr, PpcRegister ra, PpcRegister rb);
uint32_t cmplw(int cr, PpcRegister ra, PpcRegister rb);
uint32_t cmpwi(int cr, PpcRegister ra, int16_t imm);

// Integer load/store
uint32_t lwz(PpcRegister rd, int16_t offset, PpcRegister ra);
uint32_t lbz(PpcRegister rd, int16_t offset, PpcRegister ra);
uint32_t lhz(PpcRegister rd, int16_t offset, PpcRegister ra);
uint32_t stw(PpcRegister rs, int16_t offset, PpcRegister ra);
uint32_t stb(PpcRegister rs, int16_t offset, PpcRegister ra);
uint32_t sth(PpcRegister rs, int16_t offset, PpcRegister ra);
uint32_t stwu(PpcRegister rs, int16_t offset, PpcRegister ra);

// Float load/store
uint32_t lfs(PpcFloatRegister fd, int16_t offset, PpcRegister ra);
uint32_t lfd(PpcFloatRegister fd, int16_t offset, PpcRegister ra);
uint32_t stfs(PpcFloatRegister fs, int16_t offset, PpcRegister ra);
uint32_t stfd(PpcFloatRegister fs, int16_t offset, PpcRegister ra);

// Float arithmetic
uint32_t fadd(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fadds(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fsub(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fsubs(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fmul(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fc);
uint32_t fmuls(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fc);
uint32_t fdiv(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fdivs(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb);
uint32_t fneg(PpcFloatRegister fd, PpcFloatRegister fb);
uint32_t fmr(PpcFloatRegister fd, PpcFloatRegister fb);
uint32_t fcmpu(int cr, PpcFloatRegister fa, PpcFloatRegister fb);

// Conversion
uint32_t fctiwz(PpcFloatRegister fd, PpcFloatRegister fb);

// Branch
uint32_t b(int32_t offset);
uint32_t bl(int32_t offset);
uint32_t bc(int bo, int bi, int16_t offset);
uint32_t bclr(int bo, int bi);
uint32_t bctr();
uint32_t bctrl();
uint32_t blr();

// Convenience conditional branches
uint32_t beq(int16_t offset, int cr = 0);
uint32_t bne(int16_t offset, int cr = 0);
uint32_t blt(int16_t offset, int cr = 0);
uint32_t bgt(int16_t offset, int cr = 0);
uint32_t ble(int16_t offset, int cr = 0);
uint32_t bge(int16_t offset, int cr = 0);

// System
uint32_t mflr(PpcRegister rd);
uint32_t mtlr(PpcRegister rs);
uint32_t mfctr(PpcRegister rd);
uint32_t mtctr(PpcRegister rs);
uint32_t mr(PpcRegister rd, PpcRegister rs);
uint32_t li(PpcRegister rd, int16_t imm);
uint32_t lis(PpcRegister rd, int16_t imm);
uint32_t nop();
uint32_t sc();

} // namespace ppc
} // namespace nexia

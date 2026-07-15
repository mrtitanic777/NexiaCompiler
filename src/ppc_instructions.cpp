// NexiaCompiler v2.0 — PPC Instruction Encoder
// Ported from PpcInstructions.cs

#include "ppc_instructions.h"

namespace nexia {
namespace ppc {

static uint32_t encode_d(int opcode, int rd, int ra, int16_t imm) {
    return ((uint32_t)opcode << 26) | ((uint32_t)(rd & 0x1F) << 21) |
           ((uint32_t)(ra & 0x1F) << 16) | ((uint32_t)(uint16_t)imm);
}
static uint32_t encode_x(int opcode, int rs, int ra, int rb, int xo, int rc) {
    return ((uint32_t)opcode << 26) | ((uint32_t)(rs & 0x1F) << 21) |
           ((uint32_t)(ra & 0x1F) << 16) | ((uint32_t)(rb & 0x1F) << 11) |
           ((uint32_t)(xo & 0x3FF) << 1) | ((uint32_t)(rc & 1));
}
static uint32_t encode_xo(int opcode, int rd, int ra, int rb, int oe, int xo, int rc) {
    return ((uint32_t)opcode << 26) | ((uint32_t)(rd & 0x1F) << 21) |
           ((uint32_t)(ra & 0x1F) << 16) | ((uint32_t)(rb & 0x1F) << 11) |
           ((uint32_t)(oe & 1) << 10) | ((uint32_t)(xo & 0x1FF) << 1) | ((uint32_t)(rc & 1));
}
static uint32_t encode_a(int opcode, int fd, int fa, int fb, int fc, int xo, int rc) {
    return ((uint32_t)opcode << 26) | ((uint32_t)(fd & 0x1F) << 21) |
           ((uint32_t)(fa & 0x1F) << 16) | ((uint32_t)(fb & 0x1F) << 11) |
           ((uint32_t)(fc & 0x1F) << 6) | ((uint32_t)(xo & 0x1F) << 1) | ((uint32_t)(rc & 1));
}

uint32_t addi(PpcRegister rd, PpcRegister ra, int16_t imm) { return encode_d(14, (int)rd, (int)ra, imm); }
uint32_t addis(PpcRegister rd, PpcRegister ra, int16_t imm) { return encode_d(15, (int)rd, (int)ra, imm); }
uint32_t add(PpcRegister rd, PpcRegister ra, PpcRegister rb) { return encode_xo(31, (int)rd, (int)ra, (int)rb, 0, 266, 0); }
uint32_t sub(PpcRegister rd, PpcRegister ra, PpcRegister rb) { return encode_xo(31, (int)rd, (int)rb, (int)ra, 0, 40, 0); }
uint32_t mullw(PpcRegister rd, PpcRegister ra, PpcRegister rb) { return encode_xo(31, (int)rd, (int)ra, (int)rb, 0, 235, 0); }
uint32_t divw(PpcRegister rd, PpcRegister ra, PpcRegister rb) { return encode_xo(31, (int)rd, (int)ra, (int)rb, 0, 491, 0); }
uint32_t divwu(PpcRegister rd, PpcRegister ra, PpcRegister rb) { return encode_xo(31, (int)rd, (int)ra, (int)rb, 0, 459, 0); }
uint32_t neg(PpcRegister rd, PpcRegister ra) { return encode_xo(31, (int)rd, (int)ra, 0, 0, 104, 0); }
uint32_t and_op(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 28, 0); }
uint32_t or_op(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 444, 0); }
uint32_t xor_op(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 316, 0); }
uint32_t nor(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 124, 0); }
uint32_t andi(PpcRegister ra, PpcRegister rs, uint16_t imm) { return encode_d(28, (int)rs, (int)ra, (int16_t)imm); }
uint32_t ori(PpcRegister ra, PpcRegister rs, uint16_t imm) { return encode_d(24, (int)rs, (int)ra, (int16_t)imm); }
uint32_t xori(PpcRegister ra, PpcRegister rs, uint16_t imm) { return encode_d(26, (int)rs, (int)ra, (int16_t)imm); }
uint32_t slw(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 24, 0); }
uint32_t srw(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 536, 0); }
uint32_t sraw(PpcRegister ra, PpcRegister rs, PpcRegister rb) { return encode_x(31, (int)rs, (int)ra, (int)rb, 792, 0); }
uint32_t cmpw(int cr, PpcRegister ra, PpcRegister rb) { return encode_x(31, (cr & 7) << 2, (int)ra, (int)rb, 0, 0); }
uint32_t cmplw(int cr, PpcRegister ra, PpcRegister rb) { return encode_x(31, (cr & 7) << 2, (int)ra, (int)rb, 32, 0); }
uint32_t cmpwi(int cr, PpcRegister ra, int16_t imm) { return encode_d(11, (cr & 7) << 2, (int)ra, imm); }
uint32_t lwz(PpcRegister rd, int16_t offset, PpcRegister ra) { return encode_d(32, (int)rd, (int)ra, offset); }
uint32_t lbz(PpcRegister rd, int16_t offset, PpcRegister ra) { return encode_d(34, (int)rd, (int)ra, offset); }
uint32_t lhz(PpcRegister rd, int16_t offset, PpcRegister ra) { return encode_d(40, (int)rd, (int)ra, offset); }
uint32_t stw(PpcRegister rs, int16_t offset, PpcRegister ra) { return encode_d(36, (int)rs, (int)ra, offset); }
uint32_t stb(PpcRegister rs, int16_t offset, PpcRegister ra) { return encode_d(38, (int)rs, (int)ra, offset); }
uint32_t sth(PpcRegister rs, int16_t offset, PpcRegister ra) { return encode_d(44, (int)rs, (int)ra, offset); }
uint32_t stwu(PpcRegister rs, int16_t offset, PpcRegister ra) { return encode_d(37, (int)rs, (int)ra, offset); }
uint32_t lfs(PpcFloatRegister fd, int16_t offset, PpcRegister ra) { return encode_d(48, (int)fd, (int)ra, offset); }
uint32_t lfd(PpcFloatRegister fd, int16_t offset, PpcRegister ra) { return encode_d(50, (int)fd, (int)ra, offset); }
uint32_t stfs(PpcFloatRegister fs, int16_t offset, PpcRegister ra) { return encode_d(52, (int)fs, (int)ra, offset); }
uint32_t stfd(PpcFloatRegister fs, int16_t offset, PpcRegister ra) { return encode_d(54, (int)fs, (int)ra, offset); }
uint32_t fadd(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(63, (int)fd, (int)fa, (int)fb, 0, 21, 0); }
uint32_t fadds(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(59, (int)fd, (int)fa, (int)fb, 0, 21, 0); }
uint32_t fsub(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(63, (int)fd, (int)fa, (int)fb, 0, 20, 0); }
uint32_t fsubs(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(59, (int)fd, (int)fa, (int)fb, 0, 20, 0); }
uint32_t fmul(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fc) { return encode_a(63, (int)fd, (int)fa, 0, (int)fc, 25, 0); }
uint32_t fmuls(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fc) { return encode_a(59, (int)fd, (int)fa, 0, (int)fc, 25, 0); }
uint32_t fdiv(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(63, (int)fd, (int)fa, (int)fb, 0, 18, 0); }
uint32_t fdivs(PpcFloatRegister fd, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_a(59, (int)fd, (int)fa, (int)fb, 0, 18, 0); }
uint32_t fneg(PpcFloatRegister fd, PpcFloatRegister fb) { return encode_x(63, (int)fd, 0, (int)fb, 40, 0); }
uint32_t fmr(PpcFloatRegister fd, PpcFloatRegister fb) { return encode_x(63, (int)fd, 0, (int)fb, 72, 0); }
uint32_t fcmpu(int cr, PpcFloatRegister fa, PpcFloatRegister fb) { return encode_x(63, (cr & 7) << 2, (int)fa, (int)fb, 0, 0); }
uint32_t fctiwz(PpcFloatRegister fd, PpcFloatRegister fb) { return encode_x(63, (int)fd, 0, (int)fb, 15, 0); }
uint32_t b(int32_t offset) { return (18u << 26) | ((uint32_t)(offset & 0x03FFFFFC)); }
uint32_t bl(int32_t offset) { return (18u << 26) | ((uint32_t)(offset & 0x03FFFFFC)) | 1; }
uint32_t bc(int bo, int bi, int16_t offset) { return (16u << 26) | ((uint32_t)(bo & 0x1F) << 21) | ((uint32_t)(bi & 0x1F) << 16) | ((uint32_t)(uint16_t)offset & 0xFFFC); }
uint32_t bclr(int bo, int bi) { return encode_x(19, bo & 0x1F, bi & 0x1F, 0, 16, 0); }
uint32_t bctr() { return encode_x(19, 20, 0, 0, 528, 0); }
uint32_t bctrl() { return encode_x(19, 20, 0, 0, 528, 1); }
uint32_t blr() { return bclr(20, 0); }
uint32_t beq(int16_t offset, int cr) { return bc(12, cr * 4 + 2, offset); }
uint32_t bne(int16_t offset, int cr) { return bc(4,  cr * 4 + 2, offset); }
uint32_t blt(int16_t offset, int cr) { return bc(12, cr * 4 + 0, offset); }
uint32_t bgt(int16_t offset, int cr) { return bc(12, cr * 4 + 1, offset); }
uint32_t ble(int16_t offset, int cr) { return bc(4,  cr * 4 + 1, offset); }
uint32_t bge(int16_t offset, int cr) { return bc(4,  cr * 4 + 0, offset); }
uint32_t mflr(PpcRegister rd) { return encode_x(31, (int)rd, 8, 0, 339, 0); }
uint32_t mtlr(PpcRegister rs) { return encode_x(31, (int)rs, 8, 0, 467, 0); }
uint32_t mfctr(PpcRegister rd) { return encode_x(31, (int)rd, 9, 0, 339, 0); }
uint32_t mtctr(PpcRegister rs) { return encode_x(31, (int)rs, 9, 0, 467, 0); }
uint32_t mr(PpcRegister rd, PpcRegister rs) { return or_op(rd, rs, rs); }
uint32_t li(PpcRegister rd, int16_t imm) { return addi(rd, PpcRegister::R0, imm); }
uint32_t lis(PpcRegister rd, int16_t imm) { return addis(rd, PpcRegister::R0, imm); }
uint32_t nop() { return ori(PpcRegister::R0, PpcRegister::R0, 0); }
uint32_t sc() { return (17u << 26) | 2; }

} // namespace ppc
} // namespace nexia

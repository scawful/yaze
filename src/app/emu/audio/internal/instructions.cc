#include "app/emu/audio/spc700.h"
#include "util/log.h"

namespace yaze {
namespace emu {

// ===========================================================================
// CYCLE-ACCURATE ATOMIC INSTRUCTION IMPLEMENTATIONS
// ===========================================================================
// All instructions are now fully atomic (no bstep mechanism).
// Cycle counts match Anomie's SPC700 reference and nesdev.org timing table.
// Memory-targeting MOV instructions include dummy read cycles as documented.
// ===========================================================================

// ---------------------------------------------------------------------------
// MOV Instructions (Load from memory to register)
// ---------------------------------------------------------------------------

void Spc700::MOV(uint16_t adr) {
  // MOV A, (adr) - Read from memory to A
  A = read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80);
}

void Spc700::MOVX(uint16_t adr) {
  // MOV X, (adr) - Read from memory to X
  X = read(adr);
  PSW.Z = (X == 0);
  PSW.N = (X & 0x80);
}

void Spc700::MOVY(uint16_t adr) {
  // MOV Y, (adr) - Read from memory to Y
  Y = read(adr);
  PSW.Z = (Y == 0);
  PSW.N = (Y & 0x80);
}

// ---------------------------------------------------------------------------
// MOV Instructions (Store from register to memory)
// ---------------------------------------------------------------------------
// Note: Per Anomie's doc, these include a dummy read cycle before writing

void Spc700::MOVS(uint16_t address) {
  // MOV (address), A - Write A to memory (with dummy read)
  read(address);  // Dummy read (documented behavior)
  write(address, A);
}

void Spc700::MOVSX(uint16_t address) {
  // MOV (address), X - Write X to memory (with dummy read)
  read(address);  // Dummy read (documented behavior)
  write(address, X);
}

void Spc700::MOVSY(uint16_t address) {
  // MOV (address), Y - Write Y to memory (with dummy read)
  read(address);  // Dummy read (documented behavior)
  write(address, Y);
}

void Spc700::MOV_ADDR(uint16_t address, uint8_t operand) {
  // MOV (address), #imm - Write immediate to memory (with dummy read)
  read(address);  // Dummy read (documented behavior)
  write(address, operand);
}

// ---------------------------------------------------------------------------
// Arithmetic Instructions (ADC, SBC)
// ---------------------------------------------------------------------------

void Spc700::ADC(uint16_t adr) {
  // ADC A, (adr) - Add with carry
  uint8_t value = read(adr);
  uint16_t result = A + value + PSW.C;
  PSW.V = ((A ^ result) & (value ^ result) & 0x80) != 0;
  PSW.H = ((A & 0xf) + (value & 0xf) + PSW.C) > 0xf;
  PSW.C = (result > 0xFF);
  A = result & 0xFF;
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80) != 0;
}

void Spc700::ADCM(uint16_t& dest, uint8_t operand) {
  // ADC (dest), operand - Add with carry to memory
  uint8_t applyOn = read(dest);
  int result = applyOn + operand + PSW.C;
  PSW.V = ((applyOn & 0x80) == (operand & 0x80)) &&
          ((operand & 0x80) != (result & 0x80));
  PSW.H = ((applyOn & 0xf) + (operand & 0xf) + PSW.C) > 0xf;
  PSW.C = result > 0xff;
  write(dest, result & 0xFF);
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::SBC(uint16_t adr) {
  // SBC A, (adr) - Subtract with carry (borrow)
  uint8_t value = read(adr) ^ 0xff;
  int result = A + value + PSW.C;
  PSW.V = ((A & 0x80) == (value & 0x80)) && ((value & 0x80) != (result & 0x80));
  PSW.H = ((A & 0xf) + (value & 0xf) + PSW.C) > 0xf;
  PSW.C = result > 0xff;
  A = result & 0xFF;
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80) != 0;
}

void Spc700::SBCM(uint16_t& dest, uint8_t operand) {
  // SBC (dest), operand - Subtract with carry from memory
  operand ^= 0xff;
  uint8_t applyOn = read(dest);
  int result = applyOn + operand + PSW.C;
  PSW.V = ((applyOn & 0x80) == (operand & 0x80)) &&
          ((operand & 0x80) != (result & 0x80));
  PSW.H = ((applyOn & 0xF) + (operand & 0xF) + PSW.C) > 0xF;
  PSW.C = result > 0xFF;
  write(dest, result & 0xFF);
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// Comparison Instructions (CMP, CMPX, CMPY, CMPM)
// ---------------------------------------------------------------------------

void Spc700::CMP(uint16_t adr) {
  // CMP A, (adr) - Compare A with memory
  uint8_t value = read(adr) ^ 0xff;
  int result = A + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::CMPX(uint16_t adr) {
  // CMP X, (adr) - Compare X with memory
  uint8_t value = read(adr) ^ 0xff;
  int result = X + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::CMPY(uint16_t adr) {
  // CMP Y, (adr) - Compare Y with memory
  uint8_t value = read(adr) ^ 0xff;
  int result = Y + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::CMPM(uint16_t dst, uint8_t value) {
  // CMP (dst), value - Compare memory with value
  value ^= 0xff;
  int result = read(dst) + value + 1;
  PSW.C = result > 0xff;
  callbacks_.idle(false);  // Extra cycle for memory comparison
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// Logical Instructions (AND, OR, EOR)
// ---------------------------------------------------------------------------

void Spc700::AND(uint16_t adr) {
  // AND A, (adr) - Logical AND with memory
  A &= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80) != 0;
}

void Spc700::ANDM(uint16_t dest, uint8_t operand) {
  // AND (dest), operand - Logical AND memory with value
  uint8_t result = read(dest) & operand;
  write(dest, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::OR(uint16_t adr) {
  // OR A, (adr) - Logical OR with memory
  A |= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80) != 0;
}

void Spc700::ORM(uint16_t dst, uint8_t value) {
  // OR (dst), value - Logical OR memory with value
  uint8_t result = read(dst) | value;
  write(dst, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80) != 0;
}

void Spc700::EOR(uint16_t adr) {
  // EOR A, (adr) - Logical XOR with memory
  A ^= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80) != 0;
}

void Spc700::EORM(uint16_t dest, uint8_t operand) {
  // EOR (dest), operand - Logical XOR memory with value
  uint8_t result = read(dest) ^ operand;
  write(dest, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// Shift and Rotate Instructions (ASL, LSR, ROL, ROR)
// ---------------------------------------------------------------------------

void Spc700::ASL(uint16_t adr) {
  // ASL (adr) - Arithmetic shift left
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  PSW.C = (val & 0x80) != 0;
  val <<= 1;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

void Spc700::LSR(uint16_t adr) {
  // LSR (adr) - Logical shift right
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  PSW.C = (val & 0x01) != 0;
  val >>= 1;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

void Spc700::ROL(uint16_t adr) {
  // ROL (adr) - Rotate left through carry
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  bool newC = (val & 0x80) != 0;
  val = (val << 1) | PSW.C;
  PSW.C = newC;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

void Spc700::ROR(uint16_t adr) {
  // ROR (adr) - Rotate right through carry
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  bool newC = (val & 1) != 0;
  val = (val >> 1) | (PSW.C << 7);
  PSW.C = newC;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// Increment/Decrement Instructions (INC, DEC)
// ---------------------------------------------------------------------------

void Spc700::INC(uint16_t adr) {
  // INC (adr) - Increment memory
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  val++;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

void Spc700::DEC(uint16_t adr) {
  // DEC (adr) - Decrement memory
  uint8_t val = read(adr);
  write(adr, val);  // Dummy write (RMW instruction)
  val--;
  write(adr, val);  // Actual write
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80) != 0;
}

void Spc700::XCN(uint8_t operand, bool isImmediate) {
  // XCN - Exchange nibbles (note: this is only used for A register)
  uint8_t value = isImmediate ? imm() : operand;
  value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
  PSW.Z = (value == 0);
  PSW.N = (value & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// 16-bit Instructions (MOVW, INCW, DECW, ADDW, SUBW, CMPW)
// ---------------------------------------------------------------------------

void Spc700::MOVW(uint16_t& dest, uint16_t operand) {
  // MOVW - Move 16-bit word
  dest = operand;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000) != 0;
}

void Spc700::INCW(uint16_t& operand) {
  // INCW - Increment 16-bit word
  operand++;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000) != 0;
}

void Spc700::DECW(uint16_t& operand) {
  // DECW - Decrement 16-bit word
  operand--;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000) != 0;
}

void Spc700::ADDW(uint16_t& dest, uint16_t operand) {
  // ADDW - Add 16-bit word
  uint32_t result = dest + operand;
  PSW.C = (result > 0xFFFF);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000) != 0;
  PSW.V = ((dest ^ result) & (operand ^ result) & 0x8000) != 0;
  PSW.H = ((dest & 0xfff) + (operand & 0xfff)) > 0xfff;
  dest = result & 0xFFFF;
}

void Spc700::SUBW(uint16_t& dest, uint16_t operand) {
  // SUBW - Subtract 16-bit word
  uint32_t result = dest - operand;
  PSW.C = (result <= 0xFFFF);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000) != 0;
  PSW.V = ((dest ^ result) & (dest ^ operand) & 0x8000) != 0;
  PSW.H = ((dest & 0xfff) - (operand & 0xfff)) >= 0;
  dest = result & 0xFFFF;
}

void Spc700::CMPW(uint16_t operand) {
  // CMPW - Compare 16-bit word with YA
  uint32_t result = YA - operand;
  PSW.C = (result <= 0xFFFF);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000) != 0;
}

// ---------------------------------------------------------------------------
// Multiply/Divide Instructions
// ---------------------------------------------------------------------------

void Spc700::MUL(uint8_t operand) {
  // MUL - Multiply A * Y -> YA
  uint16_t result = A * operand;
  YA = result;
  PSW.Z = (result == 0);
  PSW.N = (result & 0x8000) != 0;
}

void Spc700::DIV(uint8_t operand) {
  // DIV - Divide YA / X -> A (quotient), Y (remainder)
  // Note: Hardware behavior is complex; simplified here
  if (operand == 0) {
    // Divide by zero - undefined behavior
    // Real hardware has specific behavior, but we'll just return
    return;
  }
  uint8_t quotient = A / operand;
  uint8_t remainder = A % operand;
  A = quotient;
  Y = remainder;
  PSW.Z = (quotient == 0);
  PSW.N = (quotient & 0x80) != 0;
}

// ---------------------------------------------------------------------------
// Branch Instructions
// ---------------------------------------------------------------------------
// Note: Branch timing is handled in DoBranch() in spc700.cc
// These helpers are only used by old code paths

void Spc700::BRA(int8_t offset) {
  PC += offset;
}
void Spc700::BEQ(int8_t offset) {
  if (PSW.Z)
    PC += offset;
}
void Spc700::BNE(int8_t offset) {
  if (!PSW.Z)
    PC += offset;
}
void Spc700::BCS(int8_t offset) {
  if (PSW.C)
    PC += offset;
}
void Spc700::BCC(int8_t offset) {
  if (!PSW.C)
    PC += offset;
}
void Spc700::BVS(int8_t offset) {
  if (PSW.V)
    PC += offset;
}
void Spc700::BVC(int8_t offset) {
  if (!PSW.V)
    PC += offset;
}
void Spc700::BMI(int8_t offset) {
  if (PSW.N)
    PC += offset;
}
void Spc700::BPL(int8_t offset) {
  if (!PSW.N)
    PC += offset;
}

void Spc700::BBS(uint8_t bit, uint8_t operand) {
  if (operand & (1 << bit))
    PC += rel();
}

void Spc700::BBC(uint8_t bit, uint8_t operand) {
  if (!(operand & (1 << bit)))
    PC += rel();
}

// ---------------------------------------------------------------------------
// Jump and Call Instructions
// ---------------------------------------------------------------------------

void Spc700::JMP(uint16_t address) {
  PC = address;
}

void Spc700::CALL(uint16_t address) {
  uint16_t return_address = PC + 2;
  push_byte((return_address >> 8) & 0xFF);
  push_byte(return_address & 0xFF);
  PC = address;
}

void Spc700::PCALL(uint8_t offset) {
  uint16_t return_address = PC + 2;
  push_byte((return_address >> 8) & 0xFF);
  push_byte(return_address & 0xFF);
  PC += offset;
}

void Spc700::TCALL(uint8_t offset) {
  uint16_t return_address = PC + 2;
  push_byte((return_address >> 8) & 0xFF);
  push_byte(return_address & 0xFF);
  PC = 0xFFDE + offset;
}

void Spc700::BRK() {
  push_word(PC);
  push_byte(FlagsToByte(PSW));
  PSW.I = false;
  PSW.B = true;
  PC = read_word(0xFFDE);
}

void Spc700::RET() {
  PC = pull_word();
}

void Spc700::RETI() {
  PSW = ByteToFlags(pull_byte());
  PC = pull_word();
}

// ---------------------------------------------------------------------------
// Stack Instructions
// ---------------------------------------------------------------------------

void Spc700::PUSH(uint8_t operand) {
  push_byte(operand);
}

void Spc700::POP(uint8_t& operand) {
  operand = pull_byte();
}

// ---------------------------------------------------------------------------
// Bit Manipulation Instructions
// ---------------------------------------------------------------------------

void Spc700::SET1(uint8_t bit, uint8_t& operand) {
  operand |= (1 << bit);
}

void Spc700::CLR1(uint8_t bit, uint8_t& operand) {
  operand &= ~(1 << bit);
}

void Spc700::TSET1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit)) != 0;
  operand |= (1 << bit);
}

void Spc700::TCLR1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit)) != 0;
  operand &= ~(1 << bit);
}

void Spc700::AND1(uint8_t bit, uint8_t& operand) {
  operand &= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80) != 0;
}

void Spc700::OR1(uint8_t bit, uint8_t& operand) {
  operand |= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80) != 0;
}

void Spc700::EOR1(uint8_t bit, uint8_t& operand) {
  operand ^= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80) != 0;
}

void Spc700::NOT1(uint8_t bit, uint8_t& operand) {
  operand ^= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80) != 0;
}

void Spc700::MOV1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit)) != 0;
  operand |= (1 << bit);
}

// ---------------------------------------------------------------------------
// Flag Instructions
// ---------------------------------------------------------------------------

void Spc700::CLRC() {
  PSW.C = false;
}
void Spc700::SETC() {
  PSW.C = true;
}
void Spc700::NOTC() {
  PSW.C = !PSW.C;
}
void Spc700::CLRV() {
  PSW.V = false;
  PSW.H = false;
}
void Spc700::CLRP() {
  PSW.P = false;
}
void Spc700::SETP() {
  PSW.P = true;
}
void Spc700::EI() {
  PSW.I = true;
}
void Spc700::DI() {
  PSW.I = false;
}

// ---------------------------------------------------------------------------
// Special Instructions
// ---------------------------------------------------------------------------

void Spc700::NOP() {
  // No operation - PC already advanced by ReadOpcode()
}

void Spc700::SLEEP() {
  // Sleep mode - handled in ExecuteInstructions
}

void Spc700::STOP() {
  // Stop mode - handled in ExecuteInstructions
}

}  // namespace emu
}  // namespace yaze

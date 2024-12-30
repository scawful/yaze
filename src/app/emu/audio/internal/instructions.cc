#include "app/emu/audio/spc700.h"

namespace yaze {
namespace emu {

// opcode functions

void Spc700::MOVX(uint16_t adr) {
  X = read(adr);
  PSW.Z = (X == 0);
  PSW.N = (X & 0x80);
}

void Spc700::MOVY(uint16_t adr) {
  Y = read(adr);
  PSW.Z = (Y == 0);
  PSW.N = (Y & 0x80);
}

void Spc700::MOVS(uint16_t adr) {
  switch (bstep) {
    case 0: read(adr); break;
    case 1: write(adr, A); bstep = 0; break;
  }
}

void Spc700::MOVSX(uint16_t adr) {
  switch (bstep) {
    case 0: read(adr); break;
    case 1: write(adr, X); bstep = 0; break;
  }
}

void Spc700::MOVSY(uint16_t adr) {
  switch (bstep) {
    case 0: read(adr); break;
    case 1: write(adr, Y); bstep = 0; break;
  }
}

void Spc700::MOV(uint16_t adr) {
  A = read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80);
}

void Spc700::MOV_ADDR(uint16_t address, uint8_t operand) {
  write(address, operand);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::ADC(uint16_t adr) {
  uint8_t value = read(adr);
  uint16_t result = A + value + PSW.C;
  PSW.V = ((A ^ result) & (adr ^ result) & 0x80);
  PSW.C = (result > 0xFF);
  PSW.H = ((A ^ adr ^ result) & 0x10);
  A = result & 0xFF;
  PSW.Z = ((A & 0xFF) == 0);
  PSW.N = (A & 0x80);
}

void Spc700::ADCM(uint16_t& dest, uint8_t operand) {
  uint8_t applyOn = read(dest);
  int result = applyOn + operand + PSW.C;
  PSW.V = (applyOn & 0x80) == (operand & 0x80) &&
          (operand & 0x80) != (result & 0x80);
  PSW.H = ((applyOn & 0xf) + (operand & 0xf) + PSW.C) > 0xf;
  PSW.C = result > 0xff;
  write(dest, result);
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80);
}

void Spc700::SBC(uint16_t adr) {
  uint8_t value = read(adr) ^ 0xff;
  int result = A + value + PSW.C;
  PSW.V = (A & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
  PSW.H = ((A & 0xf) + (value & 0xf) + PSW.C) > 0xf;
  PSW.C = result > 0xff;
  A = result;
  PSW.Z = ((A & 0xFF) == 0);
  PSW.N = (A & 0x80);
}

void Spc700::SBCM(uint16_t& dest, uint8_t operand) {
  operand ^= 0xff;
  uint8_t applyOn = read(dest);
  int result = applyOn + operand + PSW.C;
  PSW.V = (applyOn & 0x80) == (operand & 0x80) &&
          (operand & 0x80) != (operand & 0x80);
  PSW.H = ((applyOn & 0xF) + (operand & 0xF) + PSW.C) > 0xF;
  PSW.C = result > 0xFF;
  write(dest, result);
  PSW.Z = ((A & 0xFF) == 0);
  PSW.N = (A & 0x80);
}

void Spc700::CMPX(uint16_t adr) {
  uint8_t value = read(adr) ^ 0xff;
  int result = X + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::CMPY(uint16_t adr) {
  uint8_t value = read(adr) ^ 0xff;
  int result = Y + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::CMPM(uint16_t dst, uint8_t value) {
  value ^= 0xff;
  int result = read(dst) + value + 1;
  PSW.C = result > 0xff;
  callbacks_.idle(false);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::CMP(uint16_t adr) {
  uint8_t value = read(adr) ^ 0xff;
  int result = A + value + 1;
  PSW.C = result > 0xff;
  PSW.Z = ((result & 0xFF) == 0);
  PSW.N = (result & 0x80);
}

void Spc700::AND(uint16_t adr) {
  A &= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80);
}

void Spc700::ANDM(uint16_t dest, uint8_t operand) {
  uint8_t result = read(dest) & operand;
  write(dest, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::OR(uint16_t adr) {
  A |= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80);
}

void Spc700::ORM(uint16_t dst, uint8_t value) {
  uint8_t result = read(dst) | value;
  write(dst, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::EOR(uint16_t adr) {
  A ^= read(adr);
  PSW.Z = (A == 0);
  PSW.N = (A & 0x80);
}

void Spc700::EORM(uint16_t dest, uint8_t operand) {
  uint8_t result = read(dest) ^ operand;
  write(dest, result);
  PSW.Z = (result == 0);
  PSW.N = (result & 0x80);
}

void Spc700::ASL(uint16_t operand) {
  uint8_t val = read(operand);
  write(operand, val);
  PSW.C = (val & 0x80);
  val <<= 1;
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80);
}

void Spc700::LSR(uint16_t adr) {
  uint8_t val = read(adr);
  PSW.C = (val & 0x01);
  val >>= 1;
  write(adr, val);
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80);
}

void Spc700::ROR(uint16_t adr) {
  uint8_t val = read(adr);
  bool newC = val & 1;
  val = (val >> 1) | (PSW.C << 7);
  PSW.C = newC;
  write(adr, val);
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80);
}

void Spc700::ROL(uint16_t adr) {
  uint8_t val = read(adr);
  bool newC = val & 0x80;
  val = (val << 1) | PSW.C;
  PSW.C = newC;
  write(adr, val);

  PSW.Z = (val == 0);
  PSW.N = (val & 0x80);
}

void Spc700::XCN(uint8_t operand, bool isImmediate) {
  uint8_t value = isImmediate ? imm() : operand;
  value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
  PSW.Z = (value == 0);
  PSW.N = (value & 0x80);
  // operand = value;
}

void Spc700::INC(uint16_t adr) {
  uint8_t val = read(adr) + 1;
  write(adr, val);
  PSW.Z = (val == 0);
  PSW.N = (val & 0x80);
}

void Spc700::DEC(uint16_t operand) {
  uint8_t val = read(operand) - 1;
  write(operand, val);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::MOVW(uint16_t& dest, uint16_t operand) {
  dest = operand;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000);
}

void Spc700::INCW(uint16_t& operand) {
  operand++;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000);
}

void Spc700::DECW(uint16_t& operand) {
  operand--;
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x8000);
}

void Spc700::ADDW(uint16_t& dest, uint16_t operand) {
  uint32_t result = dest + operand;
  PSW.C = (result > 0xFFFF);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000);
  PSW.V = ((dest ^ result) & (operand ^ result) & 0x8000);
  dest = result & 0xFFFF;
}

void Spc700::SUBW(uint16_t& dest, uint16_t operand) {
  uint32_t result = dest - operand;
  PSW.C = (result < 0x10000);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000);
  PSW.V = ((dest ^ result) & (dest ^ operand) & 0x8000);
  dest = result & 0xFFFF;
}

void Spc700::CMPW(uint16_t operand) {
  uint32_t result = YA - operand;
  PSW.C = (result < 0x10000);
  PSW.Z = ((result & 0xFFFF) == 0);
  PSW.N = (result & 0x8000);
}

void Spc700::MUL(uint8_t operand) {
  uint16_t result = A * operand;
  YA = result;
  PSW.Z = (result == 0);
  PSW.N = (result & 0x8000);
}

void Spc700::DIV(uint8_t operand) {
  if (operand == 0) {
    // Handle divide by zero error
    return;
  }
  uint8_t quotient = A / operand;
  uint8_t remainder = A % operand;
  A = quotient;
  Y = remainder;
  PSW.Z = (quotient == 0);
  PSW.N = (quotient & 0x80);
}

void Spc700::BRA(int8_t offset) { PC += offset; }

void Spc700::BEQ(int8_t offset) {
  if (PSW.Z) {
    PC += offset;
  }
}

void Spc700::BNE(int8_t offset) {
  if (!PSW.Z) {
    PC += offset;
  }
}

void Spc700::BCS(int8_t offset) {
  if (PSW.C) {
    PC += offset;
  }
}

void Spc700::BCC(int8_t offset) {
  if (!PSW.C) {
    PC += offset;
  }
}

void Spc700::BVS(int8_t offset) {
  if (PSW.V) {
    PC += offset;
  }
}

void Spc700::BVC(int8_t offset) {
  if (!PSW.V) {
    PC += offset;
  }
}

void Spc700::BMI(int8_t offset) {
  if (PSW.N) {
    PC += offset;
  }
}

void Spc700::BPL(int8_t offset) {
  if (!PSW.N) {
    PC += offset;
  }
}

void Spc700::BBS(uint8_t bit, uint8_t operand) {
  if (operand & (1 << bit)) {
    PC += rel();
  }
}

void Spc700::BBC(uint8_t bit, uint8_t operand) {
  if (!(operand & (1 << bit))) {
    PC += rel();
  }
}

// CBNE DBNZ
// JMP
void Spc700::JMP(uint16_t address) { PC = address; }

void Spc700::CALL(uint16_t address) {
  uint16_t return_address = PC + 2;
  write(SP, return_address & 0xFF);
  write(SP - 1, (return_address >> 8) & 0xFF);
  SP -= 2;
  PC = address;
}

void Spc700::PCALL(uint8_t offset) {
  uint16_t return_address = PC + 2;
  write(SP, return_address & 0xFF);
  write(SP - 1, (return_address >> 8) & 0xFF);
  SP -= 2;
  PC += offset;
}

void Spc700::TCALL(uint8_t offset) {
  uint16_t return_address = PC + 2;
  write(SP, return_address & 0xFF);
  write(SP - 1, (return_address >> 8) & 0xFF);
  SP -= 2;
  PC = 0xFFDE + offset;
}

void Spc700::BRK() {
  uint16_t return_address = PC + 2;
  write(SP, return_address & 0xFF);
  write(SP - 1, (return_address >> 8) & 0xFF);
  SP -= 2;
  PC = 0xFFDE;
}

void Spc700::RET() {
  uint16_t return_address = read(SP) | (read(SP + 1) << 8);
  SP += 2;
  PC = return_address;
}

void Spc700::RETI() {
  uint16_t return_address = read(SP) | (read(SP + 1) << 8);
  SP += 2;
  PC = return_address;
  PSW.I = 1;
}

void Spc700::PUSH(uint8_t operand) {
  write(SP, operand);
  SP--;
}

void Spc700::POP(uint8_t& operand) {
  SP++;
  operand = read(SP);
}

void Spc700::SET1(uint8_t bit, uint8_t& operand) { operand |= (1 << bit); }

void Spc700::CLR1(uint8_t bit, uint8_t& operand) { operand &= ~(1 << bit); }

void Spc700::TSET1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit));
  operand |= (1 << bit);
}

void Spc700::TCLR1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit));
  operand &= ~(1 << bit);
}

void Spc700::AND1(uint8_t bit, uint8_t& operand) {
  operand &= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::OR1(uint8_t bit, uint8_t& operand) {
  operand |= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::EOR1(uint8_t bit, uint8_t& operand) {
  operand ^= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::NOT1(uint8_t bit, uint8_t& operand) {
  operand ^= (1 << bit);
  PSW.Z = (operand == 0);
  PSW.N = (operand & 0x80);
}

void Spc700::MOV1(uint8_t bit, uint8_t& operand) {
  PSW.C = (operand & (1 << bit));
  operand |= (1 << bit);
}

void Spc700::CLRC() { PSW.C = 0; }

void Spc700::SETC() { PSW.C = 1; }

void Spc700::NOTC() { PSW.C = !PSW.C; }

void Spc700::CLRV() { PSW.V = 0; }

void Spc700::CLRP() { PSW.P = 0; }

void Spc700::SETP() { PSW.P = 1; }

void Spc700::EI() { PSW.I = 1; }

void Spc700::DI() { PSW.I = 0; }

void Spc700::NOP() { PC++; }

void Spc700::SLEEP() {}

void Spc700::STOP() {}

}  // namespace emu
}  // namespace yaze

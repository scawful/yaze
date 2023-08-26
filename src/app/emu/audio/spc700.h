#ifndef YAZE_APP_EMU_SPC700_H
#define YAZE_APP_EMU_SPC700_H

#include <cstdint>
#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

class AudioRam {
 public:
  virtual ~AudioRam() = default;

  // Read a byte from ARAM at the given address
  virtual uint8_t read(uint16_t address) const = 0;

  // Write a byte to ARAM at the given address
  virtual void write(uint16_t address, uint8_t value) = 0;
};

class AudioRamImpl : public AudioRam {
  static const size_t ARAM_SIZE = 64 * 1024;  // 64 KB
  std::vector<uint8_t> ram = std::vector<uint8_t>(ARAM_SIZE, 0);

 public:
  AudioRamImpl() = default;

  // Read a byte from ARAM at the given address
  uint8_t read(uint16_t address) const override {
    return ram[address % ARAM_SIZE];
  }

  // Write a byte to ARAM at the given address
  void write(uint16_t address, uint8_t value) override {
    ram[address % ARAM_SIZE] = value;
  }
};

class SPC700 {
 private:
  AudioRam& aram_;

 public:
  explicit SPC700(AudioRam& aram) : aram_(aram) {}
  uint8_t test_register_;
  uint8_t control_register_;
  uint8_t dsp_address_register_;

  // Registers
  uint8_t A;    // 8-bit accumulator
  uint8_t X;    // 8-bit index
  uint8_t Y;    // 8-bit index
  uint16_t YA;  // 16-bit pair of A (lsb) and Y (msb)
  uint16_t PC;  // program counter
  uint8_t SP;   // stack pointer

  struct Flags {
    uint8_t N : 1;  // Negative flag
    uint8_t V : 1;  // Overflow flag
    uint8_t P : 1;  // Direct page flag
    uint8_t B : 1;  // Break flag
    uint8_t H : 1;  // Half-carry flag
    uint8_t I : 1;  // Interrupt enable
    uint8_t Z : 1;  // Zero flag
    uint8_t C : 1;  // Carry flag
  };
  Flags PSW;  // Processor status word

  void Reset();

  void Notify(uint32_t address, uint8_t data);

  void ExecuteInstructions(uint8_t opcode);

  // Read a byte from the memory-mapped registers
  uint8_t read(uint16_t address) {
    switch (address) {
      case 0xF0:
        return test_register_;
      case 0xF1:
        return control_register_;
      case 0xF2:
        return dsp_address_register_;
      default:
        if (address < 0xFFC0) {
          return aram_.read(address);
        } else {
          // Handle IPL ROM or RAM reads here
        }
    }
    return 0;
  }

  // Write a byte to the memory-mapped registers
  void write(uint16_t address, uint8_t value) {
    switch (address) {
      case 0xF0:
        test_register_ = value;
        break;
      case 0xF1:
        control_register_ = value;
        break;
      case 0xF2:
        dsp_address_register_ = value;
        break;
      default:
        if (address < 0xFFC0) {
          aram_.write(address, value);
        } else {
          // Handle IPL ROM or RAM writes here
        }
    }
  }

  // ==========================================================================
  // Addressing modes

  // Immediate
  uint8_t imm() {
    PC++;
    return read(PC);
  }

  // Direct page
  uint8_t dp() {
    PC++;
    uint8_t offset = read(PC);
    return read((PSW.P << 8) + offset);
  }

  uint8_t get_dp_addr() {
    PC++;
    uint8_t offset = read(PC);
    return (PSW.P << 8) + offset;
  }

  // Direct page indexed by X
  uint8_t dp_plus_x() {
    PC++;
    uint8_t offset = read(PC);
    return read((PSW.P << 8) + offset + X);
  }

  // Direct page indexed by Y
  uint8_t dp_plus_y() {
    PC++;
    uint8_t offset = read(PC);
    return read((PSW.P << 8) + offset + Y);
  }

  // Indexed indirect (add index before 16-bit lookup).
  uint16_t dp_plus_x_indirect() {
    PC++;
    uint8_t offset = read(PC);
    uint16_t addr = read((PSW.P << 8) + offset + X) |
                    (read((PSW.P << 8) + offset + X + 1) << 8);
    return addr;
  }

  // Indirect indexed (add index after 16-bit lookup).
  uint16_t dp_indirect_plus_y() {
    PC++;
    uint8_t offset = read(PC);
    uint16_t baseAddr =
        read((PSW.P << 8) + offset) | (read((PSW.P << 8) + offset + 1) << 8);
    return baseAddr + Y;
  }

  uint16_t abs() {
    PC++;
    uint16_t addr = read(PC) | (read(PC) << 8);
    return addr;
  }

  int8_t rel() {
    PC++;
    return static_cast<int8_t>(read(PC));
  }

  uint8_t i() { return read((PSW.P << 8) + X); }

  uint8_t i_postinc() {
    uint8_t value = read((PSW.P << 8) + X);
    X++;
    return value;
  }

  uint16_t addr_plus_i() {
    PC++;
    uint16_t addr = read(PC) | (read(PC) << 8);
    return read(addr) + X;
  }

  uint16_t addr_plus_i_indexed() {
    PC++;
    uint16_t addr = read(PC) | (read(PC) << 8);
    addr += X;
    return read(addr) | (read(addr + 1) << 8);
  }

  // ==========================================================================
  // Instructions

  // MOV
  void MOV(uint8_t& dest, uint8_t operand) {
    dest = operand;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  void MOV_ADDR(uint16_t address, uint8_t operand) {
    write(address, operand);
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  // ADC
  void ADC(uint8_t& dest, uint8_t operand) {
    uint16_t result = dest + operand + PSW.C;
    PSW.V = ((A ^ result) & (operand ^ result) & 0x80);
    PSW.C = (result > 0xFF);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
    PSW.H = ((A ^ operand ^ result) & 0x10);
    dest = result & 0xFF;
  }

  // SBC
  void SBC(uint8_t& dest, uint8_t operand) {
    uint16_t result = dest - operand - (1 - PSW.C);
    PSW.V = ((dest ^ result) & (dest ^ operand) & 0x80);
    PSW.C = (result < 0x100);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
    PSW.H = ((dest ^ operand ^ result) & 0x10);
    dest = result & 0xFF;
  }

  // CMP
  void CMP(uint8_t& dest, uint8_t operand) {
    uint16_t result = dest - operand;
    PSW.C = (result < 0x100);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
  }

  // AND
  void AND(uint8_t& dest, uint8_t operand) {
    dest &= operand;
    PSW.Z = (dest == 0);
    PSW.N = (dest & 0x80);
  }

  // OR
  void OR(uint8_t& dest, uint8_t operand) {
    dest |= operand;
    PSW.Z = (dest == 0);
    PSW.N = (dest & 0x80);
  }

  // EOR
  void EOR(uint8_t& dest, uint8_t operand) {
    dest ^= operand;
    PSW.Z = (dest == 0);
    PSW.N = (dest & 0x80);
  }

  // ASL
  void ASL(uint8_t operand) {
    PSW.C = (operand & 0x80);
    operand <<= 1;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
    // A = value;
  }

  // LSR
  void LSR(uint8_t& operand) {
    PSW.C = (operand & 0x01);
    operand >>= 1;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  // ROL
  void ROL(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    uint8_t carry = PSW.C;
    PSW.C = (value & 0x80);
    value <<= 1;
    value |= carry;
    PSW.Z = (value == 0);
    PSW.N = (value & 0x80);
    // operand = value;
  }

  // XCN
  void XCN(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
    PSW.Z = (value == 0);
    PSW.N = (value & 0x80);
    // operand = value;
  }

  // INC
  void INC(uint8_t& operand) {
    operand++;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  // DEC
  void DEC(uint8_t& operand) {
    operand--;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  // MOVW
  void MOVW(uint16_t& dest, uint16_t operand) {
    dest = operand;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x8000);
  }

  // INCW
  void INCW(uint16_t& operand) {
    operand++;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x8000);
  }

  // DECW
  void DECW(uint16_t& operand) {
    operand--;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x8000);
  }

  // ADDW
  void ADDW(uint16_t& dest, uint16_t operand) {
    uint32_t result = dest + operand;
    PSW.C = (result > 0xFFFF);
    PSW.Z = ((result & 0xFFFF) == 0);
    PSW.N = (result & 0x8000);
    PSW.V = ((dest ^ result) & (operand ^ result) & 0x8000);
    dest = result & 0xFFFF;
  }

  // SUBW
  void SUBW(uint16_t& dest, uint16_t operand) {
    uint32_t result = dest - operand;
    PSW.C = (result < 0x10000);
    PSW.Z = ((result & 0xFFFF) == 0);
    PSW.N = (result & 0x8000);
    PSW.V = ((dest ^ result) & (dest ^ operand) & 0x8000);
    dest = result & 0xFFFF;
  }

  // CMPW
  void CMPW(uint16_t operand) {
    uint32_t result = YA - operand;
    PSW.C = (result < 0x10000);
    PSW.Z = ((result & 0xFFFF) == 0);
    PSW.N = (result & 0x8000);
  }

  // MUL
  void MUL(uint8_t operand) {
    uint16_t result = A * operand;
    YA = result;
    PSW.Z = (result == 0);
    PSW.N = (result & 0x8000);
  }

  // DIV
  void DIV(uint8_t operand) {
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

  // DAA

  // BRA
  void BRA(int8_t offset) { PC += offset; }

  // BEQ
  void BEQ(int8_t offset) {
    if (PSW.Z) {
      PC += offset;
    }
  }

  // BNE
  void BNE(int8_t offset) {
    if (!PSW.Z) {
      PC += offset;
    }
  }

  //  BCS
  void BCS(int8_t offset) {
    if (PSW.C) {
      PC += offset;
    }
  }

  //  BCC
  void BCC(int8_t offset) {
    if (!PSW.C) {
      PC += offset;
    }
  }

  //  BVS
  void BVS(int8_t offset) {
    if (PSW.V) {
      PC += offset;
    }
  }

  //  BVC
  void BVC(int8_t offset) {
    if (!PSW.V) {
      PC += offset;
    }
  }

  // BMI
  void BMI(int8_t offset) {
    if (PSW.N) {
      PC += offset;
    }
  }

  // BPL
  void BPL(int8_t offset) {
    if (!PSW.N) {
      PC += offset;
    }
  }

  // BBS
  void BBS(uint8_t bit, uint8_t operand) {
    if (operand & (1 << bit)) {
      PC += rel();
    }
  }

  // BBC
  void BBC(uint8_t bit, uint8_t operand) {
    if (!(operand & (1 << bit))) {
      PC += rel();
    }
  }

  // CBNE DBNZ
  // JMP
  void JMP(uint16_t address) { PC = address; }

  //   CALL PCALL TCALL BRK RET RETI
  //   PUSH POP
  //   SET1 CLR1 TSET1 TCLR1 AND1 OR1 EOR1 NOT1 MOV1
  //   CLRC SETC NOTC CLRV CLRP SETP EI DI
  //   NOP SLEEP STOP
};

}  // namespace emu
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EMU_SPC700_H
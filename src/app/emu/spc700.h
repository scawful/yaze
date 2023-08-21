#ifndef YAZE_APP_EMU_SPC700_H
#define YAZE_APP_EMU_SPC700_H

#include <cstdint>
#include <iostream>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

class AudioRAM {
  static const size_t ARAM_SIZE = 64 * 1024;  // 64 KB
  std::vector<uint8_t> ram;

 public:
  AudioRAM() : ram(ARAM_SIZE, 0) {}

  // Read a byte from ARAM at the given address
  uint8_t read(uint16_t address) const { return ram[address % ARAM_SIZE]; }

  // Write a byte to ARAM at the given address
  void write(uint16_t address, uint8_t value) {
    ram[address % ARAM_SIZE] = value;
  }
};

class SDSP {
  static const size_t NUM_VOICES = 8;
  static const size_t NUM_VOICE_REGS = 10;
  static const size_t NUM_GLOBAL_REGS = 15;

  // Each voice has 10 registers
  std::vector<std::vector<uint8_t>> voices;

  // Global registers
  std::vector<uint8_t> globalRegs;

 public:
  SDSP()
      : voices(NUM_VOICES, std::vector<uint8_t>(NUM_VOICE_REGS, 0)),
        globalRegs(NUM_GLOBAL_REGS, 0) {}

  // Read a byte from a voice register
  uint8_t readVoiceReg(uint8_t voice, uint8_t reg) const {
    return voices[voice % NUM_VOICES][reg % NUM_VOICE_REGS];
  }

  // Write a byte to a voice register
  void writeVoiceReg(uint8_t voice, uint8_t reg, uint8_t value) {
    voices[voice % NUM_VOICES][reg % NUM_VOICE_REGS] = value;
  }

  // Read a byte from a global register
  uint8_t readGlobalReg(uint8_t reg) const {
    return globalRegs[reg % NUM_GLOBAL_REGS];
  }

  // Write a byte to a global register
  void writeGlobalReg(uint8_t reg, uint8_t value) {
    globalRegs[reg % NUM_GLOBAL_REGS] = value;
  }
};

class SPC700 {
  AudioRAM aram;
  SDSP sdsp;
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
      case 0xF3:
        return sdsp.readGlobalReg(dsp_address_register_);
      default:
        if (address < 0xFFC0) {
          return aram.read(address);
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
      case 0xF3:
        sdsp.writeGlobalReg(dsp_address_register_, value);
        break;
      default:
        if (address < 0xFFC0) {
          aram.write(address, value);
        } else {
          // Handle IPL ROM or RAM writes here
        }
    }
  }

  // Addressing modes
  uint8_t imm() {
    PC++;
    return read(PC);
  }

  uint8_t dp() {
    PC++;
    uint8_t offset = read(PC);
    return read((PSW.P << 8) + offset);
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

  // Instructions
  // MOV
  void MOV(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    operand = value;
    PSW.Z = (operand == 0);
    PSW.N = (operand & 0x80);
  }

  // ADC
  void ADC(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    uint16_t result = A + value + PSW.C;
    PSW.V = ((A ^ result) & (value ^ result) & 0x80);
    PSW.C = (result > 0xFF);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
    PSW.H = ((A ^ value ^ result) & 0x10);
    A = result & 0xFF;
  }

  // SBC
  void SBC(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    uint16_t result = A - value - (1 - PSW.C);
    PSW.V = ((A ^ result) & (A ^ value) & 0x80);
    PSW.C = (result < 0x100);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
    PSW.H = ((A ^ value ^ result) & 0x10);
    A = result & 0xFF;
  }

  // CMP
  void CMP(uint8_t operand, bool isImmediate = false) {
    uint8_t value = isImmediate ? imm() : operand;
    uint16_t result = A - value;
    PSW.C = (result < 0x100);
    PSW.Z = ((result & 0xFF) == 0);
    PSW.N = (result & 0x80);
  }

  // AND OR EOR ASL LSR ROL XCN
  // INC DEC
  // MOVW INCW DECW ADDW SUBW CMPW MUL DIV
  // DAA DAS
  // BRA BEQ BNE BCS BCC BVS BVC BMI BPL BBS BBC CBNE DBNZ JMP
  // CALL PCALL TCALL BRK RET RETI
  // PUSH POP
  // SET1 CLR1 TSET1 TCLR1 AND1 OR1 EOR1 NOT1 MOV1
  // CLRC SETC NOTC CLRV CLRP SETP EI DI
  // NOP SLEEP STOP
};

}  // namespace emu
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EMU_SPC700_H
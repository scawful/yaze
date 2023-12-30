#ifndef YAZE_APP_EMU_SPC700_H
#define YAZE_APP_EMU_SPC700_H

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace app {
namespace emu {

class AudioRam {
 public:
  virtual ~AudioRam() = default;
  virtual void reset() = 0;
  virtual uint8_t read(uint16_t address) const = 0;
  virtual uint8_t& mutable_read(uint16_t address) = 0;
  virtual void write(uint16_t address, uint8_t value) = 0;
};

class AudioRamImpl : public AudioRam {
  static const int ARAM_SIZE = 0x10000;
  std::vector<uint8_t> ram = std::vector<uint8_t>(ARAM_SIZE, 0);

 public:
  AudioRamImpl() = default;
  void reset() override { ram = std::vector<uint8_t>(ARAM_SIZE, 0); }

  uint8_t read(uint16_t address) const override {
    return ram[address % ARAM_SIZE];
  }

  uint8_t& mutable_read(uint16_t address) override {
    return ram.at(address % ARAM_SIZE);
  }

  void write(uint16_t address, uint8_t value) override {
    ram[address % ARAM_SIZE] = value;
  }
};

class Spc700 {
 private:
  AudioRam& aram_;
  std::vector<std::string> log_;

  const uint8_t ipl_rom_[64]{
      0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA,
      0xF4, 0x8F, 0xBB, 0xF5, 0x78, 0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19,
      0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5, 0xCB,
      0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E,
      0xF4, 0x10, 0xEB, 0xBA, 0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4,
      0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF};

 public:
  explicit Spc700(AudioRam& aram) : aram_(aram) {}

  // Registers
  uint8_t A = 0x00;      // 8-bit accumulator
  uint8_t X = 0x00;      // 8-bit index
  uint8_t Y = 0x00;      // 8-bit index
  uint16_t YA = 0x00;    // 16-bit pair of A (lsb) and Y (msb)
  uint16_t PC = 0xFFC0;  // program counter
  uint8_t SP = 0x00;     // stack pointer

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

  uint8_t FlagsToByte(Flags flags) {
    return (flags.N << 7) | (flags.V << 6) | (flags.P << 5) | (flags.B << 4) |
           (flags.H << 3) | (flags.I << 2) | (flags.Z << 1) | (flags.C);
  }

  Flags ByteToFlags(uint8_t byte) {
    Flags flags;
    flags.N = (byte & 0x80) >> 7;
    flags.V = (byte & 0x40) >> 6;
    flags.P = (byte & 0x20) >> 5;
    flags.B = (byte & 0x10) >> 4;
    flags.H = (byte & 0x08) >> 3;
    flags.I = (byte & 0x04) >> 2;
    flags.Z = (byte & 0x02) >> 1;
    flags.C = (byte & 0x01);
    return flags;
  }

  void Reset();

  void BootIplRom();

  void ExecuteInstructions(uint8_t opcode);
  void LogInstruction(uint16_t initial_pc, uint8_t opcode);

  // Read a byte from the memory-mapped registers
  uint8_t read(uint16_t address) {
    if (address < 0xFFC0) {
      return aram_.read(address);
    } else {
      // Check if register is set to unmap the IPL ROM
      if (read(0xF1) & 0x80) {
        return aram_.read(address);
      }
      return ipl_rom_[address - 0xFFC0];
    }
  }

  uint8_t& mutable_read(uint16_t address) {
    if (address < 0xFFC0) {
      return aram_.mutable_read(address);
    } else {
      // NOTE: Mutable access to IPL ROM is not allowed
      return aram_.mutable_read(address);
    }
  }

  uint16_t& mutable_read_16(uint16_t address) {
    if (address < 0xFFC0) {
      return *reinterpret_cast<uint16_t*>(&aram_.mutable_read(address));
    } else {
      // NOTE: Mutable access to IPL ROM is not allowed
      return *reinterpret_cast<uint16_t*>(&aram_.mutable_read(address));
    }
  }

  uint16_t read_16(uint16_t address) {
    if (address < 0xFFC0) {
      return (aram_.read(address) | (aram_.read(address + 1) << 8));
    } else {
      // Check if register is set to unmap the IPL ROM
      if (read(0xF1) & 0x80) {
        return aram_.read(address);
      }
      return ipl_rom_[address - 0xFFC0];
    }
  }

  // Write a byte to the memory-mapped registers
  void write(uint16_t address, uint8_t value) {
    if (address < 0xFFC0) {
      aram_.write(address, value);
    } else {
      // Check if register is set to unmap the IPL ROM
      if (read(0xF1) & 0x80) {
        aram_.write(address, value);
      }
    }
  }

  // ======================================================
  // Addressing modes

  // Immediate
  uint8_t imm();

  // Direct page
  uint8_t dp();
  uint8_t& mutable_dp();

  uint8_t get_dp_addr();

  // Direct page indexed by X
  uint8_t dp_plus_x();

  // Direct page indexed by Y
  uint8_t dp_plus_y();

  // Indexed indirect (add index before 16-bit lookup).
  uint16_t dp_plus_x_indirect();

  // Indirect indexed (add index after 16-bit lookup).
  uint16_t dp_indirect_plus_y();

  uint16_t abs();

  int8_t rel();

  uint8_t i();

  uint8_t i_postinc();

  uint16_t addr_plus_i();

  uint16_t addr_plus_i_indexed();

  // ==========================================================================
  // Instructions

  void MOV(uint8_t& dest, uint8_t operand);
  void MOV_ADDR(uint16_t address, uint8_t operand);
  void ADC(uint8_t& dest, uint8_t operand);
  void SBC(uint8_t& dest, uint8_t operand);
  void CMP(uint8_t& dest, uint8_t operand);
  void AND(uint8_t& dest, uint8_t operand);
  void OR(uint8_t& dest, uint8_t operand);
  void EOR(uint8_t& dest, uint8_t operand);
  void ASL(uint8_t operand);
  void LSR(uint8_t& operand);
  void ROL(uint8_t operand, bool isImmediate = false);
  void XCN(uint8_t operand, bool isImmediate = false);
  void INC(uint8_t& operand);
  void DEC(uint8_t& operand);
  void MOVW(uint16_t& dest, uint16_t operand);
  void INCW(uint16_t& operand);
  void DECW(uint16_t& operand);
  void ADDW(uint16_t& dest, uint16_t operand);
  void SUBW(uint16_t& dest, uint16_t operand);
  void CMPW(uint16_t operand);
  void MUL(uint8_t operand);
  void DIV(uint8_t operand);
  void BRA(int8_t offset);
  void BEQ(int8_t offset);
  void BNE(int8_t offset);
  void BCS(int8_t offset);
  void BCC(int8_t offset);
  void BVS(int8_t offset);
  void BVC(int8_t offset);
  void BMI(int8_t offset);
  void BPL(int8_t offset);
  void BBS(uint8_t bit, uint8_t operand);
  void BBC(uint8_t bit, uint8_t operand);
  void JMP(uint16_t address);
  void CALL(uint16_t address);
  void PCALL(uint8_t offset);
  void TCALL(uint8_t offset);
  void BRK();
  void RET();
  void RETI();
  void PUSH(uint8_t operand);
  void POP(uint8_t& operand);
  void SET1(uint8_t bit, uint8_t& operand);
  void CLR1(uint8_t bit, uint8_t& operand);
  void TSET1(uint8_t bit, uint8_t& operand);
  void TCLR1(uint8_t bit, uint8_t& operand);
  void AND1(uint8_t bit, uint8_t& operand);
  void OR1(uint8_t bit, uint8_t& operand);
  void EOR1(uint8_t bit, uint8_t& operand);
  void NOT1(uint8_t bit, uint8_t& operand);
  void MOV1(uint8_t bit, uint8_t& operand);
  void CLRC();
  void SETC();
  void NOTC();
  void CLRV();
  void CLRP();
  void SETP();
  void EI();
  void DI();
  void NOP();
  void SLEEP();
  void STOP();

  // CBNE DBNZ
};

}  // namespace emu
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EMU_SPC700_H
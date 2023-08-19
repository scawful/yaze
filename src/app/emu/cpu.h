#ifndef YAZE_APP_EMU_CPU_H_
#define YAZE_APP_EMU_CPU_H_

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

// ADC: Add with carry
// AND: Logical AND
// ASL: Arithmetic shift left
// BCC: Branch if carry clear
// BCS: Branch if carry set
// BEQ: Branch if equal (zero set)
// BIT: Bit test
// BMI: Branch if minus (negative set)
// BNE: Branch if not equal (zero clear)
// BPL: Branch if plus (negative clear)
// BRA: Branch always
// BRK: Break
// BRL: Branch always long
// BVC: Branch if overflow clear
// BVS: Branch if overflow set
// CLC: Clear carry
// CLD: Clear decimal
// CLI: Clear interrupt disable
// CLV: Clear overflow
// CMP: Compare
// COP: Coprocessor
// CPX: Compare X register
// CPY: Compare Y register
// DEC: Decrement
// DEX: Decrement X register
// DEY: Decrement Y register
// EOR: Exclusive OR
// INC: Increment
// INX: Increment X register
// INY: Increment Y register
// JMP: Jump
// JML: Jump long
// JSR: Jump to subroutine
// JSL: Jump to subroutine long
// LDA: Load accumulator
// LDX: Load X register
// LDY: Load Y register
// LSR: Logical shift right
// MVN: Move negative
// MVP: Move positive
// NOP: No operation
// ORA: Logical OR
// PEA: Push effective address
// PEI: Push effective indirect address
// PER: Push effective PC-relative address
// PHA: Push accumulator
// PHB: Push data bank register
// PHD: Push direct page register
// PHK: Push program bank register
// PHP: Push processor status register
// PHX: Push X register
// PHY: Push Y register
// PLA: Pull accumulator
// PLB: Pull data bank register
// PLD: Pull direct page register
// PLP: Pull processor status register
// PLX: Pull X register
// PLY: Pull Y register
// ROL: Rotate left
// ROR: Rotate right
// RTI: Return from interrupt
// RTL: Return from subroutine long
// RTS: Return from subroutine
// SBC: Subtract with carry
// STA: Store accumulator
// STP: Stop the clock
// STX: Store X register
// STY: Store Y register
// STZ: Store zero
// TDC: Transfer direct page register to accumulator
// TRB: Test and reset bits
// TSB: Test and set bits
// WAI: Wait for interrupt
// XBA: Exchange B and A accumulator
// XCE: Exchange carry and emulation

class CPU : public Memory {
 private:
  Memory& memory;

 public:
  explicit CPU(Memory& mem) : memory(mem) {}

  void Init() {}

  uint8_t ReadByte(uint16_t address) const override;
  uint16_t ReadWord(uint16_t address) const override;
  uint32_t ReadWordLong(uint16_t address) const override;

  void WriteByte(uint32_t address, uint8_t value) override;
  void WriteWord(uint32_t address, uint16_t value) override;

  void SetMemory(const std::vector<uint8_t>& data) override {
    memory.SetMemory(data);
  }

  uint8_t FetchByte();
  uint16_t FetchWord();
  uint32_t FetchLong();
  int8_t FetchSignedByte();
  int16_t FetchSignedWord();

  uint8_t FetchByteDirectPage(uint8_t operand);

  uint16_t DirectPageIndexedIndirectX();
  uint16_t StackRelative();
  uint16_t DirectPage();
  uint16_t DirectPageIndirectLong();
  uint16_t Immediate();
  uint16_t Absolute();
  uint16_t AbsoluteLong();
  uint16_t DirectPageIndirectIndexedY();
  uint16_t DirectPageIndirect();
  uint16_t StackRelativeIndirectIndexedY();
  uint16_t DirectPageIndexedX();
  uint16_t DirectPageIndirectLongIndexedY();
  uint16_t AbsoluteIndexedY();
  uint16_t AbsoluteIndexedX();
  uint16_t AbsoluteLongIndexedX();

  void ExecuteInstruction(uint8_t opcode);

  void loadROM(const std::vector<uint8_t>& rom) {
    // if (rom.size() > memory.size()) {
    //   std::cerr << "ROM too large" << std::endl;
    //   return;
    // }
    // std::copy(rom.begin(), rom.end(), memory.begin());
  }

  // Registers
  uint8_t A = 0;    // Accumulator
  uint8_t X = 0;    // X index register
  uint8_t Y = 0;    // Y index register
  uint8_t SP = 0;   // Stack Pointer
  uint16_t DB = 0;  // Data Bank register
  uint16_t D = 0;   // Direct Page register
  uint16_t PB = 0;  // Program Bank register
  uint16_t PC = 0;  // Program Counter
  uint8_t status;   // Processor Status (P)

  // Mnemonic 	Value 	Binary 	Description
  // N 	      #$80 	10000000 	Negative
  // V 	      #$40 	01000000 	Overflow
  // M 	      #$20 	00100000 	Accumulator size (0 = 16-bit, 1 = 8-bit)
  // X 	      #$10 	00010000 	Index size (0 = 16-bit, 1 = 8-bit)
  // D 	      #$08 	00001000 	Decimal
  // I 	      #$04 	00000100 	IRQ disable
  // Z 	      #$02 	00000010 	Zero
  // C 	      #$01 	00000001 	Carry
  // E 			                  6502 emulation mode
  // B 	      #$10 	00010000 	Break (emulation mode only)

  // Setting flags in the status register
  int GetAccumulatorSize() { return status & 0x20; }
  int GetIndexSize() { return status & 0x10; }

  // Set individual flags
  void SetNegativeFlag(bool set) { SetFlag(0x80, set); }
  void SetOverflowFlag(bool set) { SetFlag(0x40, set); }
  void SetBreakFlag(bool set) { SetFlag(0x10, set); }
  void SetDecimalFlag(bool set) { SetFlag(0x08, set); }
  void SetInterruptFlag(bool set) { SetFlag(0x04, set); }
  void SetZeroFlag(bool set) { SetFlag(0x02, set); }
  void SetCarryFlag(bool set) { SetFlag(0x01, set); }

  // Get individual flags
  bool GetNegativeFlag() const { return GetFlag(0x80); }
  bool GetOverflowFlag() const { return GetFlag(0x40); }
  bool GetBreakFlag() const { return GetFlag(0x10); }
  bool GetDecimalFlag() const { return GetFlag(0x08); }
  bool GetInterruptFlag() const { return GetFlag(0x04); }
  bool GetZeroFlag() const { return GetFlag(0x02); }
  bool GetCarryFlag() const { return GetFlag(0x01); }

  // Instructions
  void ADC(uint8_t operand);
  void AND(uint16_t address);

  void BEQ(int8_t offset) {
    if (GetZeroFlag()) {  // If the zero flag is set
      PC += offset;       // Add the offset to the program counter
    }
  }

  void BCC(int8_t offset) {
    if (!GetCarryFlag()) {  // If the carry flag is clear
      PC += offset;         // Add the offset to the program counter
    }
  }

  void BRL(int16_t offset) {
    PC += offset;  // Add the offset to the program counter
  }

  void LDA() {
    A = memory[PC];
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
    PC++;
  }

  void SEC() { status |= 0x01; }

  void CLC() { status &= ~0x01; }

  void CLD() { status &= ~0x08; }

  void CLI() { status &= ~0x04; }

  void CLV() { status &= ~0x40; }

  void SEI() { status |= 0x04; }

  void SED() { status |= 0x08; }

  void SEP() {
    PC++;
    auto byte = FetchByte();
    status |= byte;
  }

  void REP() {
    PC++;
    auto byte = FetchByte();
    status &= ~byte;
  }

  void TCD() {
    D = A;
    SetZeroFlag(D == 0);
    SetNegativeFlag(D & 0x80);
  }

  void TDC() {
    A = D;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  void TCS() { SP = A; }

  void TAX() {
    X = A;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  void TAY() {
    Y = A;
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  }

  void TYA() {
    A = Y;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  void TXA() {
    A = X;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  void TXY() {
    X = Y;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  void TYX() {
    Y = X;
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  }

  void TSX() {
    X = SP;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  void TXS() { SP = X; }

  void TSC() {
    A = SP;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  void INX() {
    X++;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  void INY() {
    Y++;
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  }

 private:
  // Helper function to set or clear a specific flag bit
  void SetFlag(uint8_t mask, bool set) {
    if (set) {
      status |= mask;  // Set the bit
    } else {
      status &= ~mask;  // Clear the bit
    }
  }

  // Helper function to get the value of a specific flag bit
  bool GetFlag(uint8_t mask) const { return (status & mask) != 0; }

  void ClearMemory() override { memory.ClearMemory(); }
  void LoadData(const std::vector<uint8_t>& data) override {
    memory.LoadData(data);
  }

  // Appease the C++ Gods...
  uint8_t operator[](int i) const override { return 0; }
  uint8_t at(int i) const override { return 0; }
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_CPU_H_
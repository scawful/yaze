#ifndef YAZE_APP_EMU_CPU_H_
#define YAZE_APP_EMU_CPU_H_

#include <cstdint>
#include <iostream>
#include <vector>

#include "app/emu/mem.h"

namespace yaze {
namespace app {
namespace emu {

class Clock {
 public:
  Clock() = default;
  virtual ~Clock() = default;

  void Cycle() { cycle++; }
  void SetFrequency(float frequency) { this->frequency = frequency; }
  float GetFrequency() const { return frequency; }
  unsigned long long GetCycleCount() const { return cycleCount; }

 private:
  uint64_t cycle;                 // Current cycle
  float frequency;                // Frequency of the clock in Hz
  unsigned long long cycleCount;  // Total number of cycles executed
};

class CPU : public Memory, public Clock {
 public:
  explicit CPU(Memory& mem) : memory(mem) {}
  void Init() { memory.ClearMemory(); }

  uint8_t ReadByte(uint16_t address) const override;
  uint16_t ReadWord(uint16_t address) const override;
  uint32_t ReadWordLong(uint16_t address) const override;
  void WriteByte(uint32_t address, uint8_t value) override;
  void WriteWord(uint32_t address, uint16_t value) override;
  void SetMemory(const std::vector<uint8_t>& data) override {
    memory.SetMemory(data);
  }
  int16_t SP() const override { return memory.SP(); }
  void SetSP(int16_t value) override { memory.SetSP(value); }

  uint8_t FetchByte();
  uint16_t FetchWord();
  uint32_t FetchLong();
  int8_t FetchSignedByte();
  int16_t FetchSignedWord();

  uint8_t FetchByteDirectPage(uint8_t operand);

  void ExecuteInstruction(uint8_t opcode);

  // ==========================================================================
  // Addressing Modes

  // Effective Address:
  //    Bank: Data Bank Register if locating data
  //          Program Bank Register if transferring control
  //    High: Second operand byte
  //    Low:  First operand byte
  //
  // LDA addr
  uint16_t Absolute() { return FetchWord(); }

  // Effective Address:
  //    The Data Bank Register is concatened with the 16-bit operand
  //    the 24-bit result is added to the X Index Register
  //    based on the emulation mode (16:X=0, 8:X=1)
  //
  // LDA addr, X
  uint16_t AbsoluteIndexedX() { return FetchWord() + X; }

  // Effective Address:
  //    The Data Bank Register is concatened with the 16-bit operand
  //    the 24-bit result is added to the Y Index Register
  //    based on the emulation mode (16:Y=0, 8:Y=1)
  //
  // LDA addr, Y
  uint16_t AbsoluteIndexedY() { return FetchWord() + Y; }

  // Test Me :)
  // Effective Address:
  //    Bank:             Program Bank Register (PBR)
  //    High/low:         The Indirect Address
  //    Indirect Address: Located in the Program Bank at the sum of
  //                      the operand double byte and X based on the
  //                      emulation mode
  // JMP (addr, X)
  uint16_t AbsoluteIndexedIndirect() {
    uint16_t address = FetchWord() + X;
    return memory.ReadWord(address & 0xFFFF);  // Consider PBR if needed
  }

  // Effective Address:
  //    Bank:             Program Bank Register (PBR)
  //    High/low:         The Indirect Address
  //    Indirect Address: Located in Bank Zero, at the operand double byte
  //
  // JMP (addr)
  uint16_t AbsoluteIndirect() {
    uint16_t address = FetchWord();
    return memory.ReadWord(address);
  }

  // Effective Address:
  //   Bank/High/Low: The 24-bit Indirect Address
  //   Indirect Address: Located in Bank Zero, at the operand double byte
  //
  // JMP [addr]
  uint32_t AbsoluteIndirectLong() {
    uint16_t address = FetchWord();
    return memory.ReadWordLong(address);
  }

  // Effective Address:
  //    Bank: Third operand byte
  //    High: Second operand byte
  //    Low:  First operand byte
  //
  // LDA long
  uint32_t AbsoluteLong() { return FetchLong(); }

  // Effective Address:
  //   The 24-bit operand is added to X based on the emulation mode
  //
  // LDA long, X
  uint16_t AbsoluteLongIndexedX() { return FetchLong() + X; }

  // Source Effective Address:
  //    Bank: Second operand byte
  //    High/Low: The 16-bit value in X, if X is 8-bit high byte is 0
  //
  // Destination Effective Address:
  //    Bank: First operand byte
  //    High/Low: The 16-bit value in Y, if Y is 8-bit high byte is 0
  //
  // Length:
  //    The number of bytes to be moved: 16-bit value in Acculumator C plus 1.
  //
  // MVN src, dst
  void BlockMove(uint16_t source, uint16_t dest, uint16_t length) {
    for (int i = 0; i < length; i++) {
      memory.WriteByte(dest + i, memory.ReadByte(source + i));
    }
  }

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte
  //
  // LDA dp
  uint16_t DirectPage() {
    uint8_t dp = FetchByte();
    return D + dp;
  }

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte plus X
  //              based on the emulation mode
  //
  // LDA dp, X
  uint16_t DirectPageIndexedX() {
    uint8_t dp = FetchByte();
    return (dp + X) & 0xFF;
  }

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte plus Y
  //              based on the emulation mode
  // LDA dp, Y
  uint16_t DirectPageIndexedY() {
    uint8_t dp = FetchByte();
    return (dp + Y) & 0xFF;
  }

  // Effective Address:
  // Bank:      Data bank register
  // High/low:  The indirect address
  // Indirect Address: Located in the direct page at the sum of the direct page
  // register, the operand byte, and X based on the emulation mode in bank zero.
  //
  // LDA (dp, X)
  uint16_t DirectPageIndexedIndirectX() {
    uint8_t dp = FetchByte();
    uint16_t effective_address = D + dp + X;
    uint16_t indirect_address = memory.ReadWord(effective_address & 0xFFFF);
    return indirect_address;
  }

  // Effective Address:
  // Bank:     Data bank register
  // High/low: The 16-bit indirect address
  // Indirect Address: The operand byte plus the direct page register in bank
  // zero.
  //
  // LDA (dp)
  uint16_t DirectPageIndirect() {
    uint8_t dp = FetchByte();
    // Add the Direct Page register to the fetched operand
    uint16_t effective_address = D + dp;
    return memory.ReadWord(effective_address);
  }

  // Effective Address:
  //    Bank/High/Low:    The 24-bit indirect address
  //    Indirect address: The operand byte plus the direct page
  //                   register in bank zero.
  //
  // LDA [dp]
  uint32_t DirectPageIndirectLong() {
    uint8_t dp = FetchByte();
    uint16_t effective_address = D + dp;
    return memory.ReadWordLong(effective_address);
  }

  // Effective Address:
  //    Found by concatenating the data bank to the double-byte
  //    indirect address, then adding Y based on the emulation mode.
  //
  // Indirect Address: Located in the Direct Page at the sum of the direct page
  //                   register and the operand byte, in bank zero.
  //
  // LDA (dp), Y
  uint16_t DirectPageIndirectIndexedY() {
    uint8_t dp = FetchByte();
    uint16_t effective_address = D + dp;
    return memory.ReadWord(effective_address) + Y;
  }

  // Effective Address:
  //    Found by adding to the triple-byte indirect address Y based on the
  //    emulation mode. Indrect Address: Located in the Direct Page at the sum
  //    of the direct page register and the operand byte in bank zero.
  // Indirect Address:
  //    Located in the Direct Page at the sum of the direct page register and
  //    the operand byte in bank zero.
  //
  // LDA (dp), Y
  uint16_t DirectPageIndirectLongIndexedY() {
    uint8_t dp = FetchByte();
    uint16_t effective_address = D + dp + Y;
    return memory.ReadWordLong(effective_address);
  }

  // 8-bit data: Data Operand Byte
  // 16-bit data 65816 native mode m or x = 0
  //   Data High: Second Operand Byte
  //   Data Low:  First Operand Byte
  //
  // LDA #const
  uint16_t Immediate() {
    if (GetAccumulatorSize()) {
      return FetchByte();
    } else {
      return FetchWord();
    }
  }

  uint16_t StackRelative() {
    uint8_t sr = FetchByte();
    return SP() + sr;
  }

  uint16_t StackRelativeIndirectIndexedY() {
    uint8_t sr = FetchByte();
    return memory.ReadWord(SP() + sr + Y);
  }

  // ==========================================================================
  // Registers

  uint8_t A = 0;    // Accumulator
  uint8_t B = 0;    // Accumulator (High)
  uint16_t X = 0;   // X index register
  uint16_t Y = 0;   // Y index register
  uint16_t D = 0;   // Direct Page register
  uint16_t DB = 0;  // Data Bank register
  uint8_t PB = 0;   // Program Bank register
  uint16_t PC = 0;  // Program Counter
  uint8_t E = 1;    // Emulation mode flag
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
  int GetAccumulatorSize() const { return status & 0x20; }
  int GetIndexSize() const { return status & 0x10; }
  void SetAccumulatorSize(bool set) { SetFlag(0x20, set); }
  void SetIndexSize(bool set) { SetFlag(0x10, set); }

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

  // ==========================================================================
  // Instructions

  // ADC: Add with carry
  void ADC(uint8_t operand);
  void ANDAbsoluteLong(uint32_t address);

  // AND: Logical AND
  void AND(uint16_t address, bool isImmediate = false);

  // ASL: Arithmetic shift left
  void ASL(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    SetCarryFlag(!(value & 0x80));  // Set carry flag if bit 7 is set
    value <<= 1;                    // Shift left
    value &= 0xFE;                  // Clear bit 0
    memory.WriteByte(address, value);
    SetNegativeFlag(!value);
    SetZeroFlag(value);
  }

  // BCC: Branch if carry clear
  void BCC(int8_t offset) {
    if (!GetCarryFlag()) {  // If the carry flag is clear
      PC += offset;         // Add the offset to the program counter
    }
  }

  // BCS: Branch if carry set
  void BCS(int8_t offset) {
    if (GetCarryFlag()) {  // If the carry flag is set
      PC += offset;        // Add the offset to the program counter
    }
  }

  // BEQ: Branch if equal (zero set)
  void BEQ(int8_t offset) {
    if (GetZeroFlag()) {  // If the zero flag is set
      PC += offset;       // Add the offset to the program counter
    }
  }

  // BIT: Bit test
  void BIT(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    SetNegativeFlag(value & 0x80);
    SetOverflowFlag(value & 0x40);
    SetZeroFlag((A & value) == 0);
  }

  // BMI: Branch if minus (negative set)
  void BMI(int8_t offset) {
    if (GetNegativeFlag()) {  // If the negative flag is set
      PC += offset;           // Add the offset to the program counter
    }
  }

  // BNE: Branch if not equal (zero clear)
  void BNE(int8_t offset) {
    if (!GetZeroFlag()) {  // If the zero flag is clear
      PC += offset;        // Add the offset to the program counter
    }
  }

  // BPL: Branch if plus (negative clear)
  void BPL(int8_t offset) {
    if (!GetNegativeFlag()) {  // If the negative flag is clear
      PC += offset;            // Add the offset to the program counter
    }
  }

  // BRA: Branch always
  void BRA(int8_t offset) { PC += offset; }

  // BRK: Break
  void BRK() {
    PC += 2;  // Increment the program counter by 2
    memory.PushWord(PC);
    memory.PushByte(status);
    SetInterruptFlag(true);
    PC = memory.ReadWord(0xFFFE);
  }

  // BRL: Branch always long
  void BRL(int16_t offset) {
    PC += offset;  // Add the offset to the program counter
  }

  // BVC: Branch if overflow clear
  void BVC(int8_t offset) {
    if (!GetOverflowFlag()) {  // If the overflow flag is clear
      PC += offset;            // Add the offset to the program counter
    }
  }

  // BVS: Branch if overflow set
  void BVS(int8_t offset) {
    if (GetOverflowFlag()) {  // If the overflow flag is set
      PC += offset;           // Add the offset to the program counter
    }
  }

  // CLC: Clear carry flag
  void CLC() { status &= ~0x01; }

  // CLD: Clear decimal mode
  void CLD() { status &= ~0x08; }

  // CLI: Clear interrupt disable flag
  void CLI() { status &= ~0x04; }

  // CLV: Clear overflow flag
  void CLV() { status &= ~0x40; }

  // CMP: Compare ```
  // COP: Coprocessor ```

  // CPX: Compare X register
  // CPX: Compare X register
  void CPX(uint16_t value, bool isImmediate = false) {
    uint16_t memory_value = isImmediate
                                ? value
                                : (GetIndexSize() ? memory.ReadByte(value)
                                                  : memory.ReadWord(value));
    compare(X, memory_value);
  }

  // CPY: Compare Y register
  void CPY(uint16_t value, bool isImmediate = false) {
    uint16_t memory_value = isImmediate
                                ? value
                                : (GetIndexSize() ? memory.ReadByte(value)
                                                  : memory.ReadWord(value));
    compare(Y, memory_value);
  }

  // DEC: Decrement ```

  // DEX: Decrement X register
  void DEX() {
    if (GetIndexSize()) {  // 8-bit
      X = static_cast<uint8_t>(X - 1);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x80);
    } else {  // 16-bit
      X = static_cast<uint16_t>(X - 1);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x8000);
    }
  }

  // DEY: Decrement Y register
  void DEY() {
    if (GetIndexSize()) {  // 8-bit
      Y = static_cast<uint8_t>(Y - 1);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x80);
    } else {  // 16-bit
      Y = static_cast<uint16_t>(Y - 1);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x8000);
    }
  }

  // EOR: Exclusive OR ```

  // INC: Increment
  void INC(uint16_t address) {
    if (GetAccumulatorSize()) {
      uint8_t value = memory.ReadByte(address);
      value++;
      memory.WriteByte(address, value);
      SetNegativeFlag(value & 0x80);
      SetZeroFlag(value == 0);
    } else {
      uint16_t value = memory.ReadWord(address);
      value++;
      memory.WriteWord(address, value);
      SetNegativeFlag(value & 0x8000);
      SetZeroFlag(value == 0);
    }
  }

  // INX: Increment X register
  void INX() {
    if (GetIndexSize()) {  // 8-bit
      X = static_cast<uint8_t>(X + 1);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x80);
    } else {  // 16-bit
      X = static_cast<uint16_t>(X + 1);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x8000);
    }
  }

  // INY: Increment Y register
  void INY() {
    if (GetIndexSize()) {  // 8-bit
      Y = static_cast<uint8_t>(Y + 1);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x80);
    } else {  // 16-bit
      Y = static_cast<uint16_t>(Y + 1);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x8000);
    }
  }

  // JMP: Jump
  void JMP(uint16_t address) {
    PC = address;  // Set program counter to the new address
  }

  // JML: Jump long
  void JML(uint32_t address) {
    // Set the lower 16 bits of PC to the lower 16 bits of the address
    PC = static_cast<uint8_t>(address & 0xFFFF);
    // Set the PBR to the upper 8 bits of the address
    PB = static_cast<uint8_t>((address >> 16) & 0xFF);
  }

  // JSR: Jump to subroutine
  void JSR(uint16_t address) {
    PC -= 1;              // Subtract 1 from program counter
    memory.PushWord(PC);  // Push the program counter onto the stack
    PC = address;         // Set program counter to the new address
  }

  // JSL: Jump to subroutine long
  void JSL(uint32_t address) {
    PC -= 1;              // Subtract 1 from program counter
    memory.PushLong(PC);  // Push the program counter onto the stack as a long
                          // value (24 bits)
    PC = address;         // Set program counter to the new address
  }

  // LDA: Load accumulator
  void LDA(uint16_t address, bool isImmediate = false) {
    if (GetAccumulatorSize()) {
      A = isImmediate ? address : memory.ReadByte(address);
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {
      A = isImmediate ? address : memory.ReadWord(address);
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
  }

  // LDX: Load X register
  void LDX(uint16_t address, bool isImmediate = false) {
    if (GetIndexSize()) {
      X = isImmediate ? address : memory.ReadByte(address);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x80);
    } else {
      X = isImmediate ? address : memory.ReadWord(address);
      SetZeroFlag(X == 0);
      SetNegativeFlag(X & 0x8000);
    }
  }

  // LDY: Load Y register
  void LDY(uint16_t address, bool isImmediate = false) {
    if (GetIndexSize()) {
      Y = isImmediate ? address : memory.ReadByte(address);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x80);
    } else {
      Y = isImmediate ? address : memory.ReadWord(address);
      SetZeroFlag(Y == 0);
      SetNegativeFlag(Y & 0x8000);
    }
  }

  // LSR: Logical shift right
  void LSR(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    SetCarryFlag(value & 0x01);
    value >>= 1;
    memory.WriteByte(address, value);
    SetNegativeFlag(false);
    SetZeroFlag(value == 0);
  }

  // MVN: Move negative ```
  // MVP: Move positive ```

  // NOP: No operation
  void NOP() {
    // Do nothing
  }

  // ORA: Logical OR
  void ORA(uint16_t address, bool isImmediate = false) {
    if (GetAccumulatorSize()) {
      A |= isImmediate ? address : memory.ReadByte(address);
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x80);
    } else {
      A |= isImmediate ? address : memory.ReadWord(address);
      SetZeroFlag(A == 0);
      SetNegativeFlag(A & 0x8000);
    }
  }

  // PEA: Push effective address
  void PEA() {
    uint16_t address = FetchWord();
    memory.PushWord(address);
  }

  // PEI: Push effective indirect address
  void PEI() {
    uint16_t address = FetchWord();
    memory.PushWord(memory.ReadWord(address));
  }

  // PER: Push effective PC-relative address
  void PER() {
    uint16_t address = FetchWord();
    memory.PushWord(PC + address);
  }

  // PHA: Push Accumulator on Stack
  void PHA() { memory.PushByte(A); }

  // PHB: Push Data Bank Register on Stack
  void PHB() { memory.PushByte(DB); }

  // PHD: Push Program Bank Register on Stack
  void PHD() { memory.PushWord(D); }

  // PHK: Push Program Bank Register on Stack
  void PHK() { memory.PushByte(PB); }

  // PHP: Push Processor Status Register on Stack
  void PHP() { memory.PushByte(status); }

  // PHX: Push X Index Register on Stack
  void PHX() { memory.PushByte(X); }

  // PHY: Push Y Index Register on Stack
  void PHY() { memory.PushByte(Y); }

  // PLA: Pull Accumulator from Stack
  void PLA() {
    A = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
    SetZeroFlag(A == 0);
  }

  // PLB: Pull data bank register
  void PLB() {
    DB = memory.PopByte();
    SetNegativeFlag((DB & 0x80) != 0);
    SetZeroFlag(DB == 0);
  }

  // Pull Direct Page Register from Stack
  void PLD() {
    D = memory.PopWord();
    SetNegativeFlag((D & 0x8000) != 0);
    SetZeroFlag(D == 0);
  }

  // Pull Processor Status Register from Stack
  void PLP() { status = memory.PopByte(); }

  // PLX: Pull X Index Register from Stack
  void PLX() {
    X = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
    SetZeroFlag(X == 0);
  }

  // PHY: Pull Y Index Register from Stack
  void PLY() {
    Y = memory.PopByte();
    SetNegativeFlag((A & 0x80) != 0);
    SetZeroFlag(Y == 0);
  }

  // REP: Reset status bits
  void REP() {
    PC++;
    auto byte = FetchByte();
    status &= ~byte;
  }

  // ROL: Rotate left
  void ROL(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    uint8_t carry = GetCarryFlag() ? 0x01 : 0x00;
    SetCarryFlag(value & 0x80);
    value <<= 1;
    value |= carry;
    memory.WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  }

  // ROR: Rotate right
  void ROR(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    uint8_t carry = GetCarryFlag() ? 0x80 : 0x00;
    SetCarryFlag(value & 0x01);
    value >>= 1;
    value |= carry;
    memory.WriteByte(address, value);
    SetNegativeFlag(value & 0x80);
    SetZeroFlag(value == 0);
  }

  // RTI: Return from interrupt
  void RTI() {
    status = memory.PopByte();
    PC = memory.PopWord();
  }

  // RTL: Return from subroutine long
  void RTL() {
    PC = memory.PopWord();
    PB = memory.PopByte();
  }

  // RTS: Return from subroutine
  void RTS() { PC = memory.PopWord() + 1; }

  // SBC: Subtract with carry
  void SBC(uint16_t operand, bool isImmediate = false);

  // SEC: Set carry flag
  void SEC() { status |= 0x01; }

  // SED: Set decimal mode
  void SED() { status |= 0x08; }

  // SEI: Set interrupt disable flag
  void SEI() { status |= 0x04; }

  // SEP: Set status bits
  void SEP() {
    PC++;
    auto byte = FetchByte();
    status |= byte;
  }

  // STA: Store accumulator
  void STA(uint16_t address) {
    if (GetAccumulatorSize()) {
      memory.WriteByte(address, static_cast<uint8_t>(A));
    } else {
      memory.WriteWord(address, A);
    }
  }

  // STP: Stop the clock ```

  // STX: Store X register
  void STX(uint16_t address) {
    if (GetIndexSize()) {
      memory.WriteByte(address, static_cast<uint8_t>(X));
    } else {
      memory.WriteWord(address, X);
    }
  }

  // STY: Store Y register
  void STY(uint16_t address) {
    if (GetIndexSize()) {
      memory.WriteByte(address, static_cast<uint8_t>(Y));
    } else {
      memory.WriteWord(address, Y);
    }
  }

  // STZ: Store zero
  void STZ(uint16_t address) {
    if (GetAccumulatorSize()) {
      memory.WriteByte(address, 0x00);
    } else {
      memory.WriteWord(address, 0x0000);
    }
  }

  // TAX: Transfer accumulator to X
  void TAX() {
    X = A;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  // TAY: Transfer accumulator to Y
  void TAY() {
    Y = A;
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  }

  // TCD: Transfer accumulator to direct page register
  void TCD() {
    D = A;
    SetZeroFlag(D == 0);
    SetNegativeFlag(D & 0x80);
  }

  // TCS: Transfer accumulator to stack pointer
  void TCS() { memory.SetSP(A); }

  // TDC: Transfer direct page register to accumulator
  void TDC() {
    A = D;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  // TRB: Test and reset bits
  void TRB(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    SetZeroFlag((A & value) == 0);
    value &= ~A;
    memory.WriteByte(address, value);
  }

  // TSB: Test and set bits
  void TSB(uint16_t address) {
    uint8_t value = memory.ReadByte(address);
    SetZeroFlag((A & value) == 0);
    value |= A;
    memory.WriteByte(address, value);
  }

  // TSC: Transfer stack pointer to accumulator
  void TSC() {
    A = SP();
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  // TSX: Transfer stack pointer to X
  void TSX() {
    X = SP();
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  // TXA: Transfer X to accumulator
  void TXA() {
    A = X;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  // TXS: Transfer X to stack pointer
  void TXS() { memory.SetSP(X); }

  // TXY: Transfer X to Y
  void TXY() {
    X = Y;
    SetZeroFlag(X == 0);
    SetNegativeFlag(X & 0x80);
  }

  // TYA: Transfer Y to accumulator
  void TYA() {
    A = Y;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  // TYX: Transfer Y to X
  void TYX() {
    Y = X;
    SetZeroFlag(Y == 0);
    SetNegativeFlag(Y & 0x80);
  }

  // WAI: Wait for interrupt ```

  // XBA: Exchange B and A accumulator
  void XBA() {
    uint8_t temp = A;
    A = B;
    B = temp;
    SetZeroFlag(A == 0);
    SetNegativeFlag(A & 0x80);
  }

  // XCE: Exchange Carry and Emulation Flags
  void XCE() {
    uint8_t carry = status & 0x01;
    status &= ~0x01;
    status |= E;
    E = carry;
  }

 private:
  void compare(uint16_t register_value, uint16_t memory_value) {
    uint16_t result;
    if (GetIndexSize()) {
      // 8-bit mode
      uint8_t result8 = static_cast<uint8_t>(register_value) -
                        static_cast<uint8_t>(memory_value);
      result = result8;
      SetNegativeFlag(result & 0x80);  // Negative flag for 8-bit
    } else {
      // 16-bit mode
      result = register_value - memory_value;
      SetNegativeFlag(result & 0x8000);  // Negative flag for 16-bit
    }
    SetZeroFlag(result == 0);                      // Zero flag
    SetCarryFlag(register_value >= memory_value);  // Carry flag
  }

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

  // Appease the C++ Gods...
  void PushByte(uint8_t value) override { memory.PushByte(value); }
  void PushWord(uint16_t value) override { memory.PushWord(value); }
  uint8_t PopByte() override { return memory.PopByte(); }
  uint16_t PopWord() override { return memory.PopWord(); }
  void PushLong(uint32_t value) override { memory.PushLong(value); }
  uint32_t PopLong() override { return memory.PopLong(); }
  void ClearMemory() override { memory.ClearMemory(); }
  void LoadData(const std::vector<uint8_t>& data) override {
    memory.LoadData(data);
  }
  uint8_t operator[](int i) const override { return 0; }
  uint8_t at(int i) const override { return 0; }

  Memory& memory;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_CPU_H_
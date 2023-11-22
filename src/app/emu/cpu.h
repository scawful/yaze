#ifndef YAZE_APP_EMU_CPU_H_
#define YAZE_APP_EMU_CPU_H_

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "app/core/common.h"
#include "app/emu/clock.h"
#include "app/emu/debug/log.h"
#include "app/emu/internal/opcodes.h"
#include "app/emu/memory/memory.h"

namespace yaze {
namespace app {
namespace emu {

class InstructionEntry {
 public:
  // Constructor
  InstructionEntry(uint32_t addr, uint8_t op, const std::string& ops,
                   const std::string& instr)
      : address(addr), opcode(op), operands(ops), instruction(instr) {}

  // Getters for the class members
  uint32_t GetAddress() const { return address; }
  uint8_t GetOpcode() const { return opcode; }
  const std::string& GetOperands() const { return operands; }
  const std::string& GetInstruction() const { return instruction; }

  uint32_t address;         // Memory address of the instruction
  uint8_t opcode;           // Opcode of the instruction
  std::string operands;     // Operand(s) of the instruction, if any
  std::string instruction;  // Human-readable instruction text
};

const int kCpuClockSpeed = 21477272;  // 21.477272 MHz

class CPU : public Memory, public Loggable, public core::ExperimentFlags {
 public:
  explicit CPU(Memory& mem, Clock& vclock) : memory(mem), clock(vclock) {}
  enum class UpdateMode { Run, Step, Pause };

  void Init() { clock.SetFrequency(kCpuClockSpeed); }

  void Update(UpdateMode mode = UpdateMode::Run, int stepCount = 1);

  void ExecuteInstruction(uint8_t opcode);
  void LogInstructions(uint16_t PC, uint8_t opcode, uint16_t operand,
                       bool immediate, bool accumulator_mode);
  void HandleInterrupts();

  bool IsBreakpoint(uint32_t address) {
    return std::find(breakpoints_.begin(), breakpoints_.end(), address) !=
           breakpoints_.end();
  }

  void SetBreakpoint(uint32_t address) { breakpoints_.push_back(address); }

  void ClearBreakpoint(uint32_t address) {
    breakpoints_.erase(
        std::remove(breakpoints_.begin(), breakpoints_.end(), address),
        breakpoints_.end());
  }

  void ClearBreakpoints() {
    breakpoints_.clear();
    breakpoints_.shrink_to_fit();
  }

  auto GetBreakpoints() { return breakpoints_; }

  std::vector<uint32_t> breakpoints_;
  std::vector<InstructionEntry> instruction_log_;

  // ======================================================
  // Registers

  uint16_t A = 0;               // Accumulator
  uint16_t X = 0;               // X index register
  uint16_t Y = 0;               // Y index register
  uint16_t D = 0;               // Direct Page register
  uint8_t DB = 0;               // Data Bank register
  uint8_t PB = 0;               // Program Bank register
  uint16_t PC = 0;              // Program Counter
  uint8_t E = 1;                // Emulation mode flag
  uint8_t status = 0b00110000;  // Processor Status (P)

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

  // ======================================================
  // Instructions

  // ADC: Add with carry
  void ADC(uint8_t operand);
  void ANDAbsoluteLong(uint32_t address);

  // AND: Logical AND
  void AND(uint16_t address, bool isImmediate = false);

  // ASL: Arithmetic shift left
  void ASL(uint16_t address);

  // BCC: Branch if carry clear
  void BCC(int8_t offset);

  // BCS: Branch if carry set
  void BCS(int8_t offset);

  // BEQ: Branch if equal
  void BEQ(int8_t offset);

  // BIT: Bit test
  void BIT(uint16_t address);

  // BMI: Branch if minus
  void BMI(int8_t offset);

  // BNE: Branch if not equal
  void BNE(int8_t offset);

  // BPL: Branch if plus
  void BPL(int8_t offset);

  // BRA: Branch always
  void BRA(int8_t offset);

  // BRK: Force interrupt
  void BRK();

  // BRL: Branch always long
  void BRL(int16_t offset);

  // BVC: Branch if overflow clear
  void BVC(int8_t offset);

  // BVS: Branch if overflow set
  void BVS(int8_t offset);

  // CLC: Clear carry flag
  void CLC();

  // CLD: Clear decimal mode
  void CLD();

  // CLI: Clear interrupt disable bit
  void CLI();

  // CLV: Clear overflow flag
  void CLV();

  // CMP: Compare
  void CMP(uint16_t address, bool isImmediate = false);

  // COP: Coprocessor enable
  void COP();

  // CPX: Compare X register
  void CPX(uint16_t address, bool isImmediate = false);

  // CPY: Compare Y register
  void CPY(uint16_t address, bool isImmediate = false);

  // DEC: Decrement memory
  void DEC(uint16_t address);

  // DEX: Decrement X register
  void DEX();

  // DEY: Decrement Y register
  void DEY();

  // EOR: Exclusive OR
  void EOR(uint16_t address, bool isImmediate = false);

  // INC: Increment memory
  void INC(uint16_t address);

  // INX: Increment X register
  void INX();

  // INY: Increment Y register
  void INY();

  // JMP: Jump
  void JMP(uint16_t address);

  // JML: Jump long
  void JML(uint32_t address);

  // JSR: Jump to subroutine
  void JSR(uint16_t address);

  // JSL: Jump to subroutine long
  void JSL(uint32_t address);

  // LDA: Load accumulator
  void LDA(uint16_t address, bool isImmediate = false);

  // LDX: Load X register
  void LDX(uint16_t address, bool isImmediate = false);

  // LDY: Load Y register
  void LDY(uint16_t address, bool isImmediate = false);

  // LSR: Logical shift right
  void LSR(uint16_t address);

  // MVN: Block move next
  void MVN(uint16_t source, uint16_t dest, uint16_t length);

  // MVP: Block move previous
  void MVP(uint16_t source, uint16_t dest, uint16_t length);

  // NOP: No operation
  void NOP();

  // ORA: Logical inclusive OR
  void ORA(uint16_t address, bool isImmediate = false);

  // PEA: Push effective absolute address
  void PEA();

  // PEI: Push effective indirect address
  void PEI();

  // PER: Push effective relative address
  void PER();

  // PHA: Push accumulator
  void PHA();

  // PHB: Push data bank register
  void PHB();

  // PHD: Push direct page register
  void PHD();

  // PHK: Push program bank register
  void PHK();

  // PHP: Push processor status (flags)
  void PHP();

  // PHX: Push X register
  void PHX();

  // PHY: Push Y register
  void PHY();

  // PLA: Pull accumulator
  void PLA();

  // PLB: Pull data bank register
  void PLB();

  // PLD: Pull direct page register
  void PLD();

  // PLP: Pull processor status (flags)
  void PLP();

  // PLX: Pull X register
  void PLX();

  // PLY: Pull Y register
  void PLY();

  // REP: Reset processor status bits
  void REP();

  // ROL: Rotate left
  void ROL(uint16_t address);

  // ROR: Rotate right
  void ROR(uint16_t address);

  // RTI: Return from interrupt
  void RTI();

  // RTL: Return from subroutine long
  void RTL();

  // RTS: Return from subroutine
  void RTS();

  // SBC: Subtract with carry
  void SBC(uint16_t operand, bool isImmediate = false);

  // SEC: Set carry flag
  void SEC();

  // SED: Set decimal mode
  void SED();

  // SEI: Set interrupt disable status
  void SEI();

  // SEP: Set processor status bits
  void SEP();

  // STA: Store accumulator
  void STA(uint16_t address);

  // STP: Stop the processor
  void STP();

  // STX: Store X register
  void STX(uint16_t address);

  // STY: Store Y register
  void STY(uint16_t address);

  // STZ: Store zero
  void STZ(uint16_t address);

  // TAX: Transfer accumulator to X
  void TAX();

  // TAY: Transfer accumulator to Y
  void TAY();

  // TCD: Transfer 16-bit accumulator to direct page register
  void TCD();

  // TCS: Transfer 16-bit accumulator to stack pointer
  void TCS();

  // TDC: Transfer direct page register to 16-bit accumulator
  void TDC();

  // TRB: Test and reset bits
  void TRB(uint16_t address);

  // TSB: Test and set bits
  void TSB(uint16_t address);

  // TSC: Transfer stack pointer to 16-bit accumulator
  void TSC();

  // TSX: Transfer stack pointer to X
  void TSX();

  // TXA: Transfer X to accumulator
  void TXA();

  // TXS: Transfer X to stack pointer
  void TXS();

  // TXY: Transfer X to Y
  void TXY();

  // TYA: Transfer Y to accumulator
  void TYA();

  // TYX: Transfer Y to X
  void TYX();

  // WAI: Wait for interrupt
  void WAI();

  // WDM: Reserved for future expansion
  void WDM();

  // XBA: Exchange B and A
  void XBA();

  // XCE: Exchange carry and emulation bits
  void XCE();

  // Memory access routines
  uint8_t ReadByte(uint32_t address) const override {
    auto value = memory.ReadByte(address);
    return value;
  }
  uint16_t ReadWord(uint32_t address) const override {
    return memory.ReadWord(address);
  }
  uint32_t ReadWordLong(uint32_t address) const override {
    return memory.ReadWordLong(address);
  }

  std::vector<uint8_t> ReadByteVector(uint32_t address,
                                      uint16_t size) const override {
    return memory.ReadByteVector(address, size);
  }

  void WriteByte(uint32_t address, uint8_t value) override {
    memory.WriteByte(address, value);
  }

  void WriteWord(uint32_t address, uint16_t value) override {
    memory.WriteWord(address, value);
  }

  uint8_t FetchByte() {
    uint8_t byte = memory.ReadByte(PC);  // Read a byte from memory at PC
    PC++;                                // Increment the Program Counter
    return byte;
  }

  uint16_t FetchWord() {
    uint16_t value = memory.ReadWord(PC);
    PC += 2;
    return value;
  }

  uint32_t FetchLong() {
    uint32_t value = memory.ReadWordLong(PC);
    PC += 3;
    return value;
  }

  int8_t FetchSignedByte() { return static_cast<int8_t>(FetchByte()); }

  int16_t FetchSignedWord() {
    auto offset = static_cast<int16_t>(FetchWord());
    return offset;
  }

  uint8_t FetchByteDirectPage(uint8_t operand) {
    uint16_t distance = D * 0x100;

    // Calculate the effective address in the Direct Page
    uint16_t effectiveAddress = operand + distance;

    // Fetch the byte from memory
    uint8_t fetchedByte = memory.ReadByte(effectiveAddress);

    PC++;  // Increment the Program Counter

    return fetchedByte;
  }

  void SetMemory(const std::vector<uint8_t>& data) override {
    memory.SetMemory(data);
  }

  int16_t SP() const override { return memory.SP(); }
  void SetSP(int16_t value) override { memory.SetSP(value); }
  void UpdateClock(int delta_time) { clock.UpdateClock(delta_time); }

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
  Clock& clock;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EMU_CPU_H_
#ifndef YAZE_APP_EMU_CPU_H_
#define YAZE_APP_EMU_CPU_H_

#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/emu/memory/memory.h"

namespace yaze {
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

class Cpu {
 public:
  explicit Cpu(Memory& mem) : memory(mem) {}
  void Reset(bool hard = false);

  auto& callbacks() { return callbacks_; }
  const auto& callbacks() const { return callbacks_; }

  void RunOpcode();

  void ExecuteInstruction(uint8_t opcode);
  void LogInstructions(uint16_t PC, uint8_t opcode, uint16_t operand,
                       bool immediate, bool accumulator_mode);

  void SetIrq(bool state) { irq_wanted_ = state; }
  void Nmi() { nmi_wanted_ = true; }

  std::vector<uint32_t> breakpoints_;
  std::vector<InstructionEntry> instruction_log_;

  // ======================================================
  // Interrupt Vectors
  // Emulation mode, e = 1      Native mode, e = 0
  //
  // 0xFFFE,FF - IRQ/BRK        0xFFEE,EF  - IRQ
  // 0xFFFC,FD - RESET
  // 0xFFFA,FB - NMI            0xFFEA,EB  - NMI
  // 0xFFF8,F9 - ABORT          0xFFE8,E9  - ABORT
  //                            0xFFE6,E7  - BRK
  // 0xFFF4,F5 - COP            0xFFE4,E5  - COP
  void DoInterrupt();

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

  void SetFlags(uint8_t val) {
    status = val;
    if (E) {
      SetAccumulatorSize(true);
      SetIndexSize(true);
      SetSP(SP() & 0xFF | 0x100);
    }
    if (GetIndexSize()) {
      X &= 0xff;
      Y &= 0xff;
    }
  }

  void SetZN(uint16_t value, bool byte) {
    if (byte) {
      SetZeroFlag((value & 0xff) == 0);
      SetNegativeFlag(value & 0x80);
    } else {
      SetZeroFlag(value == 0);
      SetNegativeFlag(value & 0x8000);
    }
  }

  // Setting flags in the status register
  bool m() { return GetAccumulatorSize() ? 1 : 0; }
  bool xf() { return GetIndexSize() ? 1 : 0; }
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

  enum class AccessType { Control, Data };

  uint8_t ReadOpcode() { return ReadByte((PB << 16) | PC++); }

  uint16_t ReadOpcodeWord(bool int_check = false) {
    uint8_t value = ReadOpcode();
    if (int_check) CheckInt();
    return value | (ReadOpcode() << 8);
  }

  // Memory access routines
  uint8_t ReadByte(uint32_t address) { return callbacks_.read_byte(address); }
  
  // Read 16-bit value from consecutive addresses (little-endian)
  uint16_t ReadWord(uint32_t address) {
    uint8_t low = ReadByte(address);
    uint8_t high = ReadByte(address + 1);
    return low | (high << 8);
  }
  
  // Read 16-bit value from two separate addresses (for wrapping/crossing boundaries)
  uint16_t ReadWord(uint32_t address, uint32_t address_high,
                    bool int_check = false) {
    uint8_t value = ReadByte(address);
    if (int_check) CheckInt();
    uint8_t value2 = ReadByte(address_high);
    return value | (value2 << 8);
  }
  uint32_t ReadWordLong(uint32_t address) {
    uint8_t value = ReadByte(address);
    uint8_t value2 = ReadByte(address + 1);
    uint8_t value3 = ReadByte(address + 2);
    return value | (value2 << 8) | (value3 << 16);
  }

  void WriteByte(uint32_t address, uint8_t value) {
    callbacks_.write_byte(address, value);
  }

  void WriteWord(uint32_t address, uint32_t address_high, uint16_t value,
                 bool reversed = false, bool int_check = false) {
    if (reversed) {
      callbacks_.write_byte(address_high, value >> 8);
      if (int_check) CheckInt();
      callbacks_.write_byte(address, value & 0xFF);
    } else {
      callbacks_.write_byte(address, value & 0xFF);
      if (int_check) CheckInt();
      callbacks_.write_byte(address_high, value >> 8);
    }
  }
  void WriteLong(uint32_t address, uint32_t value) {
    callbacks_.write_byte(address, value & 0xFF);
    callbacks_.write_byte(address + 1, (value >> 8) & 0xFF);
    callbacks_.write_byte(address + 2, value >> 16);
  }

  void PushByte(uint8_t value) {
    callbacks_.write_byte(SP(), value);
    SetSP(SP() - 1);
    if (E) SetSP((SP() & 0xff) | 0x100);
  }
  void PushWord(uint16_t value, bool int_check = false) {
    PushByte(value >> 8);
    if (int_check) CheckInt();
    PushByte(value & 0xFF);
  }
  void PushLong(uint32_t value) {  // Push 24-bit value
    PushByte(value >> 16);
    PushWord(value & 0xFFFF);
  }

  uint8_t PopByte() {
    SetSP(SP() + 1);
    if (E) SetSP((SP() & 0xff) | 0x100);
    return ReadByte(SP());
  }
  uint16_t PopWord(bool int_check = false) {
    uint8_t low = PopByte();
    if (int_check) CheckInt();
    return low | (PopByte() << 8);
  }
  uint32_t PopLong() {  // Pop 24-bit value
    uint32_t low = PopWord();
    uint32_t high = PopByte();
    return (high << 16) | low;
  }

  void DoBranch(bool check) {
    if (!check) CheckInt();
    uint8_t value = ReadOpcode();
    if (check) {
      CheckInt();
      callbacks_.idle(false);  // taken branch: 1 extra cycle
      PC += (int8_t)value;
    }
  }

  void set_int_delay(bool delay) { int_delay_ = delay; }

  // Addressing Modes

  // Effective Address:
  //    Bank: Data Bank Register if locating data
  //          Program Bank Register if transferring control
  //    High: Second operand byte
  //    Low:  First operand byte
  //
  // LDA addr
  uint32_t Absolute(uint32_t* low);

  // Effective Address:
  //    The Data Bank Register is concatened with the 16-bit operand
  //    the 24-bit result is added to the X Index Register
  //    based on the emulation mode (16:X=0, 8:X=1)
  //
  // LDA addr, X
  uint32_t AbsoluteIndexedX();
  uint32_t AdrAbx(uint32_t* low, bool write);

  // Effective Address:
  //    The Data Bank Register is concatened with the 16-bit operand
  //    the 24-bit result is added to the Y Index Register
  //    based on the emulation mode (16:Y=0, 8:Y=1)
  //
  // LDA addr, Y
  uint32_t AbsoluteIndexedY();
  uint32_t AdrAby(uint32_t* low, bool write);

  void AdrImp();
  uint32_t AdrIdx(uint32_t* low);

  uint32_t AdrIdp(uint32_t* low);
  uint32_t AdrIdy(uint32_t* low, bool write);
  uint32_t AdrIdl(uint32_t* low);
  uint32_t AdrIly(uint32_t* low);
  uint32_t AdrIsy(uint32_t* low);
  uint32_t Immediate(uint32_t* low, bool xFlag);

  // Effective Address:
  //    Bank:             Program Bank Register (PBR)
  //    High/low:         The Indirect Address
  //    Indirect Address: Located in the Program Bank at the sum of
  //                      the operand double byte and X based on the
  //                      emulation mode
  // JMP (addr, X)
  uint16_t AbsoluteIndexedIndirect();

  // Effective Address:
  //    Bank:             Program Bank Register (PBR)
  //    High/low:         The Indirect Address
  //    Indirect Address: Located in Bank Zero, at the operand double byte
  //
  // JMP (addr)
  uint16_t AbsoluteIndirect();

  // Effective Address:
  //   Bank/High/Low: The 24-bit Indirect Address
  //   Indirect Address: Located in Bank Zero, at the operand double byte
  //
  // JMP [addr]
  uint32_t AbsoluteIndirectLong();

  // Effective Address:
  //    Bank: Third operand byte
  //    High: Second operand byte
  //    Low:  First operand byte
  //
  // LDA long
  uint32_t AbsoluteLong();
  uint32_t AdrAbl(uint32_t* low);

  // Effective Address:
  //   The 24-bit operand is added to X based on the emulation mode
  //
  // LDA long, X
  uint32_t AbsoluteLongIndexedX();
  uint32_t AdrAlx(uint32_t* low);

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
  void BlockMove(uint16_t source, uint16_t dest, uint16_t length);

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte
  //
  // LDA dp
  uint16_t DirectPage();
  uint32_t AdrDp(uint32_t* low);

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte plus X
  //              based on the emulation mode
  //
  // LDA dp, X
  uint16_t DirectPageIndexedX();
  uint32_t AdrDpx(uint32_t* low);

  // Effective Address:
  //    Bank:     Zero
  //    High/low: Direct Page Register plus operand byte plus Y
  //              based on the emulation mode
  // LDA dp, Y
  uint16_t DirectPageIndexedY();
  uint32_t AdrDpy(uint32_t* low);

  // Effective Address:
  // Bank:      Data bank register
  // High/low:  The indirect address
  // Indirect Address: Located in the direct page at the sum of the direct page
  // register, the operand byte, and X based on the emulation mode in bank zero.
  //
  // LDA (dp, X)
  uint16_t DirectPageIndexedIndirectX();

  // Effective Address:
  // Bank:     Data bank register
  // High/low: The 16-bit indirect address
  // Indirect Address: The operand byte plus the direct page register in bank
  // zero.
  //
  // LDA (dp)
  uint16_t DirectPageIndirect();

  // Effective Address:
  //    Bank/High/Low:    The 24-bit indirect address
  //    Indirect address: The operand byte plus the direct page
  //                   register in bank zero.
  //
  // LDA [dp]
  uint32_t DirectPageIndirectLong();

  // Effective Address:
  //    Found by concatenating the data bank to the double-byte
  //    indirect address, then adding Y based on the emulation mode.
  //
  // Indirect Address: Located in the Direct Page at the sum of the direct page
  //                   register and the operand byte, in bank zero.
  //
  // LDA (dp), Y
  uint16_t DirectPageIndirectIndexedY();

  // Effective Address:
  //    Found by adding to the triple-byte indirect address Y based on the
  //    emulation mode. Indrect Address: Located in the Direct Page at the sum
  //    of the direct page register and the operand byte in bank zero.
  // Indirect Address:
  //    Located in the Direct Page at the sum of the direct page register and
  //    the operand byte in bank zero.
  //
  // LDA (dp), Y
  uint32_t DirectPageIndirectLongIndexedY();

  // 8-bit data: Data Operand Byte
  // 16-bit data 65816 native mode m or x = 0
  //   Data High: Second Operand Byte
  //   Data Low:  First Operand Byte
  //
  // LDA #const
  uint16_t Immediate(bool index_size = false);

  uint16_t StackRelative();
  uint32_t AdrSr(uint32_t* low);

  // Effective Address:
  //    The Data Bank Register is concatenated to the Indirect Address;
  //    the 24-bit result is added to Y (16 bits if x = 0; else 8 bits)
  // Indirect Address:
  //    Located at the 16-bit sum of the 8-bit operand and the 16-bit stack
  //    pointer
  //
  // LDA (sr, S), Y
  uint32_t StackRelativeIndirectIndexedY();

  // ======================================================
  // Instructions

  // ADC: Add with carry
  void ADC(uint16_t operand);

  // AND: Logical AND
  void AND(uint32_t address, bool immediate = false);
  void ANDAbsoluteLong(uint32_t address);

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
  void CMP(uint32_t address, bool immediate = false);

  // COP: Coprocessor enable
  void COP();

  // CPX: Compare X register
  void CPX(uint32_t address, bool immediate = false);

  // CPY: Compare Y register
  void CPY(uint32_t address, bool immediate = false);

  // DEC: Decrement memory
  void DEC(uint32_t address, bool accumulator = false);

  // DEX: Decrement X register
  void DEX();

  // DEY: Decrement Y register
  void DEY();

  // EOR: Exclusive OR
  void EOR(uint32_t address, bool immediate = false);

  // INC: Increment memory
  void INC(uint32_t address, bool accumulator = false);

  // INX: Increment X register
  void INX();

  // INY: Increment Y register
  void INY();

  // JMP: Jump
  void JMP(uint16_t address);

  // JML: Jump long
  void JML(uint16_t address);

  // JSR: Jump to subroutine
  void JSR(uint16_t address);

  // JSL: Jump to subroutine long
  void JSL(uint16_t address);

  // LDA: Load accumulator
  void LDA(uint16_t address, bool immediate = false, bool direct_page = false,
           bool data_bank = false);

  // LDX: Load X register
  void LDX(uint16_t address, bool immediate = false);

  // LDY: Load Y register
  void LDY(uint16_t address, bool immediate = false);

  // LSR: Logical shift right
  void LSR(uint16_t address, bool accumulator = false);

  // MVN: Block move next
  void MVN();

  // MVP: Block move previous
  void MVP();

  // NOP: No operation
  void NOP();

  // ORA: Logical inclusive OR
  void ORA(uint32_t low, uint32_t high);

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
  void ROL(uint32_t address, bool accumulator = false);

  // ROR: Rotate right
  void ROR(uint32_t address, bool accumulator = false);

  // RTI: Return from interrupt
  void RTI();

  // RTL: Return from subroutine long
  void RTL();

  // RTS: Return from subroutine
  void RTS();

  // SBC: Subtract with carry
  void SBC(uint32_t operand, bool immediate = false);

  // SEC: Set carry flag
  void SEC();

  // SED: Set decimal mode
  void SED();

  // SEI: Set interrupt disable status
  void SEI();

  // SEP: Set processor status bits
  void SEP();

  // STA: Store accumulator
  void STA(uint32_t address);

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

  void And(uint32_t low, uint32_t high);
  void Eor(uint32_t low, uint32_t high);
  void Adc(uint32_t low, uint32_t high);
  void Sbc(uint32_t low, uint32_t high);
  void Cmp(uint32_t low, uint32_t high);
  void Cpx(uint32_t low, uint32_t high);
  void Cpy(uint32_t low, uint32_t high);
  void Bit(uint32_t low, uint32_t high);
  void Lda(uint32_t low, uint32_t high);
  void Ldx(uint32_t low, uint32_t high);
  void Ldy(uint32_t low, uint32_t high);
  void Sta(uint32_t low, uint32_t high);
  void Stx(uint32_t low, uint32_t high);
  void Sty(uint32_t low, uint32_t high);
  void Stz(uint32_t low, uint32_t high);
  void Ror(uint32_t low, uint32_t high);
  void Rol(uint32_t low, uint32_t high);
  void Lsr(uint32_t low, uint32_t high);
  void Asl(uint32_t low, uint32_t high);
  void Inc(uint32_t low, uint32_t high);
  void Dec(uint32_t low, uint32_t high);
  void Tsb(uint32_t low, uint32_t high);
  void Trb(uint32_t low, uint32_t high);

  uint16_t SP() const { return memory.SP(); }
  void SetSP(uint16_t value) { memory.SetSP(value); }

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

  void CheckInt() {
    int_wanted_ =
        (nmi_wanted_ || (irq_wanted_ && !GetInterruptFlag())) && !int_delay_;
    int_delay_ = false;
  }

  auto mutable_log_instructions() -> bool* { return &log_instructions_; }

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

  void SetFlag(uint8_t mask, bool set) {
    if (set) {
      status |= mask;  // Set the bit
    } else {
      status &= ~mask;  // Clear the bit
    }
  }

  bool GetFlag(uint8_t mask) const { return (status & mask) != 0; }

  bool log_instructions_ = false;

  bool waiting_ = false;
  bool stopped_ = false;

  bool irq_wanted_ = false;
  bool nmi_wanted_ = false;
  bool reset_wanted_ = false;
  bool int_wanted_ = false;
  bool int_delay_ = false;

  Memory& memory;
  CpuCallbacks callbacks_;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_CPU_H_

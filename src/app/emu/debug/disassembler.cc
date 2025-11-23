#include "app/emu/debug/disassembler.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace yaze {
namespace emu {
namespace debug {

Disassembler65816::Disassembler65816() { InitializeOpcodeTable(); }

void Disassembler65816::InitializeOpcodeTable() {
  // Initialize all opcodes with their mnemonics and addressing modes
  // Format: opcode_table_[opcode] = {mnemonic, addressing_mode, base_size}

  using AM = AddressingMode65816;

  // Row 0x00-0x0F
  opcode_table_[0x00] = {"BRK", AM::kImmediate8, 2};
  opcode_table_[0x01] = {"ORA", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0x02] = {"COP", AM::kImmediate8, 2};
  opcode_table_[0x03] = {"ORA", AM::kStackRelative, 2};
  opcode_table_[0x04] = {"TSB", AM::kDirectPage, 2};
  opcode_table_[0x05] = {"ORA", AM::kDirectPage, 2};
  opcode_table_[0x06] = {"ASL", AM::kDirectPage, 2};
  opcode_table_[0x07] = {"ORA", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0x08] = {"PHP", AM::kImplied, 1};
  opcode_table_[0x09] = {"ORA", AM::kImmediateM, 2};  // Size depends on M flag
  opcode_table_[0x0A] = {"ASL", AM::kAccumulator, 1};
  opcode_table_[0x0B] = {"PHD", AM::kImplied, 1};
  opcode_table_[0x0C] = {"TSB", AM::kAbsolute, 3};
  opcode_table_[0x0D] = {"ORA", AM::kAbsolute, 3};
  opcode_table_[0x0E] = {"ASL", AM::kAbsolute, 3};
  opcode_table_[0x0F] = {"ORA", AM::kAbsoluteLong, 4};

  // Row 0x10-0x1F
  opcode_table_[0x10] = {"BPL", AM::kProgramCounterRelative, 2};
  opcode_table_[0x11] = {"ORA", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0x12] = {"ORA", AM::kDirectPageIndirect, 2};
  opcode_table_[0x13] = {"ORA", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0x14] = {"TRB", AM::kDirectPage, 2};
  opcode_table_[0x15] = {"ORA", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x16] = {"ASL", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x17] = {"ORA", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0x18] = {"CLC", AM::kImplied, 1};
  opcode_table_[0x19] = {"ORA", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0x1A] = {"INC", AM::kAccumulator, 1};
  opcode_table_[0x1B] = {"TCS", AM::kImplied, 1};
  opcode_table_[0x1C] = {"TRB", AM::kAbsolute, 3};
  opcode_table_[0x1D] = {"ORA", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x1E] = {"ASL", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x1F] = {"ORA", AM::kAbsoluteLongIndexedX, 4};

  // Row 0x20-0x2F
  opcode_table_[0x20] = {"JSR", AM::kAbsolute, 3};
  opcode_table_[0x21] = {"AND", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0x22] = {"JSL", AM::kAbsoluteLong, 4};
  opcode_table_[0x23] = {"AND", AM::kStackRelative, 2};
  opcode_table_[0x24] = {"BIT", AM::kDirectPage, 2};
  opcode_table_[0x25] = {"AND", AM::kDirectPage, 2};
  opcode_table_[0x26] = {"ROL", AM::kDirectPage, 2};
  opcode_table_[0x27] = {"AND", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0x28] = {"PLP", AM::kImplied, 1};
  opcode_table_[0x29] = {"AND", AM::kImmediateM, 2};
  opcode_table_[0x2A] = {"ROL", AM::kAccumulator, 1};
  opcode_table_[0x2B] = {"PLD", AM::kImplied, 1};
  opcode_table_[0x2C] = {"BIT", AM::kAbsolute, 3};
  opcode_table_[0x2D] = {"AND", AM::kAbsolute, 3};
  opcode_table_[0x2E] = {"ROL", AM::kAbsolute, 3};
  opcode_table_[0x2F] = {"AND", AM::kAbsoluteLong, 4};

  // Row 0x30-0x3F
  opcode_table_[0x30] = {"BMI", AM::kProgramCounterRelative, 2};
  opcode_table_[0x31] = {"AND", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0x32] = {"AND", AM::kDirectPageIndirect, 2};
  opcode_table_[0x33] = {"AND", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0x34] = {"BIT", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x35] = {"AND", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x36] = {"ROL", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x37] = {"AND", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0x38] = {"SEC", AM::kImplied, 1};
  opcode_table_[0x39] = {"AND", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0x3A] = {"DEC", AM::kAccumulator, 1};
  opcode_table_[0x3B] = {"TSC", AM::kImplied, 1};
  opcode_table_[0x3C] = {"BIT", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x3D] = {"AND", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x3E] = {"ROL", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x3F] = {"AND", AM::kAbsoluteLongIndexedX, 4};

  // Row 0x40-0x4F
  opcode_table_[0x40] = {"RTI", AM::kImplied, 1};
  opcode_table_[0x41] = {"EOR", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0x42] = {"WDM", AM::kImmediate8, 2};
  opcode_table_[0x43] = {"EOR", AM::kStackRelative, 2};
  opcode_table_[0x44] = {"MVP", AM::kBlockMove, 3};
  opcode_table_[0x45] = {"EOR", AM::kDirectPage, 2};
  opcode_table_[0x46] = {"LSR", AM::kDirectPage, 2};
  opcode_table_[0x47] = {"EOR", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0x48] = {"PHA", AM::kImplied, 1};
  opcode_table_[0x49] = {"EOR", AM::kImmediateM, 2};
  opcode_table_[0x4A] = {"LSR", AM::kAccumulator, 1};
  opcode_table_[0x4B] = {"PHK", AM::kImplied, 1};
  opcode_table_[0x4C] = {"JMP", AM::kAbsolute, 3};
  opcode_table_[0x4D] = {"EOR", AM::kAbsolute, 3};
  opcode_table_[0x4E] = {"LSR", AM::kAbsolute, 3};
  opcode_table_[0x4F] = {"EOR", AM::kAbsoluteLong, 4};

  // Row 0x50-0x5F
  opcode_table_[0x50] = {"BVC", AM::kProgramCounterRelative, 2};
  opcode_table_[0x51] = {"EOR", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0x52] = {"EOR", AM::kDirectPageIndirect, 2};
  opcode_table_[0x53] = {"EOR", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0x54] = {"MVN", AM::kBlockMove, 3};
  opcode_table_[0x55] = {"EOR", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x56] = {"LSR", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x57] = {"EOR", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0x58] = {"CLI", AM::kImplied, 1};
  opcode_table_[0x59] = {"EOR", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0x5A] = {"PHY", AM::kImplied, 1};
  opcode_table_[0x5B] = {"TCD", AM::kImplied, 1};
  opcode_table_[0x5C] = {"JMP", AM::kAbsoluteLong, 4};
  opcode_table_[0x5D] = {"EOR", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x5E] = {"LSR", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x5F] = {"EOR", AM::kAbsoluteLongIndexedX, 4};

  // Row 0x60-0x6F
  opcode_table_[0x60] = {"RTS", AM::kImplied, 1};
  opcode_table_[0x61] = {"ADC", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0x62] = {"PER", AM::kProgramCounterRelativeLong, 3};
  opcode_table_[0x63] = {"ADC", AM::kStackRelative, 2};
  opcode_table_[0x64] = {"STZ", AM::kDirectPage, 2};
  opcode_table_[0x65] = {"ADC", AM::kDirectPage, 2};
  opcode_table_[0x66] = {"ROR", AM::kDirectPage, 2};
  opcode_table_[0x67] = {"ADC", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0x68] = {"PLA", AM::kImplied, 1};
  opcode_table_[0x69] = {"ADC", AM::kImmediateM, 2};
  opcode_table_[0x6A] = {"ROR", AM::kAccumulator, 1};
  opcode_table_[0x6B] = {"RTL", AM::kImplied, 1};
  opcode_table_[0x6C] = {"JMP", AM::kAbsoluteIndirect, 3};
  opcode_table_[0x6D] = {"ADC", AM::kAbsolute, 3};
  opcode_table_[0x6E] = {"ROR", AM::kAbsolute, 3};
  opcode_table_[0x6F] = {"ADC", AM::kAbsoluteLong, 4};

  // Row 0x70-0x7F
  opcode_table_[0x70] = {"BVS", AM::kProgramCounterRelative, 2};
  opcode_table_[0x71] = {"ADC", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0x72] = {"ADC", AM::kDirectPageIndirect, 2};
  opcode_table_[0x73] = {"ADC", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0x74] = {"STZ", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x75] = {"ADC", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x76] = {"ROR", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x77] = {"ADC", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0x78] = {"SEI", AM::kImplied, 1};
  opcode_table_[0x79] = {"ADC", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0x7A] = {"PLY", AM::kImplied, 1};
  opcode_table_[0x7B] = {"TDC", AM::kImplied, 1};
  opcode_table_[0x7C] = {"JMP", AM::kAbsoluteIndexedIndirect, 3};
  opcode_table_[0x7D] = {"ADC", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x7E] = {"ROR", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x7F] = {"ADC", AM::kAbsoluteLongIndexedX, 4};

  // Row 0x80-0x8F
  opcode_table_[0x80] = {"BRA", AM::kProgramCounterRelative, 2};
  opcode_table_[0x81] = {"STA", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0x82] = {"BRL", AM::kProgramCounterRelativeLong, 3};
  opcode_table_[0x83] = {"STA", AM::kStackRelative, 2};
  opcode_table_[0x84] = {"STY", AM::kDirectPage, 2};
  opcode_table_[0x85] = {"STA", AM::kDirectPage, 2};
  opcode_table_[0x86] = {"STX", AM::kDirectPage, 2};
  opcode_table_[0x87] = {"STA", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0x88] = {"DEY", AM::kImplied, 1};
  opcode_table_[0x89] = {"BIT", AM::kImmediateM, 2};
  opcode_table_[0x8A] = {"TXA", AM::kImplied, 1};
  opcode_table_[0x8B] = {"PHB", AM::kImplied, 1};
  opcode_table_[0x8C] = {"STY", AM::kAbsolute, 3};
  opcode_table_[0x8D] = {"STA", AM::kAbsolute, 3};
  opcode_table_[0x8E] = {"STX", AM::kAbsolute, 3};
  opcode_table_[0x8F] = {"STA", AM::kAbsoluteLong, 4};

  // Row 0x90-0x9F
  opcode_table_[0x90] = {"BCC", AM::kProgramCounterRelative, 2};
  opcode_table_[0x91] = {"STA", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0x92] = {"STA", AM::kDirectPageIndirect, 2};
  opcode_table_[0x93] = {"STA", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0x94] = {"STY", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x95] = {"STA", AM::kDirectPageIndexedX, 2};
  opcode_table_[0x96] = {"STX", AM::kDirectPageIndexedY, 2};
  opcode_table_[0x97] = {"STA", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0x98] = {"TYA", AM::kImplied, 1};
  opcode_table_[0x99] = {"STA", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0x9A] = {"TXS", AM::kImplied, 1};
  opcode_table_[0x9B] = {"TXY", AM::kImplied, 1};
  opcode_table_[0x9C] = {"STZ", AM::kAbsolute, 3};
  opcode_table_[0x9D] = {"STA", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x9E] = {"STZ", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0x9F] = {"STA", AM::kAbsoluteLongIndexedX, 4};

  // Row 0xA0-0xAF
  opcode_table_[0xA0] = {"LDY", AM::kImmediateX, 2};
  opcode_table_[0xA1] = {"LDA", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0xA2] = {"LDX", AM::kImmediateX, 2};
  opcode_table_[0xA3] = {"LDA", AM::kStackRelative, 2};
  opcode_table_[0xA4] = {"LDY", AM::kDirectPage, 2};
  opcode_table_[0xA5] = {"LDA", AM::kDirectPage, 2};
  opcode_table_[0xA6] = {"LDX", AM::kDirectPage, 2};
  opcode_table_[0xA7] = {"LDA", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0xA8] = {"TAY", AM::kImplied, 1};
  opcode_table_[0xA9] = {"LDA", AM::kImmediateM, 2};
  opcode_table_[0xAA] = {"TAX", AM::kImplied, 1};
  opcode_table_[0xAB] = {"PLB", AM::kImplied, 1};
  opcode_table_[0xAC] = {"LDY", AM::kAbsolute, 3};
  opcode_table_[0xAD] = {"LDA", AM::kAbsolute, 3};
  opcode_table_[0xAE] = {"LDX", AM::kAbsolute, 3};
  opcode_table_[0xAF] = {"LDA", AM::kAbsoluteLong, 4};

  // Row 0xB0-0xBF
  opcode_table_[0xB0] = {"BCS", AM::kProgramCounterRelative, 2};
  opcode_table_[0xB1] = {"LDA", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0xB2] = {"LDA", AM::kDirectPageIndirect, 2};
  opcode_table_[0xB3] = {"LDA", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0xB4] = {"LDY", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xB5] = {"LDA", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xB6] = {"LDX", AM::kDirectPageIndexedY, 2};
  opcode_table_[0xB7] = {"LDA", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0xB8] = {"CLV", AM::kImplied, 1};
  opcode_table_[0xB9] = {"LDA", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0xBA] = {"TSX", AM::kImplied, 1};
  opcode_table_[0xBB] = {"TYX", AM::kImplied, 1};
  opcode_table_[0xBC] = {"LDY", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xBD] = {"LDA", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xBE] = {"LDX", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0xBF] = {"LDA", AM::kAbsoluteLongIndexedX, 4};

  // Row 0xC0-0xCF
  opcode_table_[0xC0] = {"CPY", AM::kImmediateX, 2};
  opcode_table_[0xC1] = {"CMP", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0xC2] = {"REP", AM::kImmediate8, 2};
  opcode_table_[0xC3] = {"CMP", AM::kStackRelative, 2};
  opcode_table_[0xC4] = {"CPY", AM::kDirectPage, 2};
  opcode_table_[0xC5] = {"CMP", AM::kDirectPage, 2};
  opcode_table_[0xC6] = {"DEC", AM::kDirectPage, 2};
  opcode_table_[0xC7] = {"CMP", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0xC8] = {"INY", AM::kImplied, 1};
  opcode_table_[0xC9] = {"CMP", AM::kImmediateM, 2};
  opcode_table_[0xCA] = {"DEX", AM::kImplied, 1};
  opcode_table_[0xCB] = {"WAI", AM::kImplied, 1};
  opcode_table_[0xCC] = {"CPY", AM::kAbsolute, 3};
  opcode_table_[0xCD] = {"CMP", AM::kAbsolute, 3};
  opcode_table_[0xCE] = {"DEC", AM::kAbsolute, 3};
  opcode_table_[0xCF] = {"CMP", AM::kAbsoluteLong, 4};

  // Row 0xD0-0xDF
  opcode_table_[0xD0] = {"BNE", AM::kProgramCounterRelative, 2};
  opcode_table_[0xD1] = {"CMP", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0xD2] = {"CMP", AM::kDirectPageIndirect, 2};
  opcode_table_[0xD3] = {"CMP", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0xD4] = {"PEI", AM::kDirectPageIndirect, 2};
  opcode_table_[0xD5] = {"CMP", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xD6] = {"DEC", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xD7] = {"CMP", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0xD8] = {"CLD", AM::kImplied, 1};
  opcode_table_[0xD9] = {"CMP", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0xDA] = {"PHX", AM::kImplied, 1};
  opcode_table_[0xDB] = {"STP", AM::kImplied, 1};
  opcode_table_[0xDC] = {"JMP", AM::kAbsoluteIndirectLong, 3};
  opcode_table_[0xDD] = {"CMP", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xDE] = {"DEC", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xDF] = {"CMP", AM::kAbsoluteLongIndexedX, 4};

  // Row 0xE0-0xEF
  opcode_table_[0xE0] = {"CPX", AM::kImmediateX, 2};
  opcode_table_[0xE1] = {"SBC", AM::kDirectPageIndexedIndirectX, 2};
  opcode_table_[0xE2] = {"SEP", AM::kImmediate8, 2};
  opcode_table_[0xE3] = {"SBC", AM::kStackRelative, 2};
  opcode_table_[0xE4] = {"CPX", AM::kDirectPage, 2};
  opcode_table_[0xE5] = {"SBC", AM::kDirectPage, 2};
  opcode_table_[0xE6] = {"INC", AM::kDirectPage, 2};
  opcode_table_[0xE7] = {"SBC", AM::kDirectPageIndirectLong, 2};
  opcode_table_[0xE8] = {"INX", AM::kImplied, 1};
  opcode_table_[0xE9] = {"SBC", AM::kImmediateM, 2};
  opcode_table_[0xEA] = {"NOP", AM::kImplied, 1};
  opcode_table_[0xEB] = {"XBA", AM::kImplied, 1};
  opcode_table_[0xEC] = {"CPX", AM::kAbsolute, 3};
  opcode_table_[0xED] = {"SBC", AM::kAbsolute, 3};
  opcode_table_[0xEE] = {"INC", AM::kAbsolute, 3};
  opcode_table_[0xEF] = {"SBC", AM::kAbsoluteLong, 4};

  // Row 0xF0-0xFF
  opcode_table_[0xF0] = {"BEQ", AM::kProgramCounterRelative, 2};
  opcode_table_[0xF1] = {"SBC", AM::kDirectPageIndirectIndexedY, 2};
  opcode_table_[0xF2] = {"SBC", AM::kDirectPageIndirect, 2};
  opcode_table_[0xF3] = {"SBC", AM::kStackRelativeIndirectIndexedY, 2};
  opcode_table_[0xF4] = {"PEA", AM::kAbsolute, 3};
  opcode_table_[0xF5] = {"SBC", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xF6] = {"INC", AM::kDirectPageIndexedX, 2};
  opcode_table_[0xF7] = {"SBC", AM::kDirectPageIndirectLongIndexedY, 2};
  opcode_table_[0xF8] = {"SED", AM::kImplied, 1};
  opcode_table_[0xF9] = {"SBC", AM::kAbsoluteIndexedY, 3};
  opcode_table_[0xFA] = {"PLX", AM::kImplied, 1};
  opcode_table_[0xFB] = {"XCE", AM::kImplied, 1};
  opcode_table_[0xFC] = {"JSR", AM::kAbsoluteIndexedIndirect, 3};
  opcode_table_[0xFD] = {"SBC", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xFE] = {"INC", AM::kAbsoluteIndexedX, 3};
  opcode_table_[0xFF] = {"SBC", AM::kAbsoluteLongIndexedX, 4};
}

const InstructionInfo& Disassembler65816::GetInstructionInfo(
    uint8_t opcode) const {
  return opcode_table_[opcode];
}

uint8_t Disassembler65816::GetInstructionSize(uint8_t opcode, bool m_flag,
                                               bool x_flag) const {
  const auto& info = opcode_table_[opcode];
  uint8_t size = info.base_size;

  // Adjust size for M-flag dependent immediate modes
  if (info.mode == AddressingMode65816::kImmediateM && !m_flag) {
    size++;  // 16-bit accumulator mode adds 1 byte
  }
  // Adjust size for X-flag dependent immediate modes
  if (info.mode == AddressingMode65816::kImmediateX && !x_flag) {
    size++;  // 16-bit index mode adds 1 byte
  }

  return size;
}

DisassembledInstruction Disassembler65816::Disassemble(
    uint32_t address, MemoryReader read_byte, bool m_flag, bool x_flag) const {
  DisassembledInstruction result;
  result.address = address;

  // Read opcode
  result.opcode = read_byte(address);
  const auto& info = opcode_table_[result.opcode];
  result.mnemonic = info.mnemonic;
  result.size = GetInstructionSize(result.opcode, m_flag, x_flag);

  // Read operand bytes
  for (uint8_t i = 1; i < result.size; i++) {
    result.operands.push_back(read_byte(address + i));
  }

  // Format operand string
  result.operand_str =
      FormatOperand(info.mode, result.operands, address, m_flag, x_flag);

  // Determine instruction type
  const std::string& mn = result.mnemonic;
  result.is_branch = (mn == "BRA" || mn == "BRL" || mn == "BPL" ||
                      mn == "BMI" || mn == "BVC" || mn == "BVS" ||
                      mn == "BCC" || mn == "BCS" || mn == "BNE" ||
                      mn == "BEQ" || mn == "JMP");
  result.is_call = (mn == "JSR" || mn == "JSL");
  result.is_return = (mn == "RTS" || mn == "RTL" || mn == "RTI");

  // Calculate branch target if applicable
  if (result.is_branch || result.is_call) {
    result.branch_target =
        CalculateBranchTarget(address, result.operands, info.mode, result.size);
  }

  // Build full text representation
  std::ostringstream ss;
  ss << absl::StrFormat("$%06X: ", address);

  // Hex dump of bytes
  ss << absl::StrFormat("%02X ", result.opcode);
  for (const auto& byte : result.operands) {
    ss << absl::StrFormat("%02X ", byte);
  }
  // Pad to align mnemonics
  for (int i = result.size; i < 4; i++) {
    ss << "   ";
  }

  ss << " " << result.mnemonic;
  if (!result.operand_str.empty()) {
    ss << " " << result.operand_str;
  }

  // Add branch target comment if applicable
  if ((result.is_branch || result.is_call) &&
      info.mode != AddressingMode65816::kAbsoluteIndirect &&
      info.mode != AddressingMode65816::kAbsoluteIndirectLong &&
      info.mode != AddressingMode65816::kAbsoluteIndexedIndirect) {
    // Try to resolve symbol
    if (symbol_resolver_) {
      std::string symbol = symbol_resolver_(result.branch_target);
      if (!symbol.empty()) {
        ss << " ; -> " << symbol;
      }
    }
  }

  result.full_text = ss.str();
  return result;
}

std::vector<DisassembledInstruction> Disassembler65816::DisassembleRange(
    uint32_t start_address, size_t count, MemoryReader read_byte, bool m_flag,
    bool x_flag) const {
  std::vector<DisassembledInstruction> results;
  results.reserve(count);

  uint32_t current_address = start_address;
  for (size_t i = 0; i < count; i++) {
    auto instruction = Disassemble(current_address, read_byte, m_flag, x_flag);
    results.push_back(instruction);
    current_address += instruction.size;
  }

  return results;
}

std::string Disassembler65816::FormatOperand(AddressingMode65816 mode,
                                              const std::vector<uint8_t>& ops,
                                              uint32_t address, bool m_flag,
                                              bool x_flag) const {
  using AM = AddressingMode65816;

  switch (mode) {
    case AM::kImplied:
    case AM::kAccumulator:
      return "";

    case AM::kImmediate8:
      if (ops.size() >= 1) {
        return absl::StrFormat("#$%02X", ops[0]);
      }
      break;

    case AM::kImmediate16:
      if (ops.size() >= 2) {
        return absl::StrFormat("#$%04X", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kImmediateM:
      if (m_flag && ops.size() >= 1) {
        return absl::StrFormat("#$%02X", ops[0]);
      } else if (!m_flag && ops.size() >= 2) {
        return absl::StrFormat("#$%04X", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kImmediateX:
      if (x_flag && ops.size() >= 1) {
        return absl::StrFormat("#$%02X", ops[0]);
      } else if (!x_flag && ops.size() >= 2) {
        return absl::StrFormat("#$%04X", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kDirectPage:
      if (ops.size() >= 1) {
        return absl::StrFormat("$%02X", ops[0]);
      }
      break;

    case AM::kDirectPageIndexedX:
      if (ops.size() >= 1) {
        return absl::StrFormat("$%02X,X", ops[0]);
      }
      break;

    case AM::kDirectPageIndexedY:
      if (ops.size() >= 1) {
        return absl::StrFormat("$%02X,Y", ops[0]);
      }
      break;

    case AM::kDirectPageIndirect:
      if (ops.size() >= 1) {
        return absl::StrFormat("($%02X)", ops[0]);
      }
      break;

    case AM::kDirectPageIndirectLong:
      if (ops.size() >= 1) {
        return absl::StrFormat("[$%02X]", ops[0]);
      }
      break;

    case AM::kDirectPageIndexedIndirectX:
      if (ops.size() >= 1) {
        return absl::StrFormat("($%02X,X)", ops[0]);
      }
      break;

    case AM::kDirectPageIndirectIndexedY:
      if (ops.size() >= 1) {
        return absl::StrFormat("($%02X),Y", ops[0]);
      }
      break;

    case AM::kDirectPageIndirectLongIndexedY:
      if (ops.size() >= 1) {
        return absl::StrFormat("[$%02X],Y", ops[0]);
      }
      break;

    case AM::kAbsolute:
      if (ops.size() >= 2) {
        uint16_t addr = ops[0] | (ops[1] << 8);
        // Try symbol resolution
        if (symbol_resolver_) {
          std::string symbol = symbol_resolver_(addr);
          if (!symbol.empty()) {
            return symbol;
          }
        }
        return absl::StrFormat("$%04X", addr);
      }
      break;

    case AM::kAbsoluteIndexedX:
      if (ops.size() >= 2) {
        return absl::StrFormat("$%04X,X", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kAbsoluteIndexedY:
      if (ops.size() >= 2) {
        return absl::StrFormat("$%04X,Y", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kAbsoluteLong:
      if (ops.size() >= 3) {
        uint32_t addr = ops[0] | (ops[1] << 8) | (ops[2] << 16);
        if (symbol_resolver_) {
          std::string symbol = symbol_resolver_(addr);
          if (!symbol.empty()) {
            return symbol;
          }
        }
        return absl::StrFormat("$%06X", addr);
      }
      break;

    case AM::kAbsoluteLongIndexedX:
      if (ops.size() >= 3) {
        return absl::StrFormat("$%06X,X",
                               ops[0] | (ops[1] << 8) | (ops[2] << 16));
      }
      break;

    case AM::kAbsoluteIndirect:
      if (ops.size() >= 2) {
        return absl::StrFormat("($%04X)", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kAbsoluteIndirectLong:
      if (ops.size() >= 2) {
        return absl::StrFormat("[$%04X]", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kAbsoluteIndexedIndirect:
      if (ops.size() >= 2) {
        return absl::StrFormat("($%04X,X)", ops[0] | (ops[1] << 8));
      }
      break;

    case AM::kProgramCounterRelative:
      if (ops.size() >= 1) {
        // 8-bit signed offset
        int8_t offset = static_cast<int8_t>(ops[0]);
        uint32_t target = (address + 2 + offset) & 0xFFFF;
        // Preserve bank
        target |= (address & 0xFF0000);
        if (symbol_resolver_) {
          std::string symbol = symbol_resolver_(target);
          if (!symbol.empty()) {
            return symbol;
          }
        }
        return absl::StrFormat("$%04X", target & 0xFFFF);
      }
      break;

    case AM::kProgramCounterRelativeLong:
      if (ops.size() >= 2) {
        // 16-bit signed offset
        int16_t offset = static_cast<int16_t>(ops[0] | (ops[1] << 8));
        uint32_t target = (address + 3 + offset) & 0xFFFF;
        target |= (address & 0xFF0000);
        if (symbol_resolver_) {
          std::string symbol = symbol_resolver_(target);
          if (!symbol.empty()) {
            return symbol;
          }
        }
        return absl::StrFormat("$%04X", target & 0xFFFF);
      }
      break;

    case AM::kStackRelative:
      if (ops.size() >= 1) {
        return absl::StrFormat("$%02X,S", ops[0]);
      }
      break;

    case AM::kStackRelativeIndirectIndexedY:
      if (ops.size() >= 1) {
        return absl::StrFormat("($%02X,S),Y", ops[0]);
      }
      break;

    case AM::kBlockMove:
      if (ops.size() >= 2) {
        // MVN/MVP: srcBank, dstBank
        return absl::StrFormat("$%02X,$%02X", ops[0], ops[1]);
      }
      break;
  }

  return "???";
}

uint32_t Disassembler65816::CalculateBranchTarget(
    uint32_t address, const std::vector<uint8_t>& operands,
    AddressingMode65816 mode, uint8_t instruction_size) const {
  using AM = AddressingMode65816;

  switch (mode) {
    case AM::kProgramCounterRelative:
      if (operands.size() >= 1) {
        int8_t offset = static_cast<int8_t>(operands[0]);
        return (address + instruction_size + offset) & 0xFFFFFF;
      }
      break;

    case AM::kProgramCounterRelativeLong:
      if (operands.size() >= 2) {
        int16_t offset =
            static_cast<int16_t>(operands[0] | (operands[1] << 8));
        return (address + instruction_size + offset) & 0xFFFFFF;
      }
      break;

    case AM::kAbsolute:
      if (operands.size() >= 2) {
        // For JMP/JSR absolute, use current bank
        return (address & 0xFF0000) | (operands[0] | (operands[1] << 8));
      }
      break;

    case AM::kAbsoluteLong:
      if (operands.size() >= 3) {
        return operands[0] | (operands[1] << 8) | (operands[2] << 16);
      }
      break;

    default:
      break;
  }

  return 0;
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze

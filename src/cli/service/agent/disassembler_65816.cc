#include "cli/service/agent/disassembler_65816.h"

namespace yaze {
namespace cli {
namespace agent {

void Disassembler65816::InitializeOpcodeTable() {
  // Initialize the opcode table with 65816 instruction information
  // Format: opcode -> {mnemonic, addressing mode, base size, cycles}

  // 0x00 - 0x0F
  opcode_table_[0x00] = {"BRK", AddressingMode::Immediate8, 2, 7};
  opcode_table_[0x01] = {"ORA", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0x02] = {"COP", AddressingMode::Immediate8, 2, 8};
  opcode_table_[0x03] = {"ORA", AddressingMode::StackRel, 2, 8};
  opcode_table_[0x04] = {"TSB", AddressingMode::Direct, 2, 3};
  opcode_table_[0x05] = {"ORA", AddressingMode::Direct, 2, 3};
  opcode_table_[0x06] = {"ASL", AddressingMode::Direct, 2, 5};
  opcode_table_[0x07] = {"ORA", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0x08] = {"PHP", AddressingMode::Implied, 1, 3};
  opcode_table_[0x09] = {"ORA", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0x0A] = {"ASL", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x0B] = {"PHD", AddressingMode::Implied, 1, 4};
  opcode_table_[0x0C] = {"TSB", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x0D] = {"ORA", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x0E] = {"ASL", AddressingMode::Absolute, 3, 6};
  opcode_table_[0x0F] = {"ORA", AddressingMode::AbsoluteLong, 4, 6};

  // 0x10 - 0x1F
  opcode_table_[0x10] = {"BPL", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x11] = {"ORA", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0x12] = {"ORA", AddressingMode::Indirect, 2, 6};
  opcode_table_[0x13] = {"ORA", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0x14] = {"TRB", AddressingMode::Direct, 2, 4};
  opcode_table_[0x15] = {"ORA", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x16] = {"ASL", AddressingMode::DirectX, 2, 6};
  opcode_table_[0x17] = {"ORA", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0x18] = {"CLC", AddressingMode::Implied, 1, 2};
  opcode_table_[0x19] = {"ORA", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0x1A] = {"INC", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x1B] = {"TCS", AddressingMode::Implied, 1, 4};
  opcode_table_[0x1C] = {"TRB", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x1D] = {"ORA", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x1E] = {"ASL", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0x1F] = {"ORA", AddressingMode::AbsoluteXLong, 4, 7};

  // 0x20 - 0x2F
  opcode_table_[0x20] = {"JSR", AddressingMode::Absolute, 3, 6};
  opcode_table_[0x21] = {"AND", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0x22] = {"JSL", AddressingMode::AbsoluteLong, 4, 8};
  opcode_table_[0x23] = {"AND", AddressingMode::StackRel, 2, 8};
  opcode_table_[0x24] = {"BIT", AddressingMode::Direct, 2, 3};
  opcode_table_[0x25] = {"AND", AddressingMode::Direct, 2, 3};
  opcode_table_[0x26] = {"ROL", AddressingMode::Direct, 2, 5};
  opcode_table_[0x27] = {"AND", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0x28] = {"PLP", AddressingMode::Implied, 1, 4};
  opcode_table_[0x29] = {"AND", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0x2A] = {"ROL", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x2B] = {"PLD", AddressingMode::Implied, 1, 4};
  opcode_table_[0x2C] = {"BIT", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x2D] = {"AND", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x2E] = {"ROL", AddressingMode::Absolute, 3, 6};
  opcode_table_[0x2F] = {"AND", AddressingMode::AbsoluteLong, 4, 6};

  // 0x30 - 0x3F
  opcode_table_[0x30] = {"BMI", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x31] = {"AND", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0x32] = {"AND", AddressingMode::Indirect, 2, 6};
  opcode_table_[0x33] = {"AND", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0x34] = {"BIT", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x35] = {"AND", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x36] = {"ROL", AddressingMode::DirectX, 2, 6};
  opcode_table_[0x37] = {"AND", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0x38] = {"SEC", AddressingMode::Implied, 1, 2};
  opcode_table_[0x39] = {"AND", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0x3A] = {"DEC", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x3B] = {"TSC", AddressingMode::Implied, 1, 4};
  opcode_table_[0x3C] = {"BIT", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x3D] = {"AND", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x3E] = {"ROL", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0x3F] = {"AND", AddressingMode::AbsoluteXLong, 4, 7};

  // 0x40 - 0x4F
  opcode_table_[0x40] = {"RTI", AddressingMode::Implied, 1, 6};
  opcode_table_[0x41] = {"EOR", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0x42] = {"WDM", AddressingMode::Immediate8, 2, 8};
  opcode_table_[0x43] = {"EOR", AddressingMode::StackRel, 2, 8};
  opcode_table_[0x44] = {"MVP", AddressingMode::BlockMove, 3, 3};
  opcode_table_[0x45] = {"EOR", AddressingMode::Direct, 2, 3};
  opcode_table_[0x46] = {"LSR", AddressingMode::Direct, 2, 5};
  opcode_table_[0x47] = {"EOR", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0x48] = {"PHA", AddressingMode::Implied, 1, 3};
  opcode_table_[0x49] = {"EOR", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0x4A] = {"LSR", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x4B] = {"PHK", AddressingMode::Implied, 1, 4};
  opcode_table_[0x4C] = {"JMP", AddressingMode::Absolute, 3, 3};
  opcode_table_[0x4D] = {"EOR", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x4E] = {"LSR", AddressingMode::Absolute, 3, 6};
  opcode_table_[0x4F] = {"EOR", AddressingMode::AbsoluteLong, 4, 6};

  // 0x50 - 0x5F
  opcode_table_[0x50] = {"BVC", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x51] = {"EOR", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0x52] = {"EOR", AddressingMode::Indirect, 2, 6};
  opcode_table_[0x53] = {"EOR", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0x54] = {"MVN", AddressingMode::BlockMove, 3, 4};
  opcode_table_[0x55] = {"EOR", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x56] = {"LSR", AddressingMode::DirectX, 2, 6};
  opcode_table_[0x57] = {"EOR", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0x58] = {"CLI", AddressingMode::Implied, 1, 2};
  opcode_table_[0x59] = {"EOR", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0x5A] = {"PHY", AddressingMode::Implied, 1, 2};
  opcode_table_[0x5B] = {"TCD", AddressingMode::Implied, 1, 4};
  opcode_table_[0x5C] = {"JMP", AddressingMode::AbsoluteLong, 4, 4};
  opcode_table_[0x5D] = {"EOR", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x5E] = {"LSR", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0x5F] = {"EOR", AddressingMode::AbsoluteXLong, 4, 7};

  // 0x60 - 0x6F
  opcode_table_[0x60] = {"RTS", AddressingMode::Implied, 1, 6};
  opcode_table_[0x61] = {"ADC", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0x62] = {"PER", AddressingMode::Relative16, 3, 8};
  opcode_table_[0x63] = {"ADC", AddressingMode::StackRel, 2, 8};
  opcode_table_[0x64] = {"STZ", AddressingMode::Direct, 2, 3};
  opcode_table_[0x65] = {"ADC", AddressingMode::Direct, 2, 3};
  opcode_table_[0x66] = {"ROR", AddressingMode::Direct, 2, 5};
  opcode_table_[0x67] = {"ADC", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0x68] = {"PLA", AddressingMode::Implied, 1, 4};
  opcode_table_[0x69] = {"ADC", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0x6A] = {"ROR", AddressingMode::Accumulator, 1, 2};
  opcode_table_[0x6B] = {"RTL", AddressingMode::Implied, 1, 4};
  opcode_table_[0x6C] = {"JMP", AddressingMode::Indirect, 3, 5};
  opcode_table_[0x6D] = {"ADC", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x6E] = {"ROR", AddressingMode::Absolute, 3, 6};
  opcode_table_[0x6F] = {"ADC", AddressingMode::AbsoluteLong, 4, 6};

  // 0x70 - 0x7F
  opcode_table_[0x70] = {"BVS", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x71] = {"ADC", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0x72] = {"ADC", AddressingMode::Indirect, 2, 6};
  opcode_table_[0x73] = {"ADC", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0x74] = {"STZ", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x75] = {"ADC", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x76] = {"ROR", AddressingMode::DirectX, 2, 6};
  opcode_table_[0x77] = {"ADC", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0x78] = {"SEI", AddressingMode::Implied, 1, 2};
  opcode_table_[0x79] = {"ADC", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0x7A] = {"PLY", AddressingMode::Implied, 1, 2};
  opcode_table_[0x7B] = {"TDC", AddressingMode::Implied, 1, 4};
  opcode_table_[0x7C] = {"JMP", AddressingMode::AbsoluteX, 3, 6};
  opcode_table_[0x7D] = {"ADC", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x7E] = {"ROR", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0x7F] = {"ADC", AddressingMode::AbsoluteXLong, 4, 7};

  // 0x80 - 0x8F
  opcode_table_[0x80] = {"BRA", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x81] = {"STA", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0x82] = {"BRL", AddressingMode::Relative16, 3, 5};
  opcode_table_[0x83] = {"STA", AddressingMode::StackRel, 2, 8};
  opcode_table_[0x84] = {"STY", AddressingMode::Direct, 2, 3};
  opcode_table_[0x85] = {"STA", AddressingMode::Direct, 2, 3};
  opcode_table_[0x86] = {"STX", AddressingMode::Direct, 2, 3};
  opcode_table_[0x87] = {"STA", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0x88] = {"DEY", AddressingMode::Implied, 1, 2};
  opcode_table_[0x89] = {"BIT", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0x8A] = {"TXA", AddressingMode::Implied, 1, 2};
  opcode_table_[0x8B] = {"PHB", AddressingMode::Implied, 1, 4};
  opcode_table_[0x8C] = {"STY", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x8D] = {"STA", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x8E] = {"STX", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x8F] = {"STA", AddressingMode::AbsoluteLong, 4, 6};

  // 0x90 - 0x9F
  opcode_table_[0x90] = {"BCC", AddressingMode::Relative8, 2, 2};
  opcode_table_[0x91] = {"STA", AddressingMode::IndirectY, 2, 6};
  opcode_table_[0x92] = {"STA", AddressingMode::Indirect, 2, 5};
  opcode_table_[0x93] = {"STA", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0x94] = {"STY", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x95] = {"STA", AddressingMode::DirectX, 2, 4};
  opcode_table_[0x96] = {"STX", AddressingMode::DirectY, 2, 4};
  opcode_table_[0x97] = {"STA", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0x98] = {"TYA", AddressingMode::Implied, 1, 2};
  opcode_table_[0x99] = {"STA", AddressingMode::AbsoluteY, 3, 5};
  opcode_table_[0x9A] = {"TXS", AddressingMode::Implied, 1, 2};
  opcode_table_[0x9B] = {"TXY", AddressingMode::Implied, 1, 4};
  opcode_table_[0x9C] = {"STZ", AddressingMode::Absolute, 3, 4};
  opcode_table_[0x9D] = {"STA", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x9E] = {"STZ", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0x9F] = {"STA", AddressingMode::AbsoluteXLong, 4, 5};

  // 0xA0 - 0xAF
  opcode_table_[0xA0] = {"LDY", AddressingMode::ImmediateX, 2, 2};  // Size depends on X flag
  opcode_table_[0xA1] = {"LDA", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0xA2] = {"LDX", AddressingMode::ImmediateX, 2, 2};  // Size depends on X flag
  opcode_table_[0xA3] = {"LDA", AddressingMode::StackRel, 2, 8};
  opcode_table_[0xA4] = {"LDY", AddressingMode::Direct, 2, 3};
  opcode_table_[0xA5] = {"LDA", AddressingMode::Direct, 2, 3};
  opcode_table_[0xA6] = {"LDX", AddressingMode::Direct, 2, 3};
  opcode_table_[0xA7] = {"LDA", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0xA8] = {"TAY", AddressingMode::Implied, 1, 2};
  opcode_table_[0xA9] = {"LDA", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0xAA] = {"TAX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xAB] = {"PLB", AddressingMode::Implied, 1, 4};
  opcode_table_[0xAC] = {"LDY", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xAD] = {"LDA", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xAE] = {"LDX", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xAF] = {"LDA", AddressingMode::AbsoluteLong, 4, 6};

  // 0xB0 - 0xBF
  opcode_table_[0xB0] = {"BCS", AddressingMode::Relative8, 2, 2};
  opcode_table_[0xB1] = {"LDA", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0xB2] = {"LDA", AddressingMode::Indirect, 2, 5};
  opcode_table_[0xB3] = {"LDA", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0xB4] = {"LDY", AddressingMode::DirectX, 2, 4};
  opcode_table_[0xB5] = {"LDA", AddressingMode::DirectX, 2, 4};
  opcode_table_[0xB6] = {"LDX", AddressingMode::DirectY, 2, 4};
  opcode_table_[0xB7] = {"LDA", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0xB8] = {"CLV", AddressingMode::Implied, 1, 2};
  opcode_table_[0xB9] = {"LDA", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0xBA] = {"TSX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xBB] = {"TYX", AddressingMode::Implied, 1, 4};
  opcode_table_[0xBC] = {"LDY", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0xBD] = {"LDA", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0xBE] = {"LDX", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0xBF] = {"LDA", AddressingMode::AbsoluteXLong, 4, 5};

  // 0xC0 - 0xCF
  opcode_table_[0xC0] = {"CPY", AddressingMode::ImmediateX, 2, 2};  // Size depends on X flag
  opcode_table_[0xC1] = {"CMP", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0xC2] = {"REP", AddressingMode::Immediate8, 2, 2};
  opcode_table_[0xC3] = {"CMP", AddressingMode::StackRel, 2, 8};
  opcode_table_[0xC4] = {"CPY", AddressingMode::Direct, 2, 3};
  opcode_table_[0xC5] = {"CMP", AddressingMode::Direct, 2, 3};
  opcode_table_[0xC6] = {"DEC", AddressingMode::Direct, 2, 5};
  opcode_table_[0xC7] = {"CMP", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0xC8] = {"INY", AddressingMode::Implied, 1, 2};
  opcode_table_[0xC9] = {"CMP", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0xCA] = {"DEX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xCB] = {"WAI", AddressingMode::Implied, 1, 4};
  opcode_table_[0xCC] = {"CPY", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xCD] = {"CMP", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xCE] = {"DEC", AddressingMode::Absolute, 3, 6};
  opcode_table_[0xCF] = {"CMP", AddressingMode::AbsoluteLong, 4, 6};

  // 0xD0 - 0xDF
  opcode_table_[0xD0] = {"BNE", AddressingMode::Relative8, 2, 2};
  opcode_table_[0xD1] = {"CMP", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0xD2] = {"CMP", AddressingMode::Indirect, 2, 5};
  opcode_table_[0xD3] = {"CMP", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0xD4] = {"PEI", AddressingMode::Indirect, 2, 4};
  opcode_table_[0xD5] = {"CMP", AddressingMode::DirectX, 2, 4};
  opcode_table_[0xD6] = {"DEC", AddressingMode::DirectX, 2, 6};
  opcode_table_[0xD7] = {"CMP", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0xD8] = {"CLD", AddressingMode::Implied, 1, 2};
  opcode_table_[0xD9] = {"CMP", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0xDA] = {"PHX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xDB] = {"STP", AddressingMode::Implied, 1, 4};
  opcode_table_[0xDC] = {"JMP", AddressingMode::IndirectLong, 3, 3};
  opcode_table_[0xDD] = {"CMP", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0xDE] = {"DEC", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0xDF] = {"CMP", AddressingMode::AbsoluteXLong, 4, 7};

  // 0xE0 - 0xEF
  opcode_table_[0xE0] = {"CPX", AddressingMode::ImmediateX, 2, 2};  // Size depends on X flag
  opcode_table_[0xE1] = {"SBC", AddressingMode::IndirectX, 2, 6};
  opcode_table_[0xE2] = {"SEP", AddressingMode::Immediate8, 2, 2};
  opcode_table_[0xE3] = {"SBC", AddressingMode::StackRel, 2, 8};
  opcode_table_[0xE4] = {"CPX", AddressingMode::Direct, 2, 3};
  opcode_table_[0xE5] = {"SBC", AddressingMode::Direct, 2, 3};
  opcode_table_[0xE6] = {"INC", AddressingMode::Direct, 2, 5};
  opcode_table_[0xE7] = {"SBC", AddressingMode::IndirectLong, 2, 5};
  opcode_table_[0xE8] = {"INX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xE9] = {"SBC", AddressingMode::Immediate8, 2, 2};  // Size depends on M flag
  opcode_table_[0xEA] = {"NOP", AddressingMode::Implied, 1, 2};
  opcode_table_[0xEB] = {"XBA", AddressingMode::Implied, 1, 2};
  opcode_table_[0xEC] = {"CPX", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xED] = {"SBC", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xEE] = {"INC", AddressingMode::Absolute, 3, 6};
  opcode_table_[0xEF] = {"SBC", AddressingMode::AbsoluteLong, 4, 6};

  // 0xF0 - 0xFF
  opcode_table_[0xF0] = {"BEQ", AddressingMode::Relative8, 2, 2};
  opcode_table_[0xF1] = {"SBC", AddressingMode::IndirectY, 2, 5};
  opcode_table_[0xF2] = {"SBC", AddressingMode::Indirect, 2, 5};
  opcode_table_[0xF3] = {"SBC", AddressingMode::StackRelY, 2, 8};
  opcode_table_[0xF4] = {"PEA", AddressingMode::Absolute, 3, 4};
  opcode_table_[0xF5] = {"SBC", AddressingMode::DirectX, 2, 4};
  opcode_table_[0xF6] = {"INC", AddressingMode::DirectX, 2, 6};
  opcode_table_[0xF7] = {"SBC", AddressingMode::IndirectLongY, 2, 6};
  opcode_table_[0xF8] = {"SED", AddressingMode::Implied, 1, 2};
  opcode_table_[0xF9] = {"SBC", AddressingMode::AbsoluteY, 3, 4};
  opcode_table_[0xFA] = {"PLX", AddressingMode::Implied, 1, 2};
  opcode_table_[0xFB] = {"XCE", AddressingMode::Implied, 1, 2};
  opcode_table_[0xFC] = {"JSR", AddressingMode::AbsoluteX, 3, 3};
  opcode_table_[0xFD] = {"SBC", AddressingMode::AbsoluteX, 3, 4};
  opcode_table_[0xFE] = {"INC", AddressingMode::AbsoluteX, 3, 7};
  opcode_table_[0xFF] = {"SBC", AddressingMode::AbsoluteXLong, 4, 7};
}

uint8_t Disassembler65816::DisassembleInstruction(
    uint32_t address, const uint8_t* data, std::string& mnemonic,
    std::string& operand_str, std::vector<uint8_t>& operands) {
  uint8_t opcode = data[0];

  auto it = opcode_table_.find(opcode);
  if (it == opcode_table_.end()) {
    // Unknown opcode
    mnemonic = absl::StrFormat("DB $%02X", opcode);
    operand_str = "";
    operands.clear();
    return 1;
  }

  const InstructionInfo& info = it->second;
  mnemonic = info.mnemonic;

  // Get the effective size based on M/X flags
  uint8_t size = GetEffectiveSize(opcode, info.mode);

  // Extract operand bytes
  operands.clear();
  for (uint8_t i = 1; i < size; ++i) {
    operands.push_back(data[i]);
  }

  // Format the operand string
  operand_str = FormatOperand(info.mode, address, operands);

  return size;
}

uint8_t Disassembler65816::GetInstructionSize(uint8_t opcode) const {
  auto it = opcode_table_.find(opcode);
  if (it == opcode_table_.end()) {
    return 1;  // Unknown opcode
  }

  return GetEffectiveSize(opcode, it->second.mode);
}

uint8_t Disassembler65816::GetEffectiveSize(uint8_t opcode,
                                            AddressingMode mode) const {
  auto it = opcode_table_.find(opcode);
  if (it == opcode_table_.end()) {
    return 1;
  }

  uint8_t base_size = it->second.base_size;

  // Adjust size based on M and X flags for immediate mode instructions
  switch (mode) {
    case AddressingMode::Immediate8:
      // Instructions affected by M flag (accumulator/memory operations)
      if (opcode == 0x09 || opcode == 0x29 || opcode == 0x49 ||  // ORA, AND, EOR
          opcode == 0x69 || opcode == 0x89 || opcode == 0xA9 ||  // ADC, BIT, LDA
          opcode == 0xC9 || opcode == 0xE9) {                    // CMP, SBC
        return m_flag_ ? 2 : 3;  // 8-bit when M=1, 16-bit when M=0
      }
      return base_size;

    case AddressingMode::ImmediateX:
      // Instructions affected by X flag (index register operations)
      if (opcode == 0xA0 || opcode == 0xA2 ||  // LDY, LDX
          opcode == 0xC0 || opcode == 0xE0) {  // CPY, CPX
        return x_flag_ ? 2 : 3;  // 8-bit when X=1, 16-bit when X=0
      }
      return base_size;

    default:
      return base_size;
  }
}

std::string Disassembler65816::FormatOperand(
    AddressingMode mode, uint32_t address,
    const std::vector<uint8_t>& operands) const {
  switch (mode) {
    case AddressingMode::Implied:
    case AddressingMode::Accumulator:
      return "";

    case AddressingMode::Immediate8:
      if (operands.size() == 1) {
        return absl::StrFormat("#$%02X", operands[0]);
      } else if (operands.size() == 2) {
        return absl::StrFormat("#$%04X", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::Immediate16:
    case AddressingMode::ImmediateX:
      if (operands.size() == 1) {
        return absl::StrFormat("#$%02X", operands[0]);
      } else if (operands.size() == 2) {
        return absl::StrFormat("#$%04X", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::Absolute:
      if (operands.size() == 2) {
        return absl::StrFormat("$%04X", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::AbsoluteLong:
      if (operands.size() == 3) {
        return absl::StrFormat("$%02X%04X", operands[2],
                               operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::AbsoluteX:
      if (operands.size() == 2) {
        return absl::StrFormat("$%04X,X", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::AbsoluteXLong:
      if (operands.size() == 3) {
        return absl::StrFormat("$%02X%04X,X", operands[2],
                               operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::AbsoluteY:
      if (operands.size() == 2) {
        return absl::StrFormat("$%04X,Y", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::Direct:
      if (operands.size() == 1) {
        return absl::StrFormat("$%02X", operands[0]);
      }
      break;

    case AddressingMode::DirectX:
      if (operands.size() == 1) {
        return absl::StrFormat("$%02X,X", operands[0]);
      }
      break;

    case AddressingMode::DirectY:
      if (operands.size() == 1) {
        return absl::StrFormat("$%02X,Y", operands[0]);
      }
      break;

    case AddressingMode::Indirect:
      if (operands.size() == 1) {
        return absl::StrFormat("($%02X)", operands[0]);
      } else if (operands.size() == 2) {
        return absl::StrFormat("($%04X)", operands[0] | (operands[1] << 8));
      }
      break;

    case AddressingMode::IndirectX:
      if (operands.size() == 1) {
        return absl::StrFormat("($%02X,X)", operands[0]);
      }
      break;

    case AddressingMode::IndirectY:
      if (operands.size() == 1) {
        return absl::StrFormat("($%02X),Y", operands[0]);
      }
      break;

    case AddressingMode::IndirectLong:
      if (operands.size() == 1) {
        return absl::StrFormat("[$%02X]", operands[0]);
      }
      break;

    case AddressingMode::IndirectLongY:
      if (operands.size() == 1) {
        return absl::StrFormat("[$%02X],Y", operands[0]);
      }
      break;

    case AddressingMode::StackRel:
      if (operands.size() == 1) {
        return absl::StrFormat("$%02X,S", operands[0]);
      }
      break;

    case AddressingMode::StackRelY:
      if (operands.size() == 1) {
        return absl::StrFormat("($%02X,S),Y", operands[0]);
      }
      break;

    case AddressingMode::Relative8:
      if (operands.size() == 1) {
        int8_t offset = static_cast<int8_t>(operands[0]);
        uint32_t target = address + 2 + offset;  // +2 for instruction size
        return absl::StrFormat("$%04X", target & 0xFFFF);
      }
      break;

    case AddressingMode::Relative16:
      if (operands.size() == 2) {
        int16_t offset = static_cast<int16_t>(operands[0] | (operands[1] << 8));
        uint32_t target = address + 3 + offset;  // +3 for instruction size
        return absl::StrFormat("$%04X", target & 0xFFFF);
      }
      break;

    case AddressingMode::BlockMove:
      if (operands.size() == 2) {
        return absl::StrFormat("$%02X,$%02X", operands[1], operands[0]);
      }
      break;
  }

  return "???";
}

// ExecutionTraceBuffer implementation

void ExecutionTraceBuffer::RecordExecution(const TraceEntry& entry) {
  if (buffer_.size() < max_size_) {
    buffer_.push_back(entry);
  } else {
    // Circular buffer behavior
    buffer_[write_index_] = entry;
    write_index_ = (write_index_ + 1) % max_size_;
  }
}

std::vector<ExecutionTraceBuffer::TraceEntry>
ExecutionTraceBuffer::GetRecentEntries(size_t count) const {
  std::vector<TraceEntry> result;

  if (buffer_.empty()) {
    return result;
  }

  size_t start_idx;
  size_t entries_to_copy = std::min(count, buffer_.size());

  if (buffer_.size() < max_size_) {
    // Buffer not full yet
    start_idx = buffer_.size() >= entries_to_copy
                    ? buffer_.size() - entries_to_copy
                    : 0;
    for (size_t i = start_idx; i < buffer_.size(); ++i) {
      result.push_back(buffer_[i]);
    }
  } else {
    // Circular buffer is full
    start_idx = write_index_ >= entries_to_copy
                    ? write_index_ - entries_to_copy
                    : max_size_ - (entries_to_copy - write_index_);

    for (size_t i = 0; i < entries_to_copy; ++i) {
      result.push_back(buffer_[(start_idx + i) % max_size_]);
    }
  }

  return result;
}

std::vector<ExecutionTraceBuffer::TraceEntry>
ExecutionTraceBuffer::GetEntriesInRange(uint32_t start_addr,
                                        uint32_t end_addr) const {
  std::vector<TraceEntry> result;

  for (const auto& entry : buffer_) {
    if (entry.address >= start_addr && entry.address <= end_addr) {
      result.push_back(entry);
    }
  }

  return result;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
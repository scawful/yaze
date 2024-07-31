#pragma once

#include <cstdint>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "app/emu/cpu/internal/opcodes.h"

namespace yaze {
namespace app {
namespace emu {

enum class AddressingMode {
  kAbsolute,
  kAbsoluteLong,
  kAbsoluteIndexedIndirect,
  kAbsoluteIndexedX,
  kAbsoluteIndexedY,
  kAbsoluteIndirect,
  kAbsoluteIndirectLong,
  kAbsoluteLongIndexedX,
  kAccumulator,
  kBlockMove,
  kDirectPage,
  kDirectPageIndexedX,
  kDirectPageIndexedY,
  kDirectPageIndirect,
  kDirectPageIndirectIndexedY,
  kDirectPageIndirectLong,
  kDirectPageIndirectLongIndexedY,
  kDirectPageIndirectIndexedX,
  kDirectPageIndirectLongIndexedX,
  kImmediate,
  kImplied,
  kProgramCounterRelative,
  kProgramCounterRelativeLong,
  kStackRelative,
  kStackRelativeIndirectIndexedY,
  kStackRelativeIndirectIndexedYLong,
  kStack,
  kStackRelativeIndexedY,
};

// Key structure for mnemonic and addressing mode
struct MnemonicMode {
  std::string mnemonic;
  AddressingMode mode;

  bool operator==(const MnemonicMode& other) const {
    return mnemonic == other.mnemonic && mode == other.mode;
  }
};

// Custom hash function for the MnemonicMode structure
struct MnemonicModeHash {
  std::size_t operator()(const MnemonicMode& k) const {
    return std::hash<std::string>()(k.mnemonic) ^
           (std::hash<int>()(static_cast<int>(k.mode)) << 1);
  }
};

class AsmParser {
 public:
  std::vector<uint8_t> Parse(const std::string& instruction) {
    CreateInternalOpcodeMap();
    auto tokens = Tokenize(instruction);
    if (tokens.size() < 1) {
      throw std::runtime_error("Invalid instruction format: " + instruction);
    }

    size_t index = 0;
    std::vector<uint8_t> bytes;
    while (index < tokens.size()) {
      // For each "line" worth of tokens, we need to extract the
      // mnemonic, optional addressing mode qualifier, and operand.
      // The operand can come in a variety of formats:
      // - Immediate: #$01
      // - Immediate Word: #$1234
      // - Absolute: $1234
      // - Absolute Long: $123456
      // This parser is not exhaustive and only supports a subset of
      // the possible addressing modes and operands.
      const std::string& mnemonic = tokens[index];
      index++;

      // Check if addressing mode qualifier is present
      // Either .b, .w, .l, or nothing, which could mean
      // it was omitted or the operand is implied
      std::string qualifier = "";
      std::string potential_mode = tokens[index];
      if (absl::StrContains(potential_mode, ".")) {
        qualifier = potential_mode;
        index++;
      }

      // Now we check for either the immediate mode
      // symbol # or the address symbol $ to determine
      // the next step
      std::string operand = tokens[index];
      if (operand == "#") {
        index++;
        // Check if the next token is a # character, in which case it is
        // a hexadecimal value that needs to be converted to a byte
        if (tokens[index] == "#") {
          index++;
          operand = tokens[index];
          index++;
        }
      } else if (operand == "$") {
        index++;
        operand = tokens[index];
        index++;
      }

      AddressingMode mode = DetermineMode(tokens);

      MnemonicMode key{mnemonic, mode};
      auto opcode_entry = mnemonic_to_opcode_.find(key);
      if (opcode_entry == mnemonic_to_opcode_.end()) {
        throw std::runtime_error("Opcode not found for mnemonic and mode: " +
                                 mnemonic);
      }

      bytes.push_back(opcode_entry->second);
      AppendOperandBytes(bytes, operand, mode);
    }

    return bytes;
  }

  // Example: ADC.b #$01
  // Returns: ["ADC", ".b", "#", "$", "01"]
  std::vector<std::string> Tokenize(const std::string& instruction) {
    std::vector<std::string> tokens;
    std::regex tokenRegex{R"((\w+|\.\w+|[\#$]|[0-9a-fA-F]+|[a-zA-Z]+))"};
    auto words_begin = std::sregex_iterator(instruction.begin(),
                                            instruction.end(), tokenRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
      std::smatch match = *i;
      tokens.push_back(match.str());
    }
    return tokens;
  }

 private:
  void AppendOperandBytes(std::vector<uint8_t>& bytes,
                          const std::string& operand,
                          const AddressingMode& addressing_mode) {
    // Handle different addressing modes
    switch (addressing_mode) {
      case AddressingMode::kImmediate: {
        bytes.push_back(static_cast<uint8_t>(std::stoi(operand, nullptr, 16)));
        break;
      }
      case AddressingMode::kAbsolute: {
        uint16_t word_operand =
            static_cast<uint16_t>(std::stoi(operand, nullptr, 16));
        bytes.push_back(static_cast<uint8_t>(word_operand & 0xFF));
        bytes.push_back(static_cast<uint8_t>((word_operand >> 8) & 0xFF));
        break;
      }
      case AddressingMode::kAbsoluteLong: {
        uint32_t long_operand =
            static_cast<uint32_t>(std::stoul(operand, nullptr, 16));
        bytes.push_back(static_cast<uint8_t>(long_operand & 0xFF));
        bytes.push_back(static_cast<uint8_t>((long_operand >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((long_operand >> 16) & 0xFF));
        break;
      }
      case AddressingMode::kImplied: {
        break;
      }
      default:
        // Unknown, append it anyway
        bytes.push_back(static_cast<uint8_t>(std::stoi(operand, nullptr, 16)));
    }
  }

  AddressingMode DetermineMode(const std::vector<std::string>& tokens) {
    const std::string& addressingMode = tokens[1];
    if (addressingMode == ".b") {
      return AddressingMode::kImmediate;
    } else if (addressingMode == ".w") {
      return AddressingMode::kAbsolute;
    } else if (addressingMode == ".l") {
      return AddressingMode::kAbsoluteLong;
    } else {
      return AddressingMode::kImplied;
    }
  }

  bool TryParseByte(const std::string& str, uint8_t& value) {
    try {
      value = std::stoi(str, nullptr, 16);
      return true;
    } catch (const std::invalid_argument& e) {
      return false;
    }
  }

  bool TryParseHex(const std::string& str, uint32_t& value) {
    try {
      value = std::stoul(str, nullptr, 16);
      return true;
    } catch (const std::invalid_argument& e) {
      return false;
    }
  }

  void CreateInternalOpcodeMap() {
    mnemonic_to_opcode_[{"ADC", AddressingMode::kImmediate}] = 0x69;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPage}] = 0x65;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndexedX}] = 0x75;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsolute}] = 0x6D;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsoluteIndexedX}] = 0x7D;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsoluteIndexedY}] = 0x79;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndirect}] = 0x61;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x71;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kStackRelative}] = 0x63;
    mnemonic_to_opcode_[{
        "ADC", AddressingMode::kStackRelativeIndirectIndexedY}] = 0x73;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kImmediate}] = 0x69;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPage}] = 0x65;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndexedX}] = 0x75;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsolute}] = 0x6D;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsoluteIndexedX}] = 0x7D;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kAbsoluteIndexedY}] = 0x79;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndirect}] = 0x61;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x71;
    mnemonic_to_opcode_[{"ADC", AddressingMode::kStackRelative}] = 0x63;
    mnemonic_to_opcode_[{
        "ADC", AddressingMode::kStackRelativeIndirectIndexedY}] = 0x73;
    mnemonic_to_opcode_[{"AND", AddressingMode::kImmediate}] = 0x29;
    mnemonic_to_opcode_[{"AND", AddressingMode::kDirectPage}] = 0x25;
    mnemonic_to_opcode_[{"AND", AddressingMode::kDirectPageIndexedX}] = 0x35;
    mnemonic_to_opcode_[{"AND", AddressingMode::kAbsolute}] = 0x2D;
    mnemonic_to_opcode_[{"AND", AddressingMode::kAbsoluteIndexedX}] = 0x3D;
    mnemonic_to_opcode_[{"AND", AddressingMode::kAbsoluteIndexedY}] = 0x39;
    mnemonic_to_opcode_[{"AND", AddressingMode::kDirectPageIndirect}] = 0x21;
    mnemonic_to_opcode_[{"AND", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x31;
    mnemonic_to_opcode_[{"AND", AddressingMode::kStackRelative}] = 0x23;
    mnemonic_to_opcode_[{
        "AND", AddressingMode::kStackRelativeIndirectIndexedY}] = 0x33;
    mnemonic_to_opcode_[{"ASL", AddressingMode::kAccumulator}] = 0x0A;
    mnemonic_to_opcode_[{"ASL", AddressingMode::kDirectPage}] = 0x06;
    mnemonic_to_opcode_[{"ASL", AddressingMode::kDirectPageIndexedX}] = 0x16;
    mnemonic_to_opcode_[{"ASL", AddressingMode::kAbsolute}] = 0x0E;
    mnemonic_to_opcode_[{"ASL", AddressingMode::kAbsoluteIndexedX}] = 0x1E;
    mnemonic_to_opcode_[{"BCC", AddressingMode::kProgramCounterRelative}] =
        0x90;
    mnemonic_to_opcode_[{"BCS", AddressingMode::kProgramCounterRelative}] =
        0xB0;
    mnemonic_to_opcode_[{"BEQ", AddressingMode::kProgramCounterRelative}] =
        0xF0;
    mnemonic_to_opcode_[{"BIT", AddressingMode::kImmediate}] = 0x89;
    mnemonic_to_opcode_[{"BIT", AddressingMode::kDirectPage}] = 0x24;
    mnemonic_to_opcode_[{"BIT", AddressingMode::kAbsolute}] = 0x2C;
    mnemonic_to_opcode_[{"BMI", AddressingMode::kProgramCounterRelative}] =
        0x30;
    mnemonic_to_opcode_[{"BNE", AddressingMode::kProgramCounterRelative}] =
        0xD0;
    mnemonic_to_opcode_[{"BPL", AddressingMode::kProgramCounterRelative}] =
        0x10;
    mnemonic_to_opcode_[{"BRA", AddressingMode::kProgramCounterRelative}] =
        0x80;
    mnemonic_to_opcode_[{"BRK", AddressingMode::kImplied}] = 0x00;
    mnemonic_to_opcode_[{"BRL", AddressingMode::kProgramCounterRelativeLong}] =
        0x82;
    mnemonic_to_opcode_[{"BVC", AddressingMode::kProgramCounterRelative}] =
        0x50;
    mnemonic_to_opcode_[{"BVS", AddressingMode::kProgramCounterRelative}] =
        0x70;
    mnemonic_to_opcode_[{"CLC", AddressingMode::kImplied}] = 0x18;
    mnemonic_to_opcode_[{"CLD", AddressingMode::kImplied}] = 0xD8;
    mnemonic_to_opcode_[{"CLI", AddressingMode::kImplied}] = 0x58;
    mnemonic_to_opcode_[{"CLV", AddressingMode::kImplied}] = 0xB8;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kImmediate}] = 0xC9;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kDirectPage}] = 0xC5;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kDirectPageIndexedX}] = 0xD5;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kAbsolute}] = 0xCD;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kAbsoluteIndexedX}] = 0xDD;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kAbsoluteIndexedY}] = 0xD9;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kDirectPageIndirect}] = 0xC1;
    mnemonic_to_opcode_[{"CMP", AddressingMode::kDirectPageIndirectIndexedY}] =
        0xD1;
    mnemonic_to_opcode_[{"COP", AddressingMode::kImmediate}] = 0x02;
    mnemonic_to_opcode_[{"CPX", AddressingMode::kImmediate}] = 0xE0;
    mnemonic_to_opcode_[{"CPX", AddressingMode::kDirectPage}] = 0xE4;
    mnemonic_to_opcode_[{"CPX", AddressingMode::kAbsolute}] = 0xEC;
    mnemonic_to_opcode_[{"CPY", AddressingMode::kImmediate}] = 0xC0;
    mnemonic_to_opcode_[{"CPY", AddressingMode::kDirectPage}] = 0xC4;
    mnemonic_to_opcode_[{"CPY", AddressingMode::kAbsolute}] = 0xCC;
    mnemonic_to_opcode_[{"DEC", AddressingMode::kDirectPage}] = 0xC6;
    mnemonic_to_opcode_[{"DEC", AddressingMode::kDirectPageIndexedX}] = 0xD6;
    mnemonic_to_opcode_[{"DEC", AddressingMode::kAbsolute}] = 0xCE;
    mnemonic_to_opcode_[{"DEC", AddressingMode::kAbsoluteIndexedX}] = 0xDE;
    mnemonic_to_opcode_[{"DEX", AddressingMode::kImplied}] = 0xCA;
    mnemonic_to_opcode_[{"DEY", AddressingMode::kImplied}] = 0x88;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kImmediate}] = 0x49;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kDirectPage}] = 0x45;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kDirectPageIndexedX}] = 0x55;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kAbsolute}] = 0x4D;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kAbsoluteIndexedX}] = 0x5D;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kAbsoluteIndexedY}] = 0x59;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kDirectPageIndirect}] = 0x41;
    mnemonic_to_opcode_[{"EOR", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x51;
    mnemonic_to_opcode_[{"INC", AddressingMode::kDirectPage}] = 0xE6;
    mnemonic_to_opcode_[{"INC", AddressingMode::kDirectPageIndexedX}] = 0xF6;
    mnemonic_to_opcode_[{"INC", AddressingMode::kAbsolute}] = 0xEE;
    mnemonic_to_opcode_[{"INC", AddressingMode::kAbsoluteIndexedX}] = 0xFE;
    mnemonic_to_opcode_[{"INX", AddressingMode::kImplied}] = 0xE8;
    mnemonic_to_opcode_[{"INY", AddressingMode::kImplied}] = 0xC8;
    mnemonic_to_opcode_[{"JMP", AddressingMode::kAbsolute}] = 0x4C;
    mnemonic_to_opcode_[{"JMP", AddressingMode::kAbsoluteIndirect}] = 0x6C;
    mnemonic_to_opcode_[{"JSR", AddressingMode::kAbsolute}] = 0x20;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kImmediate}] = 0xA9;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kDirectPage}] = 0xA5;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kDirectPageIndexedX}] = 0xB5;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kAbsolute}] = 0xAD;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kAbsoluteIndexedX}] = 0xBD;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kAbsoluteIndexedY}] = 0xB9;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kDirectPageIndirect}] = 0xA1;
    mnemonic_to_opcode_[{"LDA", AddressingMode::kDirectPageIndirectIndexedY}] =
        0xB1;
    mnemonic_to_opcode_[{"LDX", AddressingMode::kImmediate}] = 0xA2;
    mnemonic_to_opcode_[{"LDX", AddressingMode::kDirectPage}] = 0xA6;
    mnemonic_to_opcode_[{"LDX", AddressingMode::kDirectPageIndexedY}] = 0xB6;
    mnemonic_to_opcode_[{"LDX", AddressingMode::kAbsolute}] = 0xAE;
    mnemonic_to_opcode_[{"LDX", AddressingMode::kAbsoluteIndexedY}] = 0xBE;
    mnemonic_to_opcode_[{"LDY", AddressingMode::kImmediate}] = 0xA0;
    mnemonic_to_opcode_[{"LDY", AddressingMode::kDirectPage}] = 0xA4;
    mnemonic_to_opcode_[{"LDY", AddressingMode::kDirectPageIndexedX}] = 0xB4;
    mnemonic_to_opcode_[{"LDY", AddressingMode::kAbsolute}] = 0xAC;
    mnemonic_to_opcode_[{"LDY", AddressingMode::kAbsoluteIndexedX}] = 0xBC;
    mnemonic_to_opcode_[{"LSR", AddressingMode::kAccumulator}] = 0x4A;
    mnemonic_to_opcode_[{"LSR", AddressingMode::kDirectPage}] = 0x46;
    mnemonic_to_opcode_[{"LSR", AddressingMode::kDirectPageIndexedX}] = 0x56;
    mnemonic_to_opcode_[{"LSR", AddressingMode::kAbsolute}] = 0x4E;
    mnemonic_to_opcode_[{"LSR", AddressingMode::kAbsoluteIndexedX}] = 0x5E;
    mnemonic_to_opcode_[{"NOP", AddressingMode::kImplied}] = 0xEA;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kImmediate}] = 0x09;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kDirectPage}] = 0x05;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kDirectPageIndexedX}] = 0x15;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kAbsolute}] = 0x0D;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kAbsoluteIndexedX}] = 0x1D;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kAbsoluteIndexedY}] = 0x19;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kDirectPageIndirect}] = 0x01;
    mnemonic_to_opcode_[{"ORA", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x11;
    mnemonic_to_opcode_[{"PEA", AddressingMode::kImmediate}] = 0xF4;
    mnemonic_to_opcode_[{"PEI", AddressingMode::kDirectPageIndirect}] = 0xD4;
    mnemonic_to_opcode_[{"PER", AddressingMode::kProgramCounterRelativeLong}] =
        0x62;
    mnemonic_to_opcode_[{"PHA", AddressingMode::kImplied}] = 0x48;
    mnemonic_to_opcode_[{"PHB", AddressingMode::kImplied}] = 0x8B;
    mnemonic_to_opcode_[{"PHD", AddressingMode::kImplied}] = 0x0B;
    mnemonic_to_opcode_[{"PHK", AddressingMode::kImplied}] = 0x4B;
    mnemonic_to_opcode_[{"PHP", AddressingMode::kImplied}] = 0x08;
    mnemonic_to_opcode_[{"PHX", AddressingMode::kImplied}] = 0xDA;
    mnemonic_to_opcode_[{"PHY", AddressingMode::kImplied}] = 0x5A;
    mnemonic_to_opcode_[{"PLA", AddressingMode::kImplied}] = 0x68;
    mnemonic_to_opcode_[{"PLB", AddressingMode::kImplied}] = 0xAB;
    mnemonic_to_opcode_[{"PLD", AddressingMode::kImplied}] = 0x2B;
    mnemonic_to_opcode_[{"PLP", AddressingMode::kImplied}] = 0x28;
    mnemonic_to_opcode_[{"PLX", AddressingMode::kImplied}] = 0xFA;
    mnemonic_to_opcode_[{"PLY", AddressingMode::kImplied}] = 0x7A;
    mnemonic_to_opcode_[{"REP", AddressingMode::kImmediate}] = 0xC2;
    mnemonic_to_opcode_[{"ROL", AddressingMode::kAccumulator}] = 0x2A;
    mnemonic_to_opcode_[{"ROL", AddressingMode::kDirectPage}] = 0x26;
    mnemonic_to_opcode_[{"ROL", AddressingMode::kDirectPageIndexedX}] = 0x36;
    mnemonic_to_opcode_[{"ROL", AddressingMode::kAbsolute}] = 0x2E;
    mnemonic_to_opcode_[{"ROL", AddressingMode::kAbsoluteIndexedX}] = 0x3E;
    mnemonic_to_opcode_[{"ROR", AddressingMode::kAccumulator}] = 0x6A;
    mnemonic_to_opcode_[{"ROR", AddressingMode::kDirectPage}] = 0x66;
    mnemonic_to_opcode_[{"ROR", AddressingMode::kDirectPageIndexedX}] = 0x76;
    mnemonic_to_opcode_[{"ROR", AddressingMode::kAbsolute}] = 0x6E;
    mnemonic_to_opcode_[{"ROR", AddressingMode::kAbsoluteIndexedX}] = 0x7E;
    mnemonic_to_opcode_[{"RTI", AddressingMode::kImplied}] = 0x40;
    mnemonic_to_opcode_[{"RTL", AddressingMode::kImplied}] = 0x6B;
    mnemonic_to_opcode_[{"RTS", AddressingMode::kImplied}] = 0x60;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kImmediate}] = 0xE9;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kDirectPage}] = 0xE5;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kDirectPageIndexedX}] = 0xF5;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kAbsolute}] = 0xED;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kAbsoluteIndexedX}] = 0xFD;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kAbsoluteIndexedY}] = 0xF9;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kDirectPageIndirect}] = 0xE1;
    mnemonic_to_opcode_[{"SBC", AddressingMode::kDirectPageIndirectIndexedY}] =
        0xF1;
    mnemonic_to_opcode_[{"SEC", AddressingMode::kImplied}] = 0x38;
    mnemonic_to_opcode_[{"SED", AddressingMode::kImplied}] = 0xF8;
    mnemonic_to_opcode_[{"SEI", AddressingMode::kImplied}] = 0x78;
    mnemonic_to_opcode_[{"SEP", AddressingMode::kImmediate}] = 0xE2;
    mnemonic_to_opcode_[{"STA", AddressingMode::kDirectPage}] = 0x85;
    mnemonic_to_opcode_[{"STA", AddressingMode::kDirectPageIndexedX}] = 0x95;
    mnemonic_to_opcode_[{"STA", AddressingMode::kAbsolute}] = 0x8D;
    mnemonic_to_opcode_[{"STA", AddressingMode::kAbsoluteIndexedX}] = 0x9D;
    mnemonic_to_opcode_[{"STA", AddressingMode::kAbsoluteIndexedY}] = 0x99;
    mnemonic_to_opcode_[{"STA", AddressingMode::kDirectPageIndirect}] = 0x81;
    mnemonic_to_opcode_[{"STA", AddressingMode::kDirectPageIndirectIndexedY}] =
        0x91;
    mnemonic_to_opcode_[{"STP", AddressingMode::kImplied}] = 0xDB;
    mnemonic_to_opcode_[{"STX", AddressingMode::kDirectPage}] = 0x86;
    mnemonic_to_opcode_[{"STX", AddressingMode::kDirectPageIndexedY}] = 0x96;
    mnemonic_to_opcode_[{"STX", AddressingMode::kAbsolute}] = 0x8E;
    mnemonic_to_opcode_[{"STY", AddressingMode::kDirectPage}] = 0x84;
    mnemonic_to_opcode_[{"STY", AddressingMode::kDirectPageIndexedX}] = 0x94;
    mnemonic_to_opcode_[{"STY", AddressingMode::kAbsolute}] = 0x8C;
    mnemonic_to_opcode_[{"STZ", AddressingMode::kDirectPage}] = 0x64;
    mnemonic_to_opcode_[{"STZ", AddressingMode::kDirectPageIndexedX}] = 0x74;
    mnemonic_to_opcode_[{"STZ", AddressingMode::kAbsolute}] = 0x9C;
    mnemonic_to_opcode_[{"STZ", AddressingMode::kAbsoluteIndexedX}] = 0x9E;
    mnemonic_to_opcode_[{"TAX", AddressingMode::kImplied}] = 0xAA;
    mnemonic_to_opcode_[{"TAY", AddressingMode::kImplied}] = 0xA8;
    mnemonic_to_opcode_[{"TCD", AddressingMode::kImplied}] = 0x5B;
    mnemonic_to_opcode_[{"TCS", AddressingMode::kImplied}] = 0x1B;
    mnemonic_to_opcode_[{"TDC", AddressingMode::kImplied}] = 0x7B;
    mnemonic_to_opcode_[{"TSC", AddressingMode::kImplied}] = 0x3B;
    mnemonic_to_opcode_[{"TSX", AddressingMode::kImplied}] = 0xBA;
    mnemonic_to_opcode_[{"TXA", AddressingMode::kImplied}] = 0x8A;
    mnemonic_to_opcode_[{"TXS", AddressingMode::kImplied}] = 0x9A;
    mnemonic_to_opcode_[{"TXY", AddressingMode::kImplied}] = 0x9B;
    mnemonic_to_opcode_[{"TYA", AddressingMode::kImplied}] = 0x98;
    mnemonic_to_opcode_[{"TYX", AddressingMode::kImplied}] = 0xBB;
    mnemonic_to_opcode_[{"WAI", AddressingMode::kImplied}] = 0xCB;
    mnemonic_to_opcode_[{"XBA", AddressingMode::kImplied}] = 0xEB;
    mnemonic_to_opcode_[{"XCE", AddressingMode::kImplied}] = 0xFB;
  }

  std::unordered_map<MnemonicMode, uint8_t, MnemonicModeHash>
      mnemonic_to_opcode_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze
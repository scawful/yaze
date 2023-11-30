#pragma once

#include <cstdint>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/emu/internal/opcodes.h"

namespace yaze {
namespace app {
namespace emu {

class AsmParser {
 public:
  std::vector<uint8_t> Parse(const std::string& instruction) {
    std::smatch match;
    if (!std::regex_match(instruction, match, instruction_regex_)) {
      throw std::runtime_error("Invalid instruction format: " + instruction);
    }

    std::string mnemonic = match[1];
    std::string addressing_mode = match[2];
    std::string operand = match[3];

    std::string lookup_string = mnemonic.substr(0, 3);

    auto opcode_entry = mnemonic_to_opcode_.find(mnemonic);
    if (opcode_entry == mnemonic_to_opcode_.end()) {
      throw std::runtime_error(
          "Unknown mnemonic or addressing mode: " + mnemonic + addressing_mode);
    }

    std::vector<uint8_t> bytes = {opcode_entry->second};
    // AppendOperandBytes(bytes, operand, addressing_mode);

    return bytes;
  }

  void CreateInternalOpcodeMap() {
    for (const auto& opcode_entry : opcode_to_mnemonic) {
      std::string name = opcode_entry.second;
      uint8_t opcode = opcode_entry.first;
      mnemonic_to_opcode_[name] = opcode;
    }
  }

 private:
  void AppendOperandBytes(std::vector<uint8_t>& bytes,
                          const std::string& operand,
                          const std::string& addressing_mode) {
    if (addressing_mode == ".b") {
      bytes.push_back(static_cast<uint8_t>(std::stoi(operand, nullptr, 16)));
    } else if (addressing_mode == ".w") {
      uint16_t word_operand =
          static_cast<uint16_t>(std::stoi(operand, nullptr, 16));
      bytes.push_back(static_cast<uint8_t>(word_operand & 0xFF));
      bytes.push_back(static_cast<uint8_t>((word_operand >> 8) & 0xFF));
    } else if (addressing_mode == ".l") {
      uint32_t long_operand =
          static_cast<uint32_t>(std::stoul(operand, nullptr, 16));
      bytes.push_back(static_cast<uint8_t>(long_operand & 0xFF));
      bytes.push_back(static_cast<uint8_t>((long_operand >> 8) & 0xFF));
      bytes.push_back(static_cast<uint8_t>((long_operand >> 16) & 0xFF));
    }
  }

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

  AddressingMode InferAddressingModeFromOperand(const std::string& operand) {
    if (operand[0] == '$') {
      return AddressingMode::kAbsolute;
    } else if (operand[0] == '#') {
      return AddressingMode::kImmediate;
    } else {
      return AddressingMode::kImplied;
    }
  }

  const std::regex instruction_regex_{R"((\w+)\s*(\.\w)?\s*(\$\w+|\#\w+|\w+))"};
  std::unordered_map<std::string, uint8_t> mnemonic_to_opcode_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze
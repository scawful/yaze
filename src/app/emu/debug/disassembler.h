#ifndef YAZE_APP_EMU_DEBUG_DISASSEMBLER_H_
#define YAZE_APP_EMU_DEBUG_DISASSEMBLER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"

namespace yaze {
namespace emu {
namespace debug {

/**
 * @brief Addressing modes for the 65816 CPU
 */
enum class AddressingMode65816 {
  kImplied,                           // No operand
  kAccumulator,                       // A
  kImmediate8,                        // #$xx (8-bit)
  kImmediate16,                       // #$xxxx (16-bit, depends on M/X flags)
  kImmediateM,                        // #$xx or #$xxxx (depends on M flag)
  kImmediateX,                        // #$xx or #$xxxx (depends on X flag)
  kDirectPage,                        // $xx
  kDirectPageIndexedX,                // $xx,X
  kDirectPageIndexedY,                // $xx,Y
  kDirectPageIndirect,                // ($xx)
  kDirectPageIndirectLong,            // [$xx]
  kDirectPageIndexedIndirectX,        // ($xx,X)
  kDirectPageIndirectIndexedY,        // ($xx),Y
  kDirectPageIndirectLongIndexedY,    // [$xx],Y
  kAbsolute,                          // $xxxx
  kAbsoluteIndexedX,                  // $xxxx,X
  kAbsoluteIndexedY,                  // $xxxx,Y
  kAbsoluteLong,                      // $xxxxxx
  kAbsoluteLongIndexedX,              // $xxxxxx,X
  kAbsoluteIndirect,                  // ($xxxx)
  kAbsoluteIndirectLong,              // [$xxxx]
  kAbsoluteIndexedIndirect,           // ($xxxx,X)
  kProgramCounterRelative,            // 8-bit relative branch
  kProgramCounterRelativeLong,        // 16-bit relative branch
  kStackRelative,                     // $xx,S
  kStackRelativeIndirectIndexedY,     // ($xx,S),Y
  kBlockMove,                         // src,dst (MVN/MVP)
};

/**
 * @brief Information about a single 65816 instruction
 */
struct InstructionInfo {
  std::string mnemonic;         // e.g., "LDA", "STA", "JSR"
  AddressingMode65816 mode;     // Addressing mode
  uint8_t base_size;            // Base size in bytes (1 for opcode alone)

  InstructionInfo() : mnemonic("???"), mode(AddressingMode65816::kImplied), base_size(1) {}
  InstructionInfo(const std::string& m, AddressingMode65816 am, uint8_t size)
      : mnemonic(m), mode(am), base_size(size) {}
};

/**
 * @brief Result of disassembling a single instruction
 */
struct DisassembledInstruction {
  uint32_t address;               // Full 24-bit address
  uint8_t opcode;                 // The opcode byte
  std::vector<uint8_t> operands;  // Operand bytes
  std::string mnemonic;           // Instruction mnemonic
  std::string operand_str;        // Formatted operand (e.g., "#$FF", "$1234,X")
  std::string full_text;          // Complete disassembly line
  uint8_t size;                   // Total instruction size
  bool is_branch;                 // Is this a branch instruction?
  bool is_call;                   // Is this JSR/JSL?
  bool is_return;                 // Is this RTS/RTL/RTI?
  uint32_t branch_target;         // Target address for branches/jumps

  DisassembledInstruction()
      : address(0), opcode(0), size(1), is_branch(false),
        is_call(false), is_return(false), branch_target(0) {}
};

/**
 * @brief 65816 CPU disassembler for debugging and ROM hacking
 *
 * This disassembler converts raw ROM/memory bytes into human-readable
 * assembly instructions. It handles:
 * - All 256 opcodes
 * - All addressing modes including 65816-specific ones
 * - Variable-size immediate operands based on M/X flags
 * - Branch target calculation
 * - Symbol resolution (optional)
 *
 * Usage:
 *   Disassembler65816 dis;
 *   auto result = dis.Disassemble(address, [](uint32_t addr) {
 *     return memory.ReadByte(addr);
 *   });
 *   std::cout << result.full_text << std::endl;
 */
class Disassembler65816 {
 public:
  using MemoryReader = std::function<uint8_t(uint32_t)>;
  using SymbolResolver = std::function<std::string(uint32_t)>;

  Disassembler65816();

  /**
   * @brief Disassemble a single instruction
   * @param address Starting address (24-bit)
   * @param read_byte Function to read bytes from memory
   * @param m_flag Accumulator/memory size flag (true = 8-bit)
   * @param x_flag Index register size flag (true = 8-bit)
   * @return Disassembled instruction
   */
  DisassembledInstruction Disassemble(uint32_t address,
                                       MemoryReader read_byte,
                                       bool m_flag = true,
                                       bool x_flag = true) const;

  /**
   * @brief Disassemble multiple instructions
   * @param start_address Starting address
   * @param count Number of instructions to disassemble
   * @param read_byte Function to read bytes from memory
   * @param m_flag Accumulator/memory size flag
   * @param x_flag Index register size flag
   * @return Vector of disassembled instructions
   */
  std::vector<DisassembledInstruction> DisassembleRange(
      uint32_t start_address,
      size_t count,
      MemoryReader read_byte,
      bool m_flag = true,
      bool x_flag = true) const;

  /**
   * @brief Set optional symbol resolver for address lookups
   */
  void SetSymbolResolver(SymbolResolver resolver) {
    symbol_resolver_ = resolver;
  }

  /**
   * @brief Get instruction info for an opcode
   */
  const InstructionInfo& GetInstructionInfo(uint8_t opcode) const;

  /**
   * @brief Calculate actual instruction size based on flags
   */
  uint8_t GetInstructionSize(uint8_t opcode, bool m_flag, bool x_flag) const;

 private:
  // Initialize opcode tables
  void InitializeOpcodeTable();

  // Format operand based on addressing mode
  std::string FormatOperand(AddressingMode65816 mode,
                            const std::vector<uint8_t>& operands,
                            uint32_t address,
                            bool m_flag,
                            bool x_flag) const;

  // Calculate branch target
  uint32_t CalculateBranchTarget(uint32_t address,
                                  const std::vector<uint8_t>& operands,
                                  AddressingMode65816 mode,
                                  uint8_t instruction_size) const;

  // Opcode to instruction info mapping
  InstructionInfo opcode_table_[256];

  // Optional symbol resolver
  SymbolResolver symbol_resolver_;
};

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_DISASSEMBLER_H_

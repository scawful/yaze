#ifndef YAZE_APP_EMU_DEBUG_DISASSEMBLY_VIEWER_H_
#define YAZE_APP_EMU_DEBUG_DISASSEMBLY_VIEWER_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"
#include "app/gfx/bitmap.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace emu {
namespace debug {

/**
 * @brief Represents a single disassembled instruction with metadata
 */
struct DisassemblyEntry {
  uint32_t address;           // Full 24-bit address (bank:offset)
  uint8_t opcode;             // The opcode byte
  std::vector<uint8_t> operands;  // Operand bytes (0-2 bytes)
  std::string mnemonic;       // Instruction mnemonic (e.g., "LDA", "STA")
  std::string operand_str;    // Formatted operand string (e.g., "#$00", "($10),Y")
  uint8_t size;               // Total instruction size in bytes
  uint64_t execution_count;   // How many times this instruction was executed
  bool is_breakpoint;         // Whether a breakpoint is set at this address
  bool is_current_pc;         // Whether this is the current PC location
  
  DisassemblyEntry() 
      : address(0), opcode(0), size(1), execution_count(0), 
        is_breakpoint(false), is_current_pc(false) {}
};

/**
 * @brief Advanced disassembly viewer with sparse storage and interactive features
 * 
 * This viewer provides a professional disassembly interface similar to modern
 * debuggers and ROM hacking tools. Features include:
 * - Sparse address-based storage (only stores executed instructions)
 * - Optimized ImGui table rendering with virtual scrolling
 * - Clickable addresses, opcodes, and operands
 * - Context menus for setting breakpoints, jumping to addresses, etc.
 * - Highlighting of current PC, breakpoints, and hot paths
 * - Search and filter capabilities
 * - Export to assembly file
 */
class DisassemblyViewer {
 public:
  DisassemblyViewer() = default;
  ~DisassemblyViewer() = default;

  /**
   * @brief Record an instruction execution
   * @param address Full 24-bit address
   * @param opcode The opcode byte
   * @param operands Vector of operand bytes
   * @param mnemonic Instruction mnemonic
   * @param operand_str Formatted operand string
   */
  void RecordInstruction(uint32_t address, uint8_t opcode,
                        const std::vector<uint8_t>& operands,
                        const std::string& mnemonic,
                        const std::string& operand_str);

  /**
   * @brief Render the disassembly viewer UI
   * @param current_pc Current program counter (24-bit)
   * @param breakpoints List of breakpoint addresses
   */
  void Render(uint32_t current_pc, const std::vector<uint32_t>& breakpoints);

  /**
   * @brief Clear all recorded instructions
   */
  void Clear();

  /**
   * @brief Get the number of unique instructions recorded
   */
  size_t GetInstructionCount() const { return instructions_.size(); }

  /**
   * @brief Export disassembly to file
   * @param filepath Path to output file
   * @return true if successful
   */
  bool ExportToFile(const std::string& filepath) const;

  /**
   * @brief Jump to a specific address in the viewer
   * @param address Address to jump to
   */
  void JumpToAddress(uint32_t address);

  /**
   * @brief Set whether to auto-scroll to current PC
   */
  void SetAutoScroll(bool enabled) { auto_scroll_ = enabled; }

  /**
   * @brief Get sorted list of addresses for rendering
   */
  std::vector<uint32_t> GetSortedAddresses() const;

  /**
   * @brief Check if the disassembly viewer is available
   */
  bool IsAvailable() const { return !instructions_.empty(); }

 private:
  // Sparse storage: only store executed instructions
  std::map<uint32_t, DisassemblyEntry> instructions_;
  
  // UI state
  char search_filter_[256] = "";
  uint32_t selected_address_ = 0;
  uint32_t scroll_to_address_ = 0;
  bool auto_scroll_ = true;
  bool show_execution_counts_ = true;
  bool show_hex_dump_ = true;
  
  // Rendering helpers
  void RenderToolbar();
  void RenderDisassemblyTable(uint32_t current_pc, 
                              const std::vector<uint32_t>& breakpoints);
  void RenderContextMenu(uint32_t address);
  void RenderSearchBar();
  
  // Formatting helpers
  ImVec4 GetAddressColor(const DisassemblyEntry& entry, uint32_t current_pc) const;
  ImVec4 GetMnemonicColor(const DisassemblyEntry& entry) const;
  std::string FormatHexDump(const DisassemblyEntry& entry) const;
  
  // Filter helper
  bool PassesFilter(const DisassemblyEntry& entry) const;
};

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_DEBUG_DISASSEMBLY_VIEWER_H_


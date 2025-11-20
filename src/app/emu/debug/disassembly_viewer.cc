#include "app/emu/debug/disassembly_viewer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"

namespace yaze {
namespace emu {
namespace debug {

namespace {

// Color scheme for retro hacker aesthetic
constexpr ImVec4 kColorAddress(0.4f, 0.8f, 1.0f, 1.0f);  // Cyan for addresses
constexpr ImVec4 kColorOpcode(0.8f, 0.8f, 0.8f,
                              1.0f);  // Light gray for opcodes
constexpr ImVec4 kColorMnemonic(1.0f, 0.8f, 0.2f, 1.0f);  // Gold for mnemonics
constexpr ImVec4 kColorOperand(0.6f, 1.0f, 0.6f,
                               1.0f);  // Light green for operands
constexpr ImVec4 kColorComment(0.5f, 0.5f, 0.5f, 1.0f);    // Gray for comments
constexpr ImVec4 kColorCurrentPC(1.0f, 0.3f, 0.3f, 1.0f);  // Red for current PC
constexpr ImVec4 kColorBreakpoint(1.0f, 0.0f, 0.0f,
                                  1.0f);  // Bright red for breakpoints
constexpr ImVec4 kColorHotPath(1.0f, 0.6f, 0.0f, 1.0f);  // Orange for hot paths

}  // namespace

void DisassemblyViewer::RecordInstruction(uint32_t address, uint8_t opcode,
                                          const std::vector<uint8_t>& operands,
                                          const std::string& mnemonic,
                                          const std::string& operand_str) {
  // Skip if recording disabled (for performance)
  if (!recording_enabled_) {
    return;
  }

  auto it = instructions_.find(address);
  if (it != instructions_.end()) {
    // Instruction already recorded, just increment execution count
    it->second.execution_count++;
  } else {
    // Check if we're at the limit
    if (instructions_.size() >= max_instructions_) {
      // Trim to 80% of max to avoid constant trimming
      TrimToSize(max_instructions_ * 0.8);
    }

    // New instruction, add to map
    DisassemblyEntry entry;
    entry.address = address;
    entry.opcode = opcode;
    entry.operands = operands;
    entry.mnemonic = mnemonic;
    entry.operand_str = operand_str;
    entry.size = 1 + operands.size();
    entry.execution_count = 1;
    entry.is_breakpoint = false;
    entry.is_current_pc = false;

    instructions_[address] = entry;
  }
}

void DisassemblyViewer::TrimToSize(size_t target_size) {
  if (instructions_.size() <= target_size) {
    return;
  }

  // Keep most-executed instructions
  // Remove least-executed ones
  std::vector<std::pair<uint32_t, uint64_t>> addr_counts;
  for (const auto& [addr, entry] : instructions_) {
    addr_counts.push_back({addr, entry.execution_count});
  }

  // Sort by execution count (ascending)
  std::sort(addr_counts.begin(), addr_counts.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  // Remove least-executed instructions
  size_t to_remove = instructions_.size() - target_size;
  for (size_t i = 0; i < to_remove && i < addr_counts.size(); i++) {
    instructions_.erase(addr_counts[i].first);
  }
}

void DisassemblyViewer::Render(uint32_t current_pc,
                               const std::vector<uint32_t>& breakpoints) {
  // Update current PC and breakpoint flags
  for (auto& [addr, entry] : instructions_) {
    entry.is_current_pc = (addr == current_pc);
    entry.is_breakpoint = std::find(breakpoints.begin(), breakpoints.end(),
                                    addr) != breakpoints.end();
  }

  RenderToolbar();
  RenderSearchBar();
  RenderDisassemblyTable(current_pc, breakpoints);
}

void DisassemblyViewer::RenderToolbar() {
  if (ImGui::BeginTable("##DisasmToolbar", 6, ImGuiTableFlags_None)) {
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_CLEAR_ALL " Clear")) {
      Clear();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Clear all recorded instructions");
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_SAVE " Export")) {
      // TODO: Open file dialog and export
      ExportToFile("disassembly.asm");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Export disassembly to file");
    }

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("Auto-scroll", &auto_scroll_)) {
      // Toggle auto-scroll
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Auto-scroll to current PC");
    }

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("Exec Count", &show_execution_counts_)) {
      // Toggle execution count display
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show execution counts");
    }

    ImGui::TableNextColumn();
    if (ImGui::Checkbox("Hex Dump", &show_hex_dump_)) {
      // Toggle hex dump display
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show hex dump of instruction bytes");
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MEMORY " %zu instructions", instructions_.size());

    ImGui::EndTable();
  }

  ImGui::Separator();
}

void DisassemblyViewer::RenderSearchBar() {
  ImGui::PushItemWidth(-1.0f);
  if (ImGui::InputTextWithHint("##DisasmSearch",
                               ICON_MD_SEARCH
                               " Search (address, mnemonic, operand)...",
                               search_filter_, IM_ARRAYSIZE(search_filter_))) {
    // Search filter updated
  }
  ImGui::PopItemWidth();
}

void DisassemblyViewer::RenderDisassemblyTable(
    uint32_t current_pc, const std::vector<uint32_t>& breakpoints) {
  // Table flags for professional disassembly view
  ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_Sortable |
                          ImGuiTableFlags_Reorderable |
                          ImGuiTableFlags_Hideable;

  // Calculate column count based on optional columns
  int column_count = 4;  // BP, Address, Mnemonic, Operand (always shown)
  if (show_hex_dump_) column_count++;
  if (show_execution_counts_) column_count++;

  if (!ImGui::BeginTable("##DisasmTable", column_count, flags,
                         ImVec2(0.0f, 0.0f))) {
    return;
  }

  // Setup columns
  ImGui::TableSetupColumn(ICON_MD_CIRCLE, ImGuiTableColumnFlags_WidthFixed,
                          25.0f);  // Breakpoint indicator
  ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
  if (show_hex_dump_) {
    ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed, 100.0f);
  }
  ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed, 80.0f);
  ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthStretch);
  if (show_execution_counts_) {
    ImGui::TableSetupColumn(ICON_MD_TRENDING_UP " Count",
                            ImGuiTableColumnFlags_WidthFixed, 80.0f);
  }

  ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header row
  ImGui::TableHeadersRow();

  // Render instructions
  ImGuiListClipper clipper;
  auto sorted_addrs = GetSortedAddresses();
  clipper.Begin(sorted_addrs.size());

  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
      uint32_t addr = sorted_addrs[row];
      const auto& entry = instructions_[addr];

      // Skip if doesn't pass filter
      if (!PassesFilter(entry)) {
        continue;
      }

      ImGui::TableNextRow();

      // Highlight current PC row
      if (entry.is_current_pc) {
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0,
            ImGui::GetColorU32(ImVec4(0.3f, 0.0f, 0.0f, 0.5f)));
      }

      // Column 0: Breakpoint indicator
      ImGui::TableNextColumn();
      if (entry.is_breakpoint) {
        ImGui::TextColored(kColorBreakpoint, ICON_MD_STOP);
      } else {
        ImGui::TextDisabled(" ");
      }

      // Column 1: Address (clickable)
      ImGui::TableNextColumn();
      ImVec4 addr_color = GetAddressColor(entry, current_pc);

      std::string addr_str =
          absl::StrFormat("$%02X:%04X", (addr >> 16) & 0xFF, addr & 0xFFFF);
      if (ImGui::Selectable(addr_str.c_str(), selected_address_ == addr,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected_address_ = addr;
      }

      // Context menu on right-click
      if (ImGui::BeginPopupContextItem()) {
        RenderContextMenu(addr);
        ImGui::EndPopup();
      }

      // Column 2: Hex dump (optional)
      if (show_hex_dump_) {
        ImGui::TableNextColumn();
        ImGui::TextColored(kColorOpcode, "%s", FormatHexDump(entry).c_str());
      }

      // Column 3: Mnemonic (clickable for documentation)
      ImGui::TableNextColumn();
      ImVec4 mnemonic_color = GetMnemonicColor(entry);
      if (ImGui::Selectable(entry.mnemonic.c_str(), false)) {
        // TODO: Open documentation for this mnemonic
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click for instruction documentation");
      }

      // Column 4: Operand (clickable for jump-to-address)
      ImGui::TableNextColumn();
      ImGui::TextColored(kColorOperand, "%s", entry.operand_str.c_str());

      // Column 5: Execution count (optional)
      if (show_execution_counts_) {
        ImGui::TableNextColumn();

        // Color-code by execution frequency (hot path highlighting)
        ImVec4 count_color = kColorComment;
        if (entry.execution_count > 10000) {
          count_color = kColorHotPath;
        } else if (entry.execution_count > 1000) {
          count_color = ImVec4(0.8f, 0.8f, 0.3f, 1.0f);  // Yellow
        }

        ImGui::TextColored(count_color, "%llu", entry.execution_count);
      }
    }
  }

  // Auto-scroll to current PC
  if (auto_scroll_ && scroll_to_address_ != current_pc) {
    // Find row index of current PC
    auto it = std::find(sorted_addrs.begin(), sorted_addrs.end(), current_pc);
    if (it != sorted_addrs.end()) {
      int row_index = std::distance(sorted_addrs.begin(), it);
      ImGui::SetScrollY((row_index * ImGui::GetTextLineHeightWithSpacing()) -
                        (ImGui::GetWindowHeight() * 0.5f));
      scroll_to_address_ = current_pc;
    }
  }

  ImGui::EndTable();
}

void DisassemblyViewer::RenderContextMenu(uint32_t address) {
  auto& entry = instructions_[address];

  if (ImGui::MenuItem(ICON_MD_FLAG " Toggle Breakpoint")) {
    // TODO: Implement breakpoint toggle callback
  }

  if (ImGui::MenuItem(ICON_MD_MY_LOCATION " Jump to Address")) {
    JumpToAddress(address);
  }

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy Address")) {
    ImGui::SetClipboardText(absl::StrFormat("$%06X", address).c_str());
  }

  if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy Instruction")) {
    std::string instr = absl::StrFormat("%s %s", entry.mnemonic.c_str(),
                                        entry.operand_str.c_str());
    ImGui::SetClipboardText(instr.c_str());
  }

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_MD_INFO " Show Info")) {
    // TODO: Show detailed instruction info
  }
}

ImVec4 DisassemblyViewer::GetAddressColor(const DisassemblyEntry& entry,
                                          uint32_t current_pc) const {
  if (entry.is_current_pc) {
    return kColorCurrentPC;
  }
  if (entry.is_breakpoint) {
    return kColorBreakpoint;
  }
  return kColorAddress;
}

ImVec4 DisassemblyViewer::GetMnemonicColor(
    const DisassemblyEntry& entry) const {
  // Color-code by instruction type
  const std::string& mnemonic = entry.mnemonic;

  // Branches and jumps
  if (mnemonic.find('B') == 0 || mnemonic == "JMP" || mnemonic == "JSR" ||
      mnemonic == "RTL" || mnemonic == "RTS" || mnemonic == "RTI") {
    return ImVec4(0.8f, 0.4f, 1.0f, 1.0f);  // Purple for control flow
  }

  // Loads
  if (mnemonic.find("LD") == 0) {
    return ImVec4(0.4f, 1.0f, 0.4f, 1.0f);  // Green for loads
  }

  // Stores
  if (mnemonic.find("ST") == 0) {
    return ImVec4(1.0f, 0.6f, 0.4f, 1.0f);  // Orange for stores
  }

  return kColorMnemonic;
}

std::string DisassemblyViewer::FormatHexDump(
    const DisassemblyEntry& entry) const {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0');

  // Opcode
  ss << std::setw(2) << static_cast<int>(entry.opcode);

  // Operands
  for (const auto& operand_byte : entry.operands) {
    ss << " " << std::setw(2) << static_cast<int>(operand_byte);
  }

  // Pad to consistent width (3 bytes max)
  for (size_t i = entry.operands.size(); i < 2; i++) {
    ss << "   ";
  }

  return ss.str();
}

bool DisassemblyViewer::PassesFilter(const DisassemblyEntry& entry) const {
  if (search_filter_[0] == '\0') {
    return true;  // No filter active
  }

  std::string filter_lower(search_filter_);
  std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                 ::tolower);

  // Check address
  std::string addr_str = absl::StrFormat("%06x", entry.address);
  if (addr_str.find(filter_lower) != std::string::npos) {
    return true;
  }

  // Check mnemonic
  std::string mnemonic_lower = entry.mnemonic;
  std::transform(mnemonic_lower.begin(), mnemonic_lower.end(),
                 mnemonic_lower.begin(), ::tolower);
  if (mnemonic_lower.find(filter_lower) != std::string::npos) {
    return true;
  }

  // Check operand
  std::string operand_lower = entry.operand_str;
  std::transform(operand_lower.begin(), operand_lower.end(),
                 operand_lower.begin(), ::tolower);
  if (operand_lower.find(filter_lower) != std::string::npos) {
    return true;
  }

  return false;
}

void DisassemblyViewer::Clear() {
  instructions_.clear();
  selected_address_ = 0;
  scroll_to_address_ = 0;
}

bool DisassemblyViewer::ExportToFile(const std::string& filepath) const {
  std::ofstream out(filepath);
  if (!out.is_open()) {
    return false;
  }

  out << "; YAZE Disassembly Export\n";
  out << "; Total instructions: " << instructions_.size() << "\n";
  out << "; Generated: " << __DATE__ << " " << __TIME__ << "\n\n";

  auto sorted_addrs = GetSortedAddresses();
  for (uint32_t addr : sorted_addrs) {
    const auto& entry = instructions_.at(addr);

    out << absl::StrFormat("$%02X:%04X:  %-8s %-6s %-20s ; exec=%llu\n",
                           (addr >> 16) & 0xFF, addr & 0xFFFF,
                           FormatHexDump(entry).c_str(), entry.mnemonic.c_str(),
                           entry.operand_str.c_str(), entry.execution_count);
  }

  out.close();
  return true;
}

void DisassemblyViewer::JumpToAddress(uint32_t address) {
  selected_address_ = address;
  scroll_to_address_ = 0;  // Force scroll update
  auto_scroll_ = false;    // Disable auto-scroll temporarily
}

std::vector<uint32_t> DisassemblyViewer::GetSortedAddresses() const {
  std::vector<uint32_t> addrs;
  addrs.reserve(instructions_.size());

  for (const auto& [addr, _] : instructions_) {
    addrs.push_back(addr);
  }

  std::sort(addrs.begin(), addrs.end());
  return addrs;
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze

#ifndef YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H
#define YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/gui/core/input.h"
#include "app/gui/imgui_memory_editor.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using ImGui::SameLine;
using ImGui::Text;

struct MemoryEditor {
  explicit MemoryEditor(Rom* rom = nullptr) : rom_(rom) {}

  void Update(bool& show_memory_editor);

  // Set the ROM pointer
  void SetRom(Rom* rom) { rom_ = rom; }

  // Get the ROM pointer
  Rom* rom() const { return rom_; }

 private:
  void DrawToolbar();
  void DrawJumpToAddressPopup();
  void DrawSearchPopup();
  void DrawBookmarksPopup();

  Rom* rom_;
  gui::MemoryEditorWidget memory_widget_;
  gui::MemoryEditorWidget comparison_widget_;
  bool show_compare_rom_ = false;
  Rom comparison_rom_;

  // Toolbar state
  char jump_address_[16] = "0x000000";
  char search_pattern_[256] = "";
  uint32_t current_address_ = 0;

  struct Bookmark {
    uint32_t address;
    std::string name;
    std::string description;
  };
  std::vector<Bookmark> bookmarks_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H

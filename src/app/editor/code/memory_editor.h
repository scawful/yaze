#ifndef YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H
#define YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H

#include "util/file_util.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/snes.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using ImGui::SameLine;
using ImGui::Text;

struct MemoryEditorWithDiffChecker {
  explicit MemoryEditorWithDiffChecker(Rom* rom = nullptr) : rom_(rom) {}
  
  void Update(bool &show_memory_editor) {
    DrawToolbar();
    ImGui::Separator();
    static MemoryEditor mem_edit;
    static MemoryEditor comp_edit;
    static bool show_compare_rom = false;
    static Rom comparison_rom;
    ImGui::Begin("Hex Editor", &show_memory_editor);
    if (ImGui::Button("Compare Rom")) {
      auto file_name = util::FileDialogWrapper::ShowOpenFileDialog();
      PRINT_IF_ERROR(comparison_rom.LoadFromFile(file_name));
      show_compare_rom = true;
    }

    static uint64_t convert_address = 0;
    gui::InputHex("SNES to PC", (int *)&convert_address, 6, 200.f);
    SameLine();
    Text("%x", SnesToPc(convert_address));

    // mem_edit.DrawWindow("Memory Editor", (void*)&(*rom()), rom()->size());
    BEGIN_TABLE("Memory Comparison", 2, ImGuiTableFlags_Resizable);
    SETUP_COLUMN("Source")
    SETUP_COLUMN("Dest")

    NEXT_COLUMN()
    Text("%s", rom()->filename().data());
    mem_edit.DrawContents((void *)&(*rom()), rom()->size());

    NEXT_COLUMN()
    if (show_compare_rom) {
      comp_edit.SetComparisonData((void *)&(*rom()));
      ImGui::BeginGroup();
      ImGui::BeginChild("Comparison ROM");
      Text("%s", comparison_rom.filename().data());
      comp_edit.DrawContents((void *)&(comparison_rom), comparison_rom.size());
      ImGui::EndChild();
      ImGui::EndGroup();
    }
    END_TABLE()

    ImGui::End();
  }

  // Set the ROM pointer
  void set_rom(Rom* rom) { rom_ = rom; }
  
  // Get the ROM pointer
  Rom* rom() const { return rom_; }

 private:
  void DrawToolbar();
  void DrawJumpToAddressPopup();
  void DrawSearchPopup();
  void DrawBookmarksPopup();
  
  Rom* rom_;
  
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

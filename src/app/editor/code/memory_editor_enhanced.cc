#include "app/editor/code/memory_editor_enhanced.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void EnhancedMemoryEditor::UpdateEnhanced(bool& show) {
  ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(ICON_MD_DATA_ARRAY " Enhanced Hex Editor", &show)) {
    ImGui::End();
    return;
  }
  
  DrawToolbar();
  ImGui::Separator();
  
  // Call base memory editor
  static MemoryEditor mem_edit;
  mem_edit.DrawContents((void*)&(*rom()), rom()->size());
  
  ImGui::End();
}

void EnhancedMemoryEditor::DrawToolbar() {
  if (ImGui::Button(ICON_MD_LOCATION_SEARCHING " Jump to Address")) {
    ImGui::OpenPopup("JumpToAddress");
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SEARCH " Search Pattern")) {
    ImGui::OpenPopup("SearchPattern");
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_BOOKMARK " Bookmarks")) {
    ImGui::OpenPopup("Bookmarks");
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_COMPARE " Diff View")) {
    // TODO: Show diff
  }
  
  // Jump to address popup
  if (ImGui::BeginPopup("JumpToAddress")) {
    ImGui::Text("Jump to Address");
    ImGui::Separator();
    ImGui::InputText("Address (hex)", jump_address_, IM_ARRAYSIZE(jump_address_));
    if (ImGui::Button("Go")) {
      // TODO: Parse and jump
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  
  // Search popup
  if (ImGui::BeginPopup("SearchPattern")) {
    ImGui::Text("Search Hex Pattern");
    ImGui::Separator();
    ImGui::InputText("Pattern", search_pattern_, IM_ARRAYSIZE(search_pattern_));
    ImGui::TextDisabled("Use ?? for wildcard (e.g. FF 00 ?? 12)");
    if (ImGui::Button("Search")) {
      // TODO: Implement search
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  
  // Bookmarks popup
  if (ImGui::BeginPopup("Bookmarks")) {
    ImGui::Text("Memory Bookmarks");
    ImGui::Separator();
    for (const auto& bm : bookmarks_) {
      if (ImGui::Selectable(bm.name.c_str())) {
        current_address_ = bm.address;
      }
      ImGui::TextDisabled("  0x%06X - %s", bm.address, bm.description.c_str());
    }
    ImGui::EndPopup();
  }
}

void EnhancedMemoryEditor::DrawJumpToAddress() {}
void EnhancedMemoryEditor::DrawSearch() {}
void EnhancedMemoryEditor::DrawBookmarks() {}

}  // namespace editor
}  // namespace yaze

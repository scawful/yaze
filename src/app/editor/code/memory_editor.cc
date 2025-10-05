#include "app/editor/code/memory_editor.h"
#include "app/gui/icons.h"

namespace yaze {
namespace editor {

void MemoryEditorWithDiffChecker::DrawToolbar() {
  if (ImGui::Button(ICON_MD_LOCATION_SEARCHING " Jump")) {
    ImGui::OpenPopup("JumpToAddress");
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SEARCH " Search")) {
    ImGui::OpenPopup("SearchPattern");
  }
  
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_BOOKMARK " Bookmarks")) {
    ImGui::OpenPopup("Bookmarks");
  }
  
  DrawJumpToAddressPopup();
  DrawSearchPopup();
  DrawBookmarksPopup();
}

void MemoryEditorWithDiffChecker::DrawJumpToAddressPopup() {
  if (ImGui::BeginPopup("JumpToAddress")) {
    ImGui::Text(ICON_MD_LOCATION_SEARCHING " Jump to Address");
    ImGui::Separator();
    ImGui::InputText("Address (hex)", jump_address_, IM_ARRAYSIZE(jump_address_));
    ImGui::TextDisabled("Format: 0x1C800 or 1C800");
    if (ImGui::Button("Go", ImVec2(120, 0))) {
      // TODO: Parse address and scroll to it
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MemoryEditorWithDiffChecker::DrawSearchPopup() {
  if (ImGui::BeginPopup("SearchPattern")) {
    ImGui::Text(ICON_MD_SEARCH " Search Hex Pattern");
    ImGui::Separator();
    ImGui::InputText("Pattern", search_pattern_, IM_ARRAYSIZE(search_pattern_));
    ImGui::TextDisabled("Use ?? for wildcard (e.g. FF 00 ?? 12)");
    if (ImGui::Button("Search", ImVec2(120, 0))) {
      // TODO: Implement search using hex-search handler
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MemoryEditorWithDiffChecker::DrawBookmarksPopup() {
  if (ImGui::BeginPopup("Bookmarks")) {
    ImGui::Text(ICON_MD_BOOKMARK " Memory Bookmarks");
    ImGui::Separator();
    
    if (bookmarks_.empty()) {
      ImGui::TextDisabled("No bookmarks yet");
      ImGui::Separator();
      if (ImGui::Button("Add Current Address")) {
        // TODO: Add bookmark at current address
      }
    } else {
      for (size_t i = 0; i < bookmarks_.size(); ++i) {
        const auto& bm = bookmarks_[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(bm.name.c_str())) {
          current_address_ = bm.address;
          // TODO: Jump to this address
        }
        ImGui::TextDisabled("  0x%06X - %s", bm.address, bm.description.c_str());
        ImGui::PopID();
      }
    }
    
    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace yaze

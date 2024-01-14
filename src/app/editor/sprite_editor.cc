#include "sprite_editor.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::Button;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

absl::Status SpriteEditor::Update() {
  if (rom()->isLoaded() && !sheets_loaded_) {
    // Load the values for current_sheets_ array

    sheets_loaded_ = true;
  }

  // if (ImGui::BeginTable({"Canvas", "Graphics"}, 2, nullptr, ImVec2(0, 0))) {
  //   TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
  //                    ImGui::GetContentRegionAvail().x);
  //   TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
  //   TableHeadersRow();
  //   TableNextRow();
  //   TableNextColumn();
  //   DrawSpriteCanvas();

  //   TableNextColumn();
  //   if (sheets_loaded_) {
  //     DrawCurrentSheets();
  //   }
    
  //   ImGui::EndTable();
  // }

  return absl::OkStatus();
}

void SpriteEditor::DrawEditorTable() {}

void SpriteEditor::DrawSpriteCanvas() {}

void SpriteEditor::DrawCurrentSheets() {
  static gui::Canvas graphics_sheet_canvas;
  for (int i = 0; i < 8; i++) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)7);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar |
                              ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
      graphics_sheet_canvas.DrawBackground(ImVec2(0x200 * 8, 0x200 * 8));
      ImGui::PopStyleVar(2);
      graphics_sheet_canvas.DrawContextMenu();
      graphics_sheet_canvas.DrawBitmap(
          *rom()->bitmap_manager()[current_sheets_[i]], 2, 2);
      graphics_sheet_canvas.DrawGrid(64.0f);
      graphics_sheet_canvas.DrawOverlay();
    }
    ImGui::EndChild();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
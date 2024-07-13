#include "sprite_editor.h"

#include <gui/input.h>

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
  if (rom()->is_loaded() && !sheets_loaded_) {
    // Load the values for current_sheets_ array
    sheets_loaded_ = true;
  }

  if (ImGui::BeginTable("##SpriteCanvasTable", 2, ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    TableNextColumn();
    DrawSpriteCanvas();

    TableNextColumn();
    if (sheets_loaded_) {
      DrawCurrentSheets();
    }
    ImGui::EndTable();
  }

  return absl::OkStatus();
}

void SpriteEditor::DrawSpriteCanvas() {
  if (ImGui::BeginChild(gui::GetID("##SpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
    sprite_canvas_.DrawBackground(ImVec2(0x200, 0x200));
    sprite_canvas_.DrawContextMenu();
    // sprite_canvas_.DrawBitmap(oam_bitmap_, 2, 2);
    sprite_canvas_.DrawGrid(8.0f);
    sprite_canvas_.DrawOverlay();

    // Draw a table with OAM configuration
    // X, Y, Tile, Palette, Priority, Flip X, Flip Y
    if (ImGui::BeginTable("##OAMTable", 7, ImGuiTableFlags_None,
                          ImVec2(0, 0))) {
      TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Flip X", ImGuiTableColumnFlags_WidthStretch);
      TableSetupColumn("Flip Y", ImGuiTableColumnFlags_WidthStretch);
      TableHeadersRow();
      TableNextRow();

      TableNextColumn();
      gui::InputHexWord("", &oam_config_.x);

      TableNextColumn();
      gui::InputHexWord("", &oam_config_.y);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.tile);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.palette);

      TableNextColumn();
      gui::InputHexByte("", &oam_config_.priority);

      TableNextColumn();
      ImGui::Checkbox("", &oam_config_.flip_x);

      TableNextColumn();
      ImGui::Checkbox("", &oam_config_.flip_y);

      ImGui::EndTable();
    }

    ImGui::EndChild();
  }
}

void SpriteEditor::DrawCurrentSheets() {
  for (int i = 0; i < 8; i++) {
    // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // std::string sheet_label = absl::StrFormat("Sheet %d", i);
    // gui::InputHexByte(sheet_label.c_str(), &current_sheets_[i]);
    if (ImGui::BeginChild(gui::GetID("sheet_label"),
                          ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                          ImGuiWindowFlags_NoDecoration)) {
      static gui::Canvas graphics_sheet_canvas;
      graphics_sheet_canvas.DrawBackground(ImVec2(0x80 * 2, 0x20 * 2));
      // ImGui::PopStyleVar(2);
      graphics_sheet_canvas.DrawContextMenu();
      graphics_sheet_canvas.DrawBitmap(
          *rom()->bitmap_manager()[current_sheets_[i]], 2, 2, 2);
      graphics_sheet_canvas.DrawGrid(64.0f);
      graphics_sheet_canvas.DrawOverlay();
      ImGui::EndChild();
    }
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
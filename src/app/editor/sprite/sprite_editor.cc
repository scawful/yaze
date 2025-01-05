#include "sprite_editor.h"

#include "app/core/platform/file_dialog.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndTable;
using ImGui::Selectable;
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

  if (ImGui::BeginTabBar("##SpriteEditorTabs")) {
    if (ImGui::BeginTabItem("Vanilla")) {
      DrawVanillaSpriteEditor();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Custom")) {
      DrawCustomSprites();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  return status_.ok() ? absl::OkStatus() : status_;
}

void SpriteEditor::DrawVanillaSpriteEditor() {
  if (ImGui::BeginTable("##SpriteCanvasTable", 3, ImGuiTableFlags_Resizable,
                        ImVec2(0, 0))) {
    TableSetupColumn("Sprites List", ImGuiTableColumnFlags_WidthFixed, 256);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    DrawSpritesList();

    TableNextColumn();
    static int next_tab_id = 0;

    if (ImGui::BeginTabBar("SpriteTabBar", kSpriteTabBarFlags)) {
      if (ImGui::TabItemButton(ICON_MD_ADD, kSpriteTabBarFlags)) {
        if (std::find(active_sprites_.begin(), active_sprites_.end(),
                      current_sprite_id_) != active_sprites_.end()) {
          // Room is already open
          next_tab_id++;
        }
        active_sprites_.push_back(next_tab_id++);  // Add new tab
      }

      // Submit our regular tabs
      for (int n = 0; n < active_sprites_.Size;) {
        bool open = true;

        if (active_sprites_[n] > sizeof(zelda3::kSpriteDefaultNames) / 4) {
          active_sprites_.erase(active_sprites_.Data + n);
          continue;
        }

        if (ImGui::BeginTabItem(
                zelda3::kSpriteDefaultNames[active_sprites_[n]].data(), &open,
                ImGuiTabItemFlags_None)) {
          DrawSpriteCanvas();
          ImGui::EndTabItem();
        }

        if (!open)
          active_sprites_.erase(active_sprites_.Data + n);
        else
          n++;
      }

      ImGui::EndTabBar();
    }

    TableNextColumn();
    if (sheets_loaded_) {
      DrawCurrentSheets();
    }
    ImGui::EndTable();
  }
}

void SpriteEditor::DrawSpriteCanvas() {
  static bool flip_x = false;
  static bool flip_y = false;
  if (ImGui::BeginChild(gui::GetID("##SpriteCanvas"),
                        ImGui::GetContentRegionAvail(), true)) {
    sprite_canvas_.DrawBackground();
    sprite_canvas_.DrawContextMenu();
    sprite_canvas_.DrawGrid();
    sprite_canvas_.DrawOverlay();

    // Draw a table with OAM configuration
    // X, Y, Tile, Palette, Priority, Flip X, Flip Y
    if (ImGui::BeginTable("##OAMTable", 7, ImGuiTableFlags_Resizable,
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
      if (ImGui::Checkbox("##XFlip", &flip_x)) {
        oam_config_.flip_x = flip_x;
      }

      TableNextColumn();
      if (ImGui::Checkbox("##YFlip", &flip_y)) {
        oam_config_.flip_y = flip_y;
      }

      ImGui::EndTable();
    }

    DrawAnimationFrames();

    DrawCustomSpritesMetadata();

    ImGui::EndChild();
  }
}

void SpriteEditor::DrawCurrentSheets() {
  if (ImGui::BeginChild(gui::GetID("sheet_label"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    for (int i = 0; i < 8; i++) {
      std::string sheet_label = absl::StrFormat("Sheet %d", i);
      gui::InputHexByte(sheet_label.c_str(), &current_sheets_[i]);
      if (i % 2 == 0) ImGui::SameLine();
    }

    graphics_sheet_canvas_.DrawBackground();
    graphics_sheet_canvas_.DrawContextMenu();
    graphics_sheet_canvas_.DrawTileSelector(32);
    for (int i = 0; i < 8; i++) {
      graphics_sheet_canvas_.DrawBitmap(
          GraphicsSheetManager::GetInstance().gfx_sheets().at(current_sheets_[i]), 1, (i * 0x40) + 1, 2);
    }
    graphics_sheet_canvas_.DrawGrid();
    graphics_sheet_canvas_.DrawOverlay();
    ImGui::EndChild();
  }
}

void SpriteEditor::DrawSpritesList() {
  if (ImGui::BeginChild(gui::GetID("##SpritesList"),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0), true,
                        ImGuiWindowFlags_NoDecoration)) {
    int i = 0;
    for (const auto each_sprite_name : zelda3::kSpriteDefaultNames) {
      rom()->resource_label()->SelectableLabelWithNameEdit(
          current_sprite_id_ == i, "Sprite Names", core::HexByte(i),
          zelda3::kSpriteDefaultNames[i].data());
      if (ImGui::IsItemClicked()) {
        current_sprite_id_ = i;
        if (!active_sprites_.contains(i)) {
          active_sprites_.push_back(i);
        }
      }
      i++;
    }
    ImGui::EndChild();
  }
}

void SpriteEditor::DrawAnimationFrames() {
  if (ImGui::Button("Add Frame")) {
    // Add a new frame
  }
  if (ImGui::Button("Remove Frame")) {
    // Remove the current frame
  }
}

void SpriteEditor::DrawCustomSprites() {
  if (BeginTable("##CustomSpritesTable", 3,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders |
                     ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable,
                 ImVec2(0, 0))) {
    TableSetupColumn("Metadata", ImGuiTableColumnFlags_WidthFixed, 256);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthFixed, 256);
    TableSetupColumn("TIlesheets", ImGuiTableColumnFlags_WidthFixed, 256);

    TableHeadersRow();
    TableNextRow();
    TableNextColumn();

    Separator();
    DrawCustomSpritesMetadata();

    TableNextColumn();
    DrawSpriteCanvas();

    TableNextColumn();
    DrawCurrentSheets();

    EndTable();
  }
}

void SpriteEditor::DrawCustomSpritesMetadata() {
  // ZSprite Maker format open file dialog
  if (ImGui::Button("Open ZSprite")) {
    // Open ZSprite file
    std::string file_path = core::FileDialogWrapper::ShowOpenFileDialog();
    if (!file_path.empty()) {
      zsprite::ZSprite zsprite;
      status_ = zsprite.Load(file_path);
      if (status_.ok()) {
        custom_sprites_.push_back(zsprite);
      }
    }
  }

  for (const auto custom_sprite : custom_sprites_) {
    Selectable("%s", custom_sprite.sprName.c_str());
    if (ImGui::IsItemClicked()) {
      current_sprite_id_ = 256 + stoi(custom_sprite.property_sprid.Text);
      if (!active_sprites_.contains(current_sprite_id_)) {
        active_sprites_.push_back(current_sprite_id_);
      }
    }
    Separator();
  }

  for (const auto custom_sprite : custom_sprites_) {
    // Draw the custom sprite metadata
    Text("Sprite ID: %s", custom_sprite.property_sprid.Text.c_str());
    Text("Sprite Name: %s", custom_sprite.property_sprname.Text.c_str());
    Text("Sprite Palette: %s", custom_sprite.property_palette.Text.c_str());
    Separator();
  }
}

}  // namespace editor
}  // namespace yaze

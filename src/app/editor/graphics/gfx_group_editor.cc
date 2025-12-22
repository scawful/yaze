#include "gfx_group_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginCombo;
using ImGui::BeginGroup;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::EndChild;
using ImGui::EndCombo;
using ImGui::EndGroup;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::GetContentRegionAvail;
using ImGui::GetStyle;
using ImGui::IsItemClicked;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::SetNextItemWidth;
using ImGui::SliderFloat;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

using gfx::kPaletteGroupNames;
using gfx::PaletteCategory;

namespace {

// Constants for sheet display
constexpr int kSheetDisplayWidth = 256;  // 2x scale from 128px sheets
constexpr int kSheetDisplayHeight = 64;  // 2x scale from 32px sheets
constexpr float kDefaultScale = 2.0f;
constexpr int kTileSize = 16;  // 8px tiles at 2x scale

// Draw a single sheet with proper scaling and unique ID
void DrawScaledSheet(gui::Canvas& canvas, gfx::Bitmap& sheet, int unique_id,
                     float scale = kDefaultScale) {
  PushID(unique_id);

  // Calculate scaled dimensions
  int display_width =
      static_cast<int>(gfx::kTilesheetWidth * scale);
  int display_height =
      static_cast<int>(gfx::kTilesheetHeight * scale);

  // Draw canvas background
  canvas.DrawBackground(ImVec2(display_width + 1, display_height + 1));
  canvas.DrawContextMenu();

  // Draw bitmap with proper scale
  canvas.DrawBitmap(sheet, 2, scale);

  // Draw grid at scaled tile size
  canvas.DrawGrid(static_cast<int>(8 * scale));
  canvas.DrawOverlay();

  PopID();
}

}  // namespace

absl::Status GfxGroupEditor::Update() {
  // Palette controls at top for all tabs
  DrawPaletteControls();
  Separator();

  if (BeginTabBar("##GfxGroupEditorTabs")) {
    if (BeginTabItem("Blocksets")) {
      gui::InputHexByte("Selected Blockset", &selected_blockset_,
                        static_cast<uint8_t>(0x24));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "blockset", "0x" + std::to_string(selected_blockset_),
          "Blockset " + std::to_string(selected_blockset_));
      DrawBlocksetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Roomsets")) {
      gui::InputHexByte("Selected Roomset", &selected_roomset_,
                        static_cast<uint8_t>(81));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "roomset", "0x" + std::to_string(selected_roomset_),
          "Roomset " + std::to_string(selected_roomset_));
      DrawRoomsetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Spritesets")) {
      gui::InputHexByte("Selected Spriteset", &selected_spriteset_,
                        static_cast<uint8_t>(143));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "spriteset", "0x" + std::to_string(selected_spriteset_),
          "Spriteset " + std::to_string(selected_spriteset_));
      DrawSpritesetViewer();
      EndTabItem();
    }

    EndTabBar();
  }

  return absl::OkStatus();
}

void GfxGroupEditor::DrawBlocksetViewer(bool sheet_only) {
  if (!game_data()) {
    Text("No game data loaded");
    return;
  }

  PushID("BlocksetViewer");

  if (BeginTable("##BlocksetTable", sheet_only ? 1 : 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       GetContentRegionAvail().x);
    }

    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed,
                     kSheetDisplayWidth + 16);
    TableHeadersRow();
    TableNextRow();

    if (!sheet_only) {
      TableNextColumn();
      BeginGroup();
      for (int idx = 0; idx < 8; idx++) {
        SetNextItemWidth(100.f);
        gui::InputHexByte(
            ("Slot " + std::to_string(idx)).c_str(),
            &game_data()->main_blockset_ids[selected_blockset_][idx]);
      }
      EndGroup();
    }

    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 8; idx++) {
      int sheet_id = game_data()->main_blockset_ids[selected_blockset_][idx];
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (use_custom_palette_ && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining blockset, slot, and sheet
      int unique_id = (selected_blockset_ << 16) | (idx << 8) | sheet_id;
      DrawScaledSheet(blockset_canvases_[idx], sheet, unique_id, view_scale_);
    }
    EndGroup();
    EndTable();
  }

  PopID();
}

void GfxGroupEditor::DrawRoomsetViewer() {
  if (!game_data()) {
    Text("No game data loaded");
    return;
  }

  PushID("RoomsetViewer");
  Text("Roomsets overwrite slots 4-7 of the main blockset");

  if (BeginTable("##RoomsTable", 3,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed, 120);
    TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                     GetContentRegionAvail().x);
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed,
                     kSheetDisplayWidth + 16);
    TableHeadersRow();
    TableNextRow();

    // Roomset list column
    TableNextColumn();
    if (BeginChild("##RoomsetListChild", ImVec2(0, 300))) {
      for (int idx = 0; idx < 0x51; idx++) {
        PushID(idx);
        std::string roomset_label = absl::StrFormat("0x%02X", idx);
        bool is_selected = (selected_roomset_ == idx);
        if (Selectable(roomset_label.c_str(), is_selected)) {
          selected_roomset_ = idx;
        }
        PopID();
      }
    }
    EndChild();

    // Inputs column
    TableNextColumn();
    BeginGroup();
    Text("Sheet IDs (overwrites slots 4-7):");
    for (int idx = 0; idx < 4; idx++) {
      SetNextItemWidth(100.f);
      gui::InputHexByte(
          ("Slot " + std::to_string(idx + 4)).c_str(),
          &game_data()->room_blockset_ids[selected_roomset_][idx]);
    }
    EndGroup();

    // Sheets column
    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 4; idx++) {
      int sheet_id = game_data()->room_blockset_ids[selected_roomset_][idx];
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (use_custom_palette_ && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining roomset, slot, and sheet
      int unique_id = (0x1000) | (selected_roomset_ << 8) | (idx << 4) | sheet_id;
      DrawScaledSheet(roomset_canvases_[idx], sheet, unique_id, view_scale_);
    }
    EndGroup();
    EndTable();
  }

  PopID();
}

void GfxGroupEditor::DrawSpritesetViewer(bool sheet_only) {
  if (!game_data()) {
    Text("No game data loaded");
    return;
  }

  PushID("SpritesetViewer");

  if (BeginTable("##SpritesTable", sheet_only ? 1 : 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
                 ImVec2(0, 0))) {
    if (!sheet_only) {
      TableSetupColumn("Inputs", ImGuiTableColumnFlags_WidthStretch,
                       GetContentRegionAvail().x);
    }
    TableSetupColumn("Sheets", ImGuiTableColumnFlags_WidthFixed,
                     kSheetDisplayWidth + 16);
    TableHeadersRow();
    TableNextRow();

    if (!sheet_only) {
      TableNextColumn();
      BeginGroup();
      Text("Sprite sheet IDs (base 115+):");
      for (int idx = 0; idx < 4; idx++) {
        SetNextItemWidth(100.f);
        gui::InputHexByte(
            ("Slot " + std::to_string(idx)).c_str(),
            &game_data()->spriteset_ids[selected_spriteset_][idx]);
      }
      EndGroup();
    }

    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 4; idx++) {
      int sheet_offset = game_data()->spriteset_ids[selected_spriteset_][idx];
      int sheet_id = 115 + sheet_offset;
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (use_custom_palette_ && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining spriteset, slot, and sheet
      int unique_id =
          (0x2000) | (selected_spriteset_ << 8) | (idx << 4) | sheet_offset;
      DrawScaledSheet(spriteset_canvases_[idx], sheet, unique_id, view_scale_);
    }
    EndGroup();
    EndTable();
  }

  PopID();
}

namespace {
void DrawPaletteFromPaletteGroup(gfx::SnesPalette& palette) {
  if (palette.empty()) {
    return;
  }
  for (size_t color_idx = 0; color_idx < palette.size(); color_idx++) {
    PushID(static_cast<int>(color_idx));
    if ((color_idx % 8) != 0) {
      SameLine(0.0f, GetStyle().ItemSpacing.y);
    }

    // Small icon of the color in the palette
    gui::SnesColorButton(absl::StrCat("Palette", color_idx), palette[color_idx],
                         ImGuiColorEditFlags_NoAlpha |
                             ImGuiColorEditFlags_NoPicker |
                             ImGuiColorEditFlags_NoTooltip);

    PopID();
  }
}
}  // namespace

void GfxGroupEditor::DrawPaletteControls() {
  if (!game_data()) {
    return;
  }

  // View scale control
  Text(ICON_MD_ZOOM_IN " View");
  SameLine();
  SetNextItemWidth(100.f);
  SliderFloat("##ViewScale", &view_scale_, 1.0f, 4.0f, "%.1fx");
  SameLine();

  // Palette category selector
  Text(ICON_MD_PALETTE " Palette");
  SameLine();
  SetNextItemWidth(150.f);

  // Use the category names array for display
  static constexpr int kNumPaletteCategories = 14;
  if (BeginCombo("##PaletteCategory",
                 gfx::kPaletteCategoryNames[selected_palette_category_].data())) {
    for (int cat = 0; cat < kNumPaletteCategories; cat++) {
      auto category = static_cast<PaletteCategory>(cat);
      bool is_selected = (selected_palette_category_ == category);
      if (Selectable(gfx::kPaletteCategoryNames[category].data(), is_selected)) {
        selected_palette_category_ = category;
        selected_palette_index_ = 0;
        UpdateCurrentPalette();
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndCombo();
  }

  SameLine();
  SetNextItemWidth(80.f);
  if (gui::InputHexByte("##PaletteIndex", &selected_palette_index_)) {
    UpdateCurrentPalette();
  }

  SameLine();
  ImGui::Checkbox("Apply", &use_custom_palette_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Apply selected palette to sheet previews");
  }

  // Show current palette preview
  if (current_palette_ && !current_palette_->empty()) {
    SameLine();
    DrawPaletteFromPaletteGroup(*current_palette_);
  }
}

void GfxGroupEditor::UpdateCurrentPalette() {
  if (!game_data()) {
    current_palette_ = nullptr;
    return;
  }

  auto& groups = game_data()->palette_groups;
  switch (selected_palette_category_) {
    case PaletteCategory::kSword:
      current_palette_ = groups.swords.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kShield:
      current_palette_ = groups.shields.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kClothes:
      current_palette_ = groups.armors.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kWorldColors:
      current_palette_ =
          groups.overworld_main.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kAreaColors:
      current_palette_ =
          groups.overworld_aux.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kGlobalSprites:
      current_palette_ =
          groups.global_sprites.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kSpritesAux1:
      current_palette_ =
          groups.sprites_aux1.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kSpritesAux2:
      current_palette_ =
          groups.sprites_aux2.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kSpritesAux3:
      current_palette_ =
          groups.sprites_aux3.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kDungeons:
      current_palette_ =
          groups.dungeon_main.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kWorldMap:
    case PaletteCategory::kDungeonMap:
      current_palette_ =
          groups.overworld_mini_map.mutable_palette(selected_palette_index_);
      break;
    case PaletteCategory::kTriforce:
    case PaletteCategory::kCrystal:
      current_palette_ =
          groups.object_3d.mutable_palette(selected_palette_index_);
      break;
    default:
      current_palette_ = nullptr;
      break;
  }
}

}  // namespace editor
}  // namespace yaze

#include "gfx_group_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginCombo;
using ImGui::BeginGroup;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::EndChild;
using ImGui::EndCombo;
using ImGui::EndGroup;
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
  int display_width = static_cast<int>(gfx::kTilesheetWidth * scale);
  int display_height = static_cast<int>(gfx::kTilesheetHeight * scale);

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
  if (!host_surface_hint_.empty()) {
    ImGui::TextDisabled("%s", host_surface_hint_.c_str());
    Separator();
  }

  // Palette controls at top for all tabs
  DrawPaletteControls();
  Separator();

  if (gui::BeginThemedTabBar("##GfxGroupEditorTabs")) {
    if (BeginTabItem("Blocksets")) {
      gui::InputHexByte("Selected Blockset", &Ws().selected_blockset,
                        static_cast<uint8_t>(0x24));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "blockset", "0x" + std::to_string(Ws().selected_blockset),
          "Blockset " + std::to_string(Ws().selected_blockset));
      DrawBlocksetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Roomsets")) {
      gui::InputHexByte("Selected Roomset", &Ws().selected_roomset,
                        static_cast<uint8_t>(81));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "roomset", "0x" + std::to_string(Ws().selected_roomset),
          "Roomset " + std::to_string(Ws().selected_roomset));
      DrawRoomsetViewer();
      EndTabItem();
    }

    if (BeginTabItem("Spritesets")) {
      gui::InputHexByte("Selected Spriteset", &Ws().selected_spriteset,
                        static_cast<uint8_t>(143));
      rom()->resource_label()->SelectableLabelWithNameEdit(
          false, "spriteset", "0x" + std::to_string(Ws().selected_spriteset),
          "Spriteset " + std::to_string(Ws().selected_spriteset));
      DrawSpritesetViewer();
      EndTabItem();
    }

    gui::EndThemedTabBar();
  }

  return absl::OkStatus();
}

void GfxGroupEditor::DrawBlocksetViewer(bool sheet_only) {
  if (!game_data()) {
    Text("No game data loaded");
    return;
  }

  PushID("BlocksetViewer");
  auto& ws = Ws();

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
        SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth());
        gui::InputHexByte(
            ("Slot " + std::to_string(idx)).c_str(),
            &game_data()->main_blockset_ids[ws.selected_blockset][idx]);
      }
      EndGroup();
    }

    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 8; idx++) {
      int sheet_id = game_data()->main_blockset_ids[ws.selected_blockset][idx];
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (ws.use_custom_palette && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining blockset, slot, and sheet
      int unique_id = (ws.selected_blockset << 16) | (idx << 8) | sheet_id;
      DrawScaledSheet(blockset_canvases_[idx], sheet, unique_id, ws.view_scale);
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
  auto& ws = Ws();
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
        bool is_selected = (ws.selected_roomset == static_cast<uint8_t>(idx));
        if (Selectable(roomset_label.c_str(), is_selected)) {
          ws.selected_roomset = static_cast<uint8_t>(idx);
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
      SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth());
      gui::InputHexByte(
          ("Slot " + std::to_string(idx + 4)).c_str(),
          &game_data()->room_blockset_ids[ws.selected_roomset][idx]);
    }
    EndGroup();

    // Sheets column
    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 4; idx++) {
      int sheet_id = game_data()->room_blockset_ids[ws.selected_roomset][idx];
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (ws.use_custom_palette && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining roomset, slot, and sheet
      int unique_id =
          (0x1000) | (ws.selected_roomset << 8) | (idx << 4) | sheet_id;
      DrawScaledSheet(roomset_canvases_[idx], sheet, unique_id, ws.view_scale);
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
  auto& ws = Ws();

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
        SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth());
        gui::InputHexByte(
            ("Slot " + std::to_string(idx)).c_str(),
            &game_data()->spriteset_ids[ws.selected_spriteset][idx]);
      }
      EndGroup();
    }

    TableNextColumn();
    BeginGroup();
    for (int idx = 0; idx < 4; idx++) {
      int sheet_offset = game_data()->spriteset_ids[ws.selected_spriteset][idx];
      int sheet_id = 115 + sheet_offset;
      auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

      // Apply current palette if selected
      if (ws.use_custom_palette && current_palette_) {
        sheet.SetPalette(*current_palette_);
        gfx::Arena::Get().NotifySheetModified(sheet_id);
      }

      // Unique ID combining spriteset, slot, and sheet
      int unique_id =
          (0x2000) | (ws.selected_spriteset << 8) | (idx << 4) | sheet_offset;
      DrawScaledSheet(spriteset_canvases_[idx], sheet, unique_id,
                      ws.view_scale);
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

  auto& ws = Ws();

  // View scale control
  Text(ICON_MD_ZOOM_IN " View");
  SameLine();
  SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth());
  SliderFloat("##ViewScale", &ws.view_scale, 1.0f, 4.0f, "%.1fx");
  SameLine();

  // Palette category selector
  Text(ICON_MD_PALETTE " Palette");
  SameLine();
  SetNextItemWidth(gui::LayoutHelpers::GetComboWidth());

  // Use the category names array for display
  static constexpr int kNumPaletteCategories = 14;
  if (BeginCombo(
          "##PaletteCategory",
          gfx::kPaletteCategoryNames[ws.selected_palette_category].data())) {
    for (int cat = 0; cat < kNumPaletteCategories; cat++) {
      auto category = static_cast<PaletteCategory>(cat);
      bool is_selected = (ws.selected_palette_category == category);
      if (Selectable(gfx::kPaletteCategoryNames[category].data(),
                     is_selected)) {
        ws.selected_palette_category = category;
        ws.selected_palette_index = 0;
        UpdateCurrentPalette();
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndCombo();
  }

  SameLine();
  SetNextItemWidth(gui::LayoutHelpers::GetHexInputWidth());
  if (gui::InputHexByte("##PaletteIndex", &ws.selected_palette_index)) {
    UpdateCurrentPalette();
  }

  SameLine();
  ImGui::Checkbox("Apply", &ws.use_custom_palette);
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

  const auto& ws = Ws();
  auto& groups = game_data()->palette_groups;
  switch (ws.selected_palette_category) {
    case PaletteCategory::kSword:
      current_palette_ =
          groups.swords.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kShield:
      current_palette_ =
          groups.shields.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kClothes:
      current_palette_ =
          groups.armors.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kWorldColors:
      current_palette_ =
          groups.overworld_main.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kAreaColors:
      current_palette_ =
          groups.overworld_aux.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kGlobalSprites:
      current_palette_ =
          groups.global_sprites.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kSpritesAux1:
      current_palette_ =
          groups.sprites_aux1.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kSpritesAux2:
      current_palette_ =
          groups.sprites_aux2.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kSpritesAux3:
      current_palette_ =
          groups.sprites_aux3.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kDungeons:
      current_palette_ =
          groups.dungeon_main.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kWorldMap:
    case PaletteCategory::kDungeonMap:
      current_palette_ =
          groups.overworld_mini_map.mutable_palette(ws.selected_palette_index);
      break;
    case PaletteCategory::kTriforce:
    case PaletteCategory::kCrystal:
      current_palette_ =
          groups.object_3d.mutable_palette(ws.selected_palette_index);
      break;
    default:
      current_palette_ = nullptr;
      break;
  }
}

}  // namespace editor
}  // namespace yaze

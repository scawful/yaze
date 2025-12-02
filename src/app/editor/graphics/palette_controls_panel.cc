#include "app/editor/graphics/palette_controls_panel.h"

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using gfx::kPaletteGroupAddressesKeys;

void PaletteControlsPanel::Initialize() {
  // Initialize with default palette group
  state_->palette_group_index = 0;
  state_->palette_index = 0;
  state_->sub_palette_index = 0;
}

void PaletteControlsPanel::Draw(bool* p_open) {
  // EditorPanel interface - delegate to existing Update() logic
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextDisabled("Load a ROM to manage palettes");
    return;
  }

  DrawPresets();
  ImGui::Separator();
  DrawPaletteGroupSelector();
  ImGui::Separator();
  DrawPaletteDisplay();
  ImGui::Separator();
  DrawApplyButtons();
}

absl::Status PaletteControlsPanel::Update() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextDisabled("Load a ROM to manage palettes");
    return absl::OkStatus();
  }

  DrawPresets();
  ImGui::Separator();
  DrawPaletteGroupSelector();
  ImGui::Separator();
  DrawPaletteDisplay();
  ImGui::Separator();
  DrawApplyButtons();

  return absl::OkStatus();
}

void PaletteControlsPanel::DrawPresets() {
  gui::TextWithSeparators("Quick Presets");

  if (ImGui::Button(ICON_MD_LANDSCAPE " Overworld")) {
    state_->palette_group_index = 0;  // Dungeon Main (used for overworld too)
    state_->palette_index = 0;
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Standard overworld palette");

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_CASTLE " Dungeon")) {
    state_->palette_group_index = 0;  // Dungeon Main
    state_->palette_index = 1;
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Standard dungeon palette");

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_PERSON " Sprites")) {
    state_->palette_group_index = 4;  // Sprites Aux1
    state_->palette_index = 0;
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Sprite/enemy palette");

  if (ImGui::Button(ICON_MD_ACCOUNT_BOX " Link")) {
    state_->palette_group_index = 3;  // Sprite Aux3 (Link's palettes)
    state_->palette_index = 0;
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Link's palette");

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_MENU " HUD")) {
    state_->palette_group_index = 6;  // HUD palettes
    state_->palette_index = 0;
    state_->refresh_graphics = true;
  }
  HOVER_HINT("HUD/menu palette");
}

void PaletteControlsPanel::DrawPaletteGroupSelector() {
  gui::TextWithSeparators("Palette Selection");

  // Palette group combo
  ImGui::SetNextItemWidth(160);
  if (ImGui::Combo("Group", reinterpret_cast<int*>(&state_->palette_group_index),
                   kPaletteGroupAddressesKeys,
                   IM_ARRAYSIZE(kPaletteGroupAddressesKeys))) {
    state_->refresh_graphics = true;
  }

  // Palette index within group
  ImGui::SetNextItemWidth(100);
  int palette_idx = static_cast<int>(state_->palette_index);
  if (ImGui::InputInt("Palette", &palette_idx)) {
    state_->palette_index = static_cast<uint64_t>(std::max(0, palette_idx));
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Palette index within the group");

  // Sub-palette index (for multi-row palettes)
  ImGui::SetNextItemWidth(100);
  int sub_idx = static_cast<int>(state_->sub_palette_index);
  if (ImGui::InputInt("Sub-Palette", &sub_idx)) {
    state_->sub_palette_index = static_cast<uint64_t>(std::max(0, sub_idx));
    state_->refresh_graphics = true;
  }
  HOVER_HINT("Sub-palette row (0-7 for SNES 128-color palettes)");
}

void PaletteControlsPanel::DrawPaletteDisplay() {
  gui::TextWithSeparators("Current Palette");

  // Get the current palette from GameData
  if (!game_data_) return;
  auto palette_group_result = game_data_->palette_groups.get_group(
      kPaletteGroupAddressesKeys[state_->palette_group_index]);
  if (!palette_group_result) {
    ImGui::TextDisabled("Invalid palette group");
    return;
  }

  auto palette_group = *palette_group_result;
  if (state_->palette_index >= palette_group.size()) {
    ImGui::TextDisabled("Invalid palette index");
    return;
  }

  auto palette = palette_group.palette(state_->palette_index);

  // Display palette colors in rows of 16
  int colors_per_row = 16;
  int total_colors = static_cast<int>(palette.size());
  int num_rows = (total_colors + colors_per_row - 1) / colors_per_row;

  for (int row = 0; row < num_rows; row++) {
    for (int col = 0; col < colors_per_row; col++) {
      int idx = row * colors_per_row + col;
      if (idx >= total_colors) break;

      if (col > 0) ImGui::SameLine();

      auto& color = palette[idx];
      ImVec4 im_color(color.rgb().x / 255.0f, color.rgb().y / 255.0f,
                      color.rgb().z / 255.0f, 1.0f);

      // Highlight current sub-palette row
      bool in_sub_palette =
          (row == static_cast<int>(state_->sub_palette_index));
      if (in_sub_palette) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      }

      std::string id = absl::StrFormat("##PalColor%d", idx);
      if (ImGui::ColorButton(id.c_str(), im_color,
                             ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
        // Clicking a color in a row selects that sub-palette
        state_->sub_palette_index = static_cast<uint64_t>(row);
        state_->refresh_graphics = true;
      }

      if (in_sub_palette) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Index: %d (Row %d, Col %d)", idx, row, col);
        ImGui::Text("SNES: $%04X", color.snes());
        ImGui::Text("RGB: %d, %d, %d", static_cast<int>(color.rgb().x),
                    static_cast<int>(color.rgb().y),
                    static_cast<int>(color.rgb().z));
        ImGui::EndTooltip();
      }
    }
  }

  // Row selection buttons
  ImGui::Text("Sub-palette Row:");
  for (int i = 0; i < std::min(8, num_rows); i++) {
    if (i > 0) ImGui::SameLine();
    bool selected = (state_->sub_palette_index == static_cast<uint64_t>(i));
    if (selected) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
    }
    if (ImGui::SmallButton(absl::StrFormat("%d", i).c_str())) {
      state_->sub_palette_index = static_cast<uint64_t>(i);
      state_->refresh_graphics = true;
    }
    if (selected) {
      ImGui::PopStyleColor();
    }
  }
}

void PaletteControlsPanel::DrawApplyButtons() {
  gui::TextWithSeparators("Apply Palette");

  // Apply to current sheet
  ImGui::BeginDisabled(state_->open_sheets.empty());
  if (ImGui::Button(ICON_MD_BRUSH " Apply to Current Sheet")) {
    ApplyPaletteToSheet(state_->current_sheet_id);
  }
  ImGui::EndDisabled();
  HOVER_HINT("Apply palette to the currently selected sheet");

  ImGui::SameLine();

  // Apply to all sheets
  if (ImGui::Button(ICON_MD_FORMAT_PAINT " Apply to All Sheets")) {
    ApplyPaletteToAllSheets();
  }
  HOVER_HINT("Apply palette to all active graphics sheets");

  // Apply to selected sheets (multi-select)
  if (!state_->selected_sheets.empty()) {
    if (ImGui::Button(
            absl::StrFormat(ICON_MD_CHECKLIST " Apply to %zu Selected",
                            state_->selected_sheets.size())
                .c_str())) {
      for (uint16_t sheet_id : state_->selected_sheets) {
        ApplyPaletteToSheet(sheet_id);
      }
    }
    HOVER_HINT("Apply palette to all selected sheets in browser");
  }

  // Refresh button
  ImGui::Separator();
  if (ImGui::Button(ICON_MD_REFRESH " Refresh Graphics")) {
    state_->refresh_graphics = true;
    if (!state_->open_sheets.empty()) {
      ApplyPaletteToSheet(state_->current_sheet_id);
    }
  }
  HOVER_HINT("Force refresh of current sheet graphics");
}

void PaletteControlsPanel::ApplyPaletteToSheet(uint16_t sheet_id) {
  if (!rom_ || !rom_->is_loaded() || !game_data_) return;

  auto palette_group_result = game_data_->palette_groups.get_group(
      kPaletteGroupAddressesKeys[state_->palette_group_index]);
  if (!palette_group_result) return;

  auto palette_group = *palette_group_result;
  if (state_->palette_index >= palette_group.size()) return;

  auto palette = palette_group.palette(state_->palette_index);

  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);
  if (sheet.is_active() && sheet.surface()) {
    sheet.SetPaletteWithTransparent(palette, state_->sub_palette_index);
    gfx::Arena::Get().NotifySheetModified(sheet_id);
  }
}

void PaletteControlsPanel::ApplyPaletteToAllSheets() {
  if (!rom_ || !rom_->is_loaded() || !game_data_) return;

  auto palette_group_result = game_data_->palette_groups.get_group(
      kPaletteGroupAddressesKeys[state_->palette_group_index]);
  if (!palette_group_result) return;

  auto palette_group = *palette_group_result;
  if (state_->palette_index >= palette_group.size()) return;

  auto palette = palette_group.palette(state_->palette_index);

  for (int i = 0; i < zelda3::kNumGfxSheets; i++) {
    auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->data()[i];
    if (sheet.is_active() && sheet.surface()) {
      sheet.SetPaletteWithTransparent(palette, state_->sub_palette_index);
      gfx::Arena::Get().NotifySheetModified(i);
    }
  }
}

}  // namespace editor
}  // namespace yaze

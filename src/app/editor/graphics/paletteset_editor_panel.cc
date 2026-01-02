#include "paletteset_editor_panel.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginChild;
using ImGui::BeginGroup;
using ImGui::BeginTable;
using ImGui::EndChild;
using ImGui::EndGroup;
using ImGui::EndTable;
using ImGui::GetContentRegionAvail;
using ImGui::GetStyle;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::SetNextItemWidth;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

using gfx::kPaletteGroupNames;
using gfx::PaletteCategory;

absl::Status PalettesetEditorPanel::Update() {
  if (!rom() || !rom()->is_loaded() || !game_data()) {
    Text("No ROM loaded. Please open a Zelda 3 ROM.");
    return absl::OkStatus();
  }

  // Header with controls
  Text(ICON_MD_PALETTE " Paletteset Editor");
  SameLine();
  ImGui::Checkbox("Show All Colors", &show_all_colors_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show full 16-color palettes instead of 8");
  }
  Separator();

  // Two-column layout: list on left, editor on right
  if (BeginTable("##PalettesetLayout", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    TableSetupColumn("Palettesets", ImGuiTableColumnFlags_WidthFixed, 200);
    TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    DrawPalettesetList();

    TableNextColumn();
    DrawPalettesetEditor();

    EndTable();
  }

  return absl::OkStatus();
}

void PalettesetEditorPanel::DrawPalettesetList() {
  if (BeginChild("##PalettesetListChild", ImVec2(0, 400))) {
    for (uint8_t idx = 0; idx < 72; idx++) {
      PushID(idx);

      std::string label = absl::StrFormat("0x%02X", idx);
      bool is_selected = (selected_paletteset_ == idx);

      // Show custom name if available
      std::string display_name = label;
      // if (rom()->resource_label()->HasLabel("paletteset", label)) {
      //   display_name =
      //       rom()->resource_label()->GetLabel("paletteset", label) + " (" +
      //       label + ")";
      // }

      if (Selectable(display_name.c_str(), is_selected)) {
        selected_paletteset_ = idx;
      }

      PopID();
    }
  }
  EndChild();
}

void PalettesetEditorPanel::DrawPalettesetEditor() {
  if (selected_paletteset_ >= 72) {
    selected_paletteset_ = 71;
  }

  // Paletteset name editing
  std::string paletteset_label =
      absl::StrFormat("Paletteset 0x%02X", selected_paletteset_);
  Text("%s", paletteset_label.c_str());

  rom()->resource_label()->SelectableLabelWithNameEdit(
      false, "paletteset", "0x" + std::to_string(selected_paletteset_),
      paletteset_label);

  Separator();

  // Get the paletteset data
  auto& paletteset_ids = game_data()->paletteset_ids[selected_paletteset_];

  // Dungeon Main Palette
  BeginGroup();
  Text(ICON_MD_LANDSCAPE " Dungeon Main Palette");
  SetNextItemWidth(80.f);
  gui::InputHexByte("##DungeonMainIdx", &paletteset_ids[0]);
  SameLine();
  Text("Index: %d", paletteset_ids[0]);

  auto* dungeon_palette =
      game_data()->palette_groups.dungeon_main.mutable_palette(
          paletteset_ids[0]);
  if (dungeon_palette) {
    DrawPalettePreview(*dungeon_palette, "dungeon_main");
  }
  EndGroup();

  Separator();

  // Sprite Auxiliary Palettes
  const char* sprite_labels[] = {"Sprite Aux 1", "Sprite Aux 2",
                                 "Sprite Aux 3"};
  const char* sprite_icons[] = {ICON_MD_PERSON, ICON_MD_PETS,
                                ICON_MD_SMART_TOY};
  gfx::PaletteGroup* sprite_groups[] = {
      &game_data()->palette_groups.sprites_aux1,
      &game_data()->palette_groups.sprites_aux2,
      &game_data()->palette_groups.sprites_aux3};

  for (int slot = 0; slot < 3; slot++) {
    PushID(slot);
    BeginGroup();

    Text("%s %s", sprite_icons[slot], sprite_labels[slot]);
    SetNextItemWidth(80.f);
    gui::InputHexByte("##SpriteAuxIdx", &paletteset_ids[slot + 1]);
    SameLine();
    Text("Index: %d", paletteset_ids[slot + 1]);

    auto* sprite_palette =
        sprite_groups[slot]->mutable_palette(paletteset_ids[slot + 1]);
    if (sprite_palette) {
      DrawPalettePreview(*sprite_palette, sprite_labels[slot]);
    }

    EndGroup();
    if (slot < 2) {
      Separator();
    }

    PopID();
  }
}

void PalettesetEditorPanel::DrawPalettePreview(gfx::SnesPalette& palette,
                                               const char* label) {
  PushID(label);
  DrawPaletteGrid(palette, false);
  PopID();
}

void PalettesetEditorPanel::DrawPaletteGrid(gfx::SnesPalette& palette,
                                            bool editable) {
  if (palette.empty()) {
    Text("(Empty palette)");
    return;
  }

  size_t colors_to_show = show_all_colors_ ? palette.size() : 8;
  colors_to_show = std::min(colors_to_show, palette.size());

  for (size_t color_idx = 0; color_idx < colors_to_show; color_idx++) {
    PushID(static_cast<int>(color_idx));

    if ((color_idx % 8) != 0) {
      SameLine(0.0f, GetStyle().ItemSpacing.y);
    }

    ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip;
    if (!editable) {
      flags |= ImGuiColorEditFlags_NoPicker;
    }

    if (gui::SnesColorButton(absl::StrCat("Color", color_idx),
                             palette[color_idx], flags)) {
      // Color was clicked - could open color picker if editable
    }

    if (ImGui::IsItemHovered()) {
      auto& color = palette[color_idx];
      ImGui::SetTooltip("Color %zu\nRGB: %d, %d, %d\nSNES: $%04X", color_idx,
                        color.rom_color().red, color.rom_color().green,
                        color.rom_color().blue, color.snes());
    }

    PopID();
  }

  if (!show_all_colors_ && palette.size() > 8) {
    SameLine();
    Text("(+%zu more)", palette.size() - 8);
  }
}

}  // namespace editor
}  // namespace yaze

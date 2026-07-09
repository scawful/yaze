#include "app/editor/overworld/overworld_sidebar.h"
#include "util/i18n/tr.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {
namespace editor {

using ImGui::Separator;
using ImGui::Text;

OverworldSidebar::OverworldSidebar(zelda3::Overworld* overworld, Rom* rom,
                                   MapPropertiesSystem* map_properties_system)
    : overworld_(overworld),
      rom_(rom),
      map_properties_system_(map_properties_system) {}

void OverworldSidebar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, int& game_state,
                            bool& show_custom_bg_color_editor,
                            bool& show_overlay_editor) {
  if (!overworld_->is_loaded()) {
    return;
  }

  // Use a child window for the sidebar to allow scrolling
  if (ImGui::BeginChild("OverworldSidebar", ImVec2(0, 0), false,
                        ImGuiWindowFlags_None)) {
    ImGui::PushID("OverworldSidebar");
    DrawMapSelection(current_world, current_map, current_map_lock);

    if (overworld_->overworld_map(current_map) == nullptr) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::TextDisabled(tr("Current map selection is invalid."));
      ImGui::TextDisabled(
          tr("Hover or jump to a valid overworld map to continue."));
      ImGui::PopID();
      ImGui::EndChild();
      return;
    }

    ImGui::Spacing();
    Separator();
    ImGui::Spacing();

    // Use CollapsingHeader layout for better visibility and configurability
    if (ImGui::CollapsingHeader(tr("General Settings"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawBasicPropertiesTab(current_map, game_state,
                             show_custom_bg_color_editor, show_overlay_editor);
    }

    if (ImGui::CollapsingHeader(tr("Graphics"),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawGraphicsTab(current_map, game_state);
    }

    if (ImGui::CollapsingHeader(tr("Sprites"))) {
      DrawSpritePropertiesTab(current_map, game_state);
    }

    if (ImGui::CollapsingHeader(tr("Music"))) {
      DrawMusicTab(current_map);
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void OverworldSidebar::DrawBasicPropertiesTab(int current_map, int& game_state,
                                              bool& show_custom_bg_color_editor,
                                              bool& show_overlay_editor) {
  ImGui::Spacing();
  DrawConfiguration(current_map, game_state, show_overlay_editor);
  ImGui::Spacing();
  Separator();
  ImGui::Spacing();
  DrawPaletteSettings(current_map, game_state, show_custom_bg_color_editor);
}

void OverworldSidebar::DrawGraphicsTab(int current_map, int game_state) {
  ImGui::Spacing();
  DrawGraphicsSettings(current_map, game_state);
}

void OverworldSidebar::DrawSpritePropertiesTab(int current_map,
                                               int game_state) {
  ImGui::Spacing();
  // Reuse existing logic from MapPropertiesSystem if possible, or reimplement
  // Here we'll reimplement a simplified version based on what was in MapPropertiesSystem

  ImGui::Text(ICON_MD_PEST_CONTROL_RODENT " Sprite Settings");
  ImGui::Separator();

  // Sprite Graphics (already in Graphics tab, but useful here too)
  uint8_t sprite_gfx =
      overworld_->overworld_map(current_map)->sprite_graphics(game_state);
  if (gui::InputHexByte("Sprite GFX", &sprite_gfx, kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kSpriteGraphics, game_state,
         sprite_gfx});
  }

  // Sprite Palette (already in General->Palettes, but useful here too)
  uint8_t sprite_palette =
      overworld_->overworld_map(current_map)->sprite_palette(game_state);
  if (gui::InputHexByte("Sprite Palette", &sprite_palette,
                        kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kSpritePalette, game_state,
         sprite_palette});
  }
}

void OverworldSidebar::DrawMusicTab(int current_map) {
  ImGui::Spacing();
  ImGui::Text(ICON_MD_MUSIC_NOTE " Music Settings");
  ImGui::Separator();

  for (int i = 0; i < 4; ++i) {
    uint8_t music = overworld_->overworld_map(current_map)->area_music(i);
    const std::string label = absl::StrFormat("Music Byte %d", i + 1);
    if (gui::InputHexByte(label.c_str(), &music, kHexByteInputWidth)) {
      map_properties_system_->ApplyPropertyEdit(
          {current_map, OverworldPropertyField::kMusic, i, music});
    }
  }
}

void OverworldSidebar::DrawMapSelection(int& current_world, int& current_map,
                                        bool& current_map_lock) {
  ImGui::Text(ICON_MD_MAP " Current Map");

  const int clamped_world = std::clamp(current_world, 0, 2);
  ImGui::TextDisabled(tr("World: %s"), kWorldNames[clamped_world]);

  ImGui::BeginGroup();
  ImGui::Text(tr("ID: %02X"), current_map);
  ImGui::SameLine();
  if (ImGui::Button(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN)) {
    current_map_lock = !current_map_lock;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(current_map_lock ? "Unlock Map" : "Lock Map");
  }
  ImGui::EndGroup();
}

void OverworldSidebar::DrawGraphicsSettings(int current_map, int game_state) {
  ImGui::Text(ICON_MD_IMAGE " Graphics");

  // Area Graphics
  uint8_t area_graphics =
      overworld_->overworld_map(current_map)->area_graphics();
  if (gui::InputHexByte("Area GFX", &area_graphics, kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kAreaGraphics, 0, area_graphics});
  }

  // Sprite Graphics
  uint8_t sprite_graphics =
      overworld_->overworld_map(current_map)->sprite_graphics(game_state);
  if (gui::InputHexByte("Spr GFX", &sprite_graphics, kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kSpriteGraphics, game_state,
         sprite_graphics});
  }

  // Animated Graphics (v3+)
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (zelda3::OverworldVersionHelper::SupportsAnimatedGFX(rom_version)) {
    uint8_t animated_gfx =
        overworld_->overworld_map(current_map)->animated_gfx();
    if (gui::InputHexByte("Ani GFX", &animated_gfx, kHexByteInputWidth)) {
      map_properties_system_->ApplyPropertyEdit(
          {current_map, OverworldPropertyField::kAnimatedGraphics, 0,
           animated_gfx});
    }
  }

  // Custom Tile Graphics (v1+)
  if (zelda3::OverworldVersionHelper::SupportsExpandedSpace(rom_version)) {
    if (ImGui::TreeNode("Custom Tile Graphics")) {
      if (ImGui::BeginTable("CustomTileGraphics", 2,
                            ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < 8; i++) {
          ImGui::TableNextColumn();
          std::string label = absl::StrFormat("Sheet %d", i);
          uint8_t custom_tileset =
              overworld_->overworld_map(current_map)->custom_tileset(i);
          if (gui::InputHexByte(label.c_str(), &custom_tileset,
                                kHexByteInputWidth)) {
            map_properties_system_->ApplyPropertyEdit(
                {current_map, OverworldPropertyField::kCustomTileset, i,
                 custom_tileset});
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(tr("Custom graphics sheet %d (0x00-0xFF)"), i);
          }
        }
        ImGui::EndTable();
      }
      ImGui::TreePop();
    }
  }
}

void OverworldSidebar::DrawPaletteSettings(int current_map, int game_state,
                                           bool& show_custom_bg_color_editor) {
  ImGui::Text(ICON_MD_PALETTE " Palettes");

  // Area Palette
  uint8_t area_palette = overworld_->overworld_map(current_map)->area_palette();
  if (gui::InputHexByte("Area Pal", &area_palette, kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kAreaPalette, 0, area_palette});
  }

  // Main Palette (v2+)
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    uint8_t main_palette =
        overworld_->overworld_map(current_map)->main_palette();
    if (gui::InputHexByte("Main Pal", &main_palette, kHexByteInputWidth)) {
      map_properties_system_->ApplyPropertyEdit(
          {current_map, OverworldPropertyField::kMainPalette, 0, main_palette});
    }
  }

  // Sprite Palette
  uint8_t sprite_palette =
      overworld_->overworld_map(current_map)->sprite_palette(game_state);
  if (gui::InputHexByte("Spr Pal", &sprite_palette, kHexByteInputWidth)) {
    map_properties_system_->ApplyPropertyEdit(
        {current_map, OverworldPropertyField::kSpritePalette, game_state,
         sprite_palette});
  }

  // Custom Background Color Button
  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    if (ImGui::Button(ICON_MD_FORMAT_COLOR_FILL " Custom BG Color")) {
      show_custom_bg_color_editor = !show_custom_bg_color_editor;
    }
  }
}

void OverworldSidebar::DrawConfiguration(int current_map, int& game_state,
                                         bool& show_overlay_editor) {
  if (ImGui::BeginTable("ConfigTable", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

    // Game State
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_GAMEPAD " Game State");
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##GameState", &game_state, kGameStateNames, 3)) {
      map_properties_system_->RefreshMapProperties();
      map_properties_system_->RefreshOverworldMap();
    }

    // Area Size
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_PHOTO_SIZE_SELECT_LARGE " Area Size");
    ImGui::TableNextColumn();

    auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
    int current_area_size =
        static_cast<int>(overworld_->overworld_map(current_map)->area_size());

    ImGui::SetNextItemWidth(-1);
    if (zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
      if (ImGui::Combo("##AreaSize", &current_area_size, kAreaSizeNames, 4)) {
        map_properties_system_->ApplyPropertyEdit(
            {current_map, OverworldPropertyField::kAreaSize, 0,
             current_area_size});
      }
    } else {
      const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1)
                             ? current_area_size
                             : 0;
      if (ImGui::Combo("##AreaSize", &limited_size, limited_names, 2)) {
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea
                                        : zelda3::AreaSizeEnum::SmallArea;
        map_properties_system_->ApplyPropertyEdit(
            {current_map, OverworldPropertyField::kAreaSize, 0,
             static_cast<int>(size)});
      }
    }

    // Message ID
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MESSAGE " Message ID");
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    uint16_t message_id = overworld_->overworld_map(current_map)->message_id();
    if (gui::InputHexWordCustom("##MsgID", &message_id, kHexWordInputWidth)) {
      map_properties_system_->ApplyPropertyEdit(
          {current_map, OverworldPropertyField::kMessageId, 0, message_id});
    }

    ImGui::EndTable();
  }

  // Visual Effects (Overlay)
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (rom_version != zelda3::OverworldVersion::kVanilla) {
    if (ImGui::Button(ICON_MD_LAYERS " Visual Effects", ImVec2(-1, 0))) {
      show_overlay_editor = !show_overlay_editor;
    }
  }

  // Mosaic Settings
  ImGui::Separator();
  ImGui::Text(ICON_MD_GRID_ON " Mosaic");

  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    auto* current_map_ptr = overworld_->mutable_overworld_map(current_map);
    std::array<bool, 4> mosaic_expanded = current_map_ptr->mosaic_expanded();
    const char* direction_names[] = {"North", "South", "East", "West"};

    if (ImGui::BeginTable("MosaicTable", 2)) {
      for (int i = 0; i < 4; i++) {
        ImGui::TableNextColumn();
        if (ImGui::Checkbox(direction_names[i], &mosaic_expanded[i])) {
          map_properties_system_->ApplyPropertyEdit(
              {current_map, OverworldPropertyField::kMosaicExpanded, i,
               mosaic_expanded[i] ? 1 : 0});
        }
      }
      ImGui::EndTable();
    }
  } else {
    bool mosaic =
        *overworld_->mutable_overworld_map(current_map)->mutable_mosaic();
    if (ImGui::Checkbox(tr("Mosaic Effect"), &mosaic)) {
      map_properties_system_->ApplyPropertyEdit(
          {current_map, OverworldPropertyField::kMosaic, 0, mosaic ? 1 : 0});
    }
  }
}

}  // namespace editor
}  // namespace yaze

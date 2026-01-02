#include "app/editor/overworld/overworld_sidebar.h"

#include "absl/strings/str_format.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
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
    DrawMapSelection(current_world, current_map, current_map_lock);

    ImGui::Spacing();
    Separator();
    ImGui::Spacing();

    // Use CollapsingHeader layout for better visibility and configurability
    if (ImGui::CollapsingHeader("General Settings",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawBasicPropertiesTab(current_map, game_state,
                             show_custom_bg_color_editor, show_overlay_editor);
    }

    if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
      DrawGraphicsTab(current_map, game_state);
    }

    if (ImGui::CollapsingHeader("Sprites")) {
      DrawSpritePropertiesTab(current_map, game_state);
    }

    if (ImGui::CollapsingHeader("Music")) {
      DrawMusicTab(current_map);
    }
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
  if (gui::InputHexByte("Sprite GFX",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_sprite_graphics(game_state),
                        kHexByteInputWidth)) {
    map_properties_system_->ForceRefreshGraphics(current_map);
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshOverworldMap();
  }

  // Sprite Palette (already in General->Palettes, but useful here too)
  if (gui::InputHexByte("Sprite Palette",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_sprite_palette(game_state),
                        kHexByteInputWidth)) {
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshOverworldMap();
  }
}

void OverworldSidebar::DrawMusicTab(int current_map) {
  ImGui::Spacing();
  ImGui::Text(ICON_MD_MUSIC_NOTE " Music Settings");
  ImGui::Separator();

  // Music byte 1
  uint8_t* music_byte =
      overworld_->mutable_overworld_map(current_map)->mutable_area_music(0);
  if (gui::InputHexByte("Music Byte 1", music_byte, kHexByteInputWidth)) {
    // No refresh needed usually
  }

  // Music byte 2
  music_byte =
      overworld_->mutable_overworld_map(current_map)->mutable_area_music(1);
  if (gui::InputHexByte("Music Byte 2", music_byte, kHexByteInputWidth)) {
    // No refresh needed usually
  }

  // Music byte 3
  music_byte =
      overworld_->mutable_overworld_map(current_map)->mutable_area_music(2);
  if (gui::InputHexByte("Music Byte 3", music_byte, kHexByteInputWidth)) {
    // No refresh needed usually
  }

  // Music byte 4
  music_byte =
      overworld_->mutable_overworld_map(current_map)->mutable_area_music(3);
  if (gui::InputHexByte("Music Byte 4", music_byte, kHexByteInputWidth)) {
    // No refresh needed usually
  }
}

void OverworldSidebar::DrawMapSelection(int& current_world, int& current_map,
                                        bool& current_map_lock) {
  ImGui::Text(ICON_MD_MAP " Map Selection");

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::Combo("##world", &current_world, kWorldNames, 3);

  ImGui::BeginGroup();
  ImGui::Text("ID: %02X", current_map);
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
  if (gui::InputHexByte("Area GFX",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_area_graphics(),
                        kHexByteInputWidth)) {
    map_properties_system_->RefreshMapProperties();
    overworld_->mutable_overworld_map(current_map)->LoadAreaGraphics();
    map_properties_system_->RefreshSiblingMapGraphics(current_map);
    map_properties_system_->RefreshTile16Blockset();
    map_properties_system_->RefreshOverworldMap();
  }

  // Sprite Graphics
  if (gui::InputHexByte("Spr GFX",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_sprite_graphics(game_state),
                        kHexByteInputWidth)) {
    map_properties_system_->ForceRefreshGraphics(current_map);
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshOverworldMap();
  }

  // Animated Graphics (v3+)
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (zelda3::OverworldVersionHelper::SupportsAnimatedGFX(rom_version)) {
    if (gui::InputHexByte("Ani GFX",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_animated_gfx(),
                          kHexByteInputWidth)) {
      map_properties_system_->ForceRefreshGraphics(current_map);
      map_properties_system_->RefreshMapProperties();
      map_properties_system_->RefreshTile16Blockset();
      map_properties_system_->RefreshOverworldMap();
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
          if (gui::InputHexByte(label.c_str(),
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_custom_tileset(i),
                                kHexByteInputWidth)) {
            map_properties_system_->ForceRefreshGraphics(current_map);
            map_properties_system_->RefreshMapProperties();
            map_properties_system_->RefreshTile16Blockset();
            map_properties_system_->RefreshOverworldMap();
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Custom graphics sheet %d (0x00-0xFF)", i);
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
  if (gui::InputHexByte("Area Pal",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_area_palette(),
                        kHexByteInputWidth)) {
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshMapPalette();
    map_properties_system_->RefreshOverworldMap();
  }

  // Main Palette (v2+)
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
    if (gui::InputHexByte("Main Pal",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_main_palette(),
                          kHexByteInputWidth)) {
      map_properties_system_->RefreshMapProperties();
      map_properties_system_->RefreshMapPalette();
      map_properties_system_->RefreshOverworldMap();
    }
  }

  // Sprite Palette
  if (gui::InputHexByte("Spr Pal",
                        overworld_->mutable_overworld_map(current_map)
                            ->mutable_sprite_palette(game_state),
                        kHexByteInputWidth)) {
    map_properties_system_->RefreshMapProperties();
    map_properties_system_->RefreshOverworldMap();
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
        auto status = overworld_->ConfigureMultiAreaMap(
            current_map, static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok()) {
          map_properties_system_->RefreshSiblingMapGraphics(current_map, true);
          map_properties_system_->RefreshOverworldMap();
        }
      }
    } else {
      const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1)
                             ? current_area_size
                             : 0;
      if (ImGui::Combo("##AreaSize", &limited_size, limited_names, 2)) {
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = overworld_->ConfigureMultiAreaMap(current_map, size);
        if (status.ok()) {
          map_properties_system_->RefreshSiblingMapGraphics(current_map, true);
          map_properties_system_->RefreshOverworldMap();
        }
      }
    }

    // Message ID
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MESSAGE " Message ID");
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    if (gui::InputHexWordCustom("##MsgID",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_message_id(),
                                kHexWordInputWidth)) {
      map_properties_system_->RefreshMapProperties();
      map_properties_system_->RefreshOverworldMap();
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
          current_map_ptr->set_mosaic_expanded(i, mosaic_expanded[i]);
          map_properties_system_->RefreshMapProperties();
          map_properties_system_->RefreshOverworldMap();
        }
      }
      ImGui::EndTable();
    }
  } else {
    if (ImGui::Checkbox(
            "Mosaic Effect",
            overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      map_properties_system_->RefreshMapProperties();
      map_properties_system_->RefreshOverworldMap();
    }
  }
}

}  // namespace editor
}  // namespace yaze

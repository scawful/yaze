#include "app/editor/overworld/map_properties.h"

#include "app/core/performance_monitor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/gfx/atlas_renderer.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
// HOVER_HINT is defined in util/macro.h
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

// Using centralized UI constants

void MapPropertiesSystem::DrawSimplifiedMapSettings(
    int& current_world, int& current_map, bool& current_map_lock,
    bool& show_map_properties_panel, bool& show_custom_bg_color_editor,
    bool& show_overlay_editor, bool& show_overlay_preview, int& game_state,
    int& current_mode) {
  // Enhanced settings table with popup buttons for quick access and integrated toolset
  if (BeginTable("SimplifiedMapSettings", 9,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit,
                 ImVec2(0, 0), -1)) {
    ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnWorld);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnMap);
    ImGui::TableSetupColumn("Area Size", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnAreaSize);
    ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnLock);
    ImGui::TableSetupColumn("Graphics", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnGraphics);
    ImGui::TableSetupColumn("Palettes", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnPalettes);
    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnProperties);
    ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnView);
    ImGui::TableSetupColumn("Quick", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnQuick);

    TableNextColumn();
    ImGui::SetNextItemWidth(kComboWorldWidth);
    ImGui::Combo("##world", &current_world, kWorldNames, 3);

    TableNextColumn();
    ImGui::Text("%d (0x%02X)", current_map, current_map);

    TableNextColumn();
    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF) {
      int current_area_size =
          static_cast<int>(overworld_->overworld_map(current_map)->area_size());
      ImGui::SetNextItemWidth(kComboAreaSizeWidth);
      if (ImGui::Combo("##AreaSize", &current_area_size, kAreaSizeNames, 4)) {
        overworld_->mutable_overworld_map(current_map)
            ->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
        RefreshOverworldMap();
      }
    } else {
      ImGui::Text("N/A");
    }

    TableNextColumn();
    if (ImGui::Button(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
                      ImVec2(40, 0))) {
      current_map_lock = !current_map_lock;
    }
    HOVER_HINT(current_map_lock ? "Unlock Map" : "Lock Map");

    TableNextColumn();
    if (ImGui::Button("Graphics", ImVec2(kTableButtonGraphics, 0))) {
      ImGui::OpenPopup("GraphicsPopup");
    }
    HOVER_HINT("Graphics Settings");
    DrawGraphicsPopup(current_map, game_state);

    TableNextColumn();
    if (ImGui::Button("Palettes", ImVec2(kTableButtonPalettes, 0))) {
      ImGui::OpenPopup("PalettesPopup");
    }
    HOVER_HINT("Palette Settings");
    DrawPalettesPopup(current_map, game_state, show_custom_bg_color_editor);

    TableNextColumn();
    if (ImGui::Button("Properties", ImVec2(kTableButtonProperties, 0))) {
      ImGui::OpenPopup("PropertiesPopup");
    }
    HOVER_HINT("Map Properties & Overlays");
    DrawPropertiesPopup(current_map, show_map_properties_panel,
                        show_overlay_preview, game_state);

    TableNextColumn();
    // View Controls
    if (ImGui::Button("View", ImVec2(kTableButtonView, 0))) {
      ImGui::OpenPopup("ViewPopup");
    }
    HOVER_HINT("View Controls");
    DrawViewPopup();

    TableNextColumn();
    // Quick Access Tools
    if (ImGui::Button("Quick", ImVec2(kTableButtonQuick, 0))) {
      ImGui::OpenPopup("QuickPopup");
    }
    HOVER_HINT("Quick Access Tools");
    DrawQuickAccessPopup();

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawMapPropertiesPanel(
    int current_map, bool& show_map_properties_panel) {
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  // Header with map info and lock status
  ImGui::BeginGroup();
  Text("Current Map: %d (0x%02X)", current_map, current_map);
  ImGui::EndGroup();

  Separator();

  // Create tabs for different property categories
  if (ImGui::BeginTabBar("MapPropertiesTabs",
                         ImGuiTabBarFlags_FittingPolicyScroll)) {

    // Basic Properties Tab
    if (ImGui::BeginTabItem("Basic Properties")) {
      DrawBasicPropertiesTab(current_map);
      ImGui::EndTabItem();
    }

    // Sprite Properties Tab
    if (ImGui::BeginTabItem("Sprite Properties")) {
      DrawSpritePropertiesTab(current_map);
      ImGui::EndTabItem();
    }

    // Custom Overworld Features Tab
    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF && ImGui::BeginTabItem("Custom Features")) {
      DrawCustomFeaturesTab(current_map);
      ImGui::EndTabItem();
    }

    // Tile Graphics Tab
    if (ImGui::BeginTabItem("Tile Graphics")) {
      DrawTileGraphicsTab(current_map);
      ImGui::EndTabItem();
    }

    // Music Tab
    if (ImGui::BeginTabItem("Music")) {
      DrawMusicTab(current_map);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void MapPropertiesSystem::DrawCustomBackgroundColorEditor(
    int current_map, bool& show_custom_bg_color_editor) {
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version < 2) {
    Text("Custom background colors require ZSCustomOverworld v2+");
    return;
  }

  Text("Custom Background Color Editor");
  Separator();

  // Enable/disable area-specific background color
  static bool use_area_specific_bg_color = false;
  if (ImGui::Checkbox("Use Area-Specific Background Color",
                      &use_area_specific_bg_color)) {
    // Update ROM data
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] =
        use_area_specific_bg_color ? 1 : 0;
  }

  if (use_area_specific_bg_color) {
    // Get current color
    uint16_t current_color =
        overworld_->overworld_map(current_map)->area_specific_bg_color();
    gfx::SnesColor snes_color(current_color);

    // Convert to ImVec4 for color picker
    ImVec4 color_vec = gui::ConvertSnesColorToImVec4(snes_color);

    if (ImGui::ColorPicker4(
            "Background Color", (float*)&color_vec,
            ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex)) {
      // Convert back to SNES color and update
      gfx::SnesColor new_snes_color = gui::ConvertImVec4ToSnesColor(color_vec);
      overworld_->mutable_overworld_map(current_map)
          ->set_area_specific_bg_color(new_snes_color.snes());

      // Update ROM
      int rom_address =
          zelda3::OverworldCustomAreaSpecificBGPalette + (current_map * 2);
      (*rom_)[rom_address] = new_snes_color.snes() & 0xFF;
      (*rom_)[rom_address + 1] = (new_snes_color.snes() >> 8) & 0xFF;
    }

    Text("SNES Color: 0x%04X", current_color);
  }
}

void MapPropertiesSystem::DrawOverlayEditor(int current_map,
                                            bool& show_overlay_editor) {
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version < 1) {
    Text("Subscreen overlays require ZSCustomOverworld v1+");
    return;
  }

  Text("Overlay Editor");
  Separator();

  // Enable/disable subscreen overlay
  static bool use_subscreen_overlay = false;
  if (ImGui::Checkbox("Use Subscreen Overlay", &use_subscreen_overlay)) {
    // Update ROM data
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
        use_subscreen_overlay ? 1 : 0;
  }

  if (use_subscreen_overlay) {
    uint16_t current_overlay =
        overworld_->overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord("Overlay ID", &current_overlay,
                          kInputFieldSize + 20)) {
      overworld_->mutable_overworld_map(current_map)
          ->set_subscreen_overlay(current_overlay);

      // Update ROM
      int rom_address =
          zelda3::OverworldCustomSubscreenOverlayArray + (current_map * 2);
      (*rom_)[rom_address] = current_overlay & 0xFF;
      (*rom_)[rom_address + 1] = (current_overlay >> 8) & 0xFF;
    }

    Text("Common overlay IDs:");
    Text("0x0000 = None");
    Text("0x0001 = Map overlay");
    Text("0x0002 = Dungeon overlay");
  }
}

void MapPropertiesSystem::SetupCanvasContextMenu(
    gui::Canvas& canvas, int current_map, bool current_map_lock,
    bool& show_map_properties_panel, bool& show_custom_bg_color_editor,
    bool& show_overlay_editor) {
  // Clear any existing context menu items
  canvas.ClearContextMenuItems();

  // Add overworld-specific context menu items
  gui::Canvas::ContextMenuItem lock_item;
  lock_item.label = current_map_lock ? "Unlock Map" : "Lock to This Map";
  lock_item.callback = [&current_map_lock]() {
    current_map_lock = !current_map_lock;
  };
  canvas.AddContextMenuItem(lock_item);

  // Map Properties
  gui::Canvas::ContextMenuItem properties_item;
  properties_item.label = "Map Properties";
  properties_item.callback = [&show_map_properties_panel]() {
    show_map_properties_panel = true;
  };
  canvas.AddContextMenuItem(properties_item);

  // Custom overworld features (only show if v3+)
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version >= 3 && asm_version != 0xFF) {
    // Custom Background Color
    gui::Canvas::ContextMenuItem bg_color_item;
    bg_color_item.label = "Custom Background Color";
    bg_color_item.callback = [&show_custom_bg_color_editor]() {
      show_custom_bg_color_editor = true;
    };
    canvas.AddContextMenuItem(bg_color_item);

    // Overlay Settings
    gui::Canvas::ContextMenuItem overlay_item;
    overlay_item.label = "Overlay Settings";
    overlay_item.callback = [&show_overlay_editor]() {
      show_overlay_editor = true;
    };
    canvas.AddContextMenuItem(overlay_item);
  }

  // Canvas controls
  gui::Canvas::ContextMenuItem reset_pos_item;
  reset_pos_item.label = "Reset Canvas Position";
  reset_pos_item.callback = [&canvas]() {
    canvas.set_scrolling(ImVec2(0, 0));
  };
  canvas.AddContextMenuItem(reset_pos_item);

  gui::Canvas::ContextMenuItem zoom_fit_item;
  zoom_fit_item.label = "Zoom to Fit";
  zoom_fit_item.callback = [&canvas]() {
    canvas.set_global_scale(1.0f);
    canvas.set_scrolling(ImVec2(0, 0));
  };
  canvas.AddContextMenuItem(zoom_fit_item);
}

// Private method implementations
void MapPropertiesSystem::DrawGraphicsPopup(int current_map, int game_state) {
  if (ImGui::BeginPopup("GraphicsPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text("Graphics Settings");
    ImGui::Separator();

    if (gui::InputHexByteCustom("Area Graphics",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_area_graphics(),
                                kHexByteInputWidth)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    if (gui::InputHexByteCustom(
            absl::StrFormat("Sprite GFX (%s)", kGameStateNames[game_state])
                .c_str(),
            overworld_->mutable_overworld_map(current_map)
                ->mutable_sprite_graphics(game_state),
            kHexByteInputWidth)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 3) {
      if (gui::InputHexByte("Animated GFX",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }

    ImGui::Separator();
    ImGui::Text("Custom Tile Graphics (8 sheets):");

    // Show the 8 custom graphics IDs in a more accessible way
    for (int i = 0; i < 8; i++) {
      std::string label = absl::StrFormat("Sheet %d", i);
      if (gui::InputHexByte(label.c_str(),
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_custom_tileset(i),
                            80.f)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawPalettesPopup(int current_map, int game_state,
                                            bool& show_custom_bg_color_editor) {
  if (ImGui::BeginPopup("PalettesPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text("Palette Settings");
    ImGui::Separator();

    if (gui::InputHexByteCustom("Area Palette",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_area_palette(),
                                kHexByteInputWidth)) {
      RefreshMapProperties();
      auto status = RefreshMapPalette();
      RefreshOverworldMap();
    }

    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 2) {
      if (gui::InputHexByteCustom("Main Palette",
                                  overworld_->mutable_overworld_map(current_map)
                                      ->mutable_main_palette(),
                                  kHexByteInputWidth)) {
        RefreshMapProperties();
        auto status = RefreshMapPalette();
        RefreshOverworldMap();
      }
    }

    if (gui::InputHexByteCustom(
            absl::StrFormat("Sprite Palette (%s)", kGameStateNames[game_state])
                .c_str(),
            overworld_->mutable_overworld_map(current_map)
                ->mutable_sprite_palette(game_state),
            kHexByteInputWidth)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    ImGui::Separator();
    if (ImGui::Button("Background Color")) {
      show_custom_bg_color_editor = !show_custom_bg_color_editor;
    }

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawPropertiesPopup(int current_map,
                                              bool& show_map_properties_panel,
                                              bool& show_overlay_preview,
                                              int& game_state) {
  if (ImGui::BeginPopup("PropertiesPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text("Map Properties");
    ImGui::Separator();

    // Basic Map Properties Section
    ImGui::Text("Basic Properties");
    ImGui::Separator();

    if (gui::InputHexWordCustom("Message ID",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_message_id(),
                                kHexWordInputWidth)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    ImGui::SetNextItemWidth(kComboGameStateWidth);
    if (ImGui::Combo("Game State", &game_state, kGameStateNames, 3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    // Area Configuration Section
    ImGui::Separator();
    ImGui::Text("Area Configuration");
    ImGui::Separator();

    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF) {
      int current_area_size =
          static_cast<int>(overworld_->overworld_map(current_map)->area_size());
      ImGui::SetNextItemWidth(kComboAreaSizeWidth);
      if (ImGui::Combo("Area Size", &current_area_size, kAreaSizeNames, 4)) {
        overworld_->mutable_overworld_map(current_map)
            ->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
        RefreshOverworldMap();
      }
    } else {
      // Vanilla ROM - show small/large map controls
      auto* map = overworld_->mutable_overworld_map(current_map);
      bool is_small = !map->is_large_map();
      if (ImGui::Checkbox("Small Map", &is_small)) {
        if (is_small) {
          map->SetAsSmallMap();
        } else {
          // For vanilla, use default parent and quadrant values
          map->SetAsLargeMap(0, 0);
        }
        RefreshOverworldMap();
      }
    }

    // Visual Effects Section
    ImGui::Separator();
    ImGui::Text("Visual Effects");
    ImGui::Separator();

    DrawMosaicControls(current_map);
    DrawOverlayControls(current_map, show_overlay_preview);

    // Advanced Options Section
    ImGui::Separator();
    ImGui::Text("Advanced Options");
    ImGui::Separator();

    if (ImGui::Button("Full Properties Panel",
                      ImVec2(kLargeButtonWidth + 50, 0))) {
      show_map_properties_panel = true;
      ImGui::CloseCurrentPopup();
    }
    HOVER_HINT("Open comprehensive properties editor");

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawBasicPropertiesTab(int current_map) {
  if (BeginTable("BasicProperties", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text("Area Graphics");
    TableNextColumn();
    if (gui::InputHexByte("##AreaGfx",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Area Palette");
    TableNextColumn();
    if (gui::InputHexByte("##AreaPal",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      auto status = RefreshMapPalette();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Message ID");
    TableNextColumn();
    if (gui::InputHexWord("##MsgId",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_message_id(),
                          kInputFieldSize + 20)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Mosaic Effect");
    TableNextColumn();
    if (ImGui::Checkbox(
            "##mosaic",
            overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Enable Mosaic effect for the current map");

    // Add music editing controls
    TableNextColumn();
    ImGui::Text("Music (Beginning)");
    TableNextColumn();
    if (gui::InputHexByte("##Music0",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(0),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    HOVER_HINT("Music track for game beginning state");

    TableNextColumn();
    ImGui::Text("Music (Zelda)");
    TableNextColumn();
    if (gui::InputHexByte("##Music1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    HOVER_HINT("Music track for Zelda rescued state");

    TableNextColumn();
    ImGui::Text("Music (Master Sword)");
    TableNextColumn();
    if (gui::InputHexByte("##Music2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    HOVER_HINT("Music track for Master Sword obtained state");

    TableNextColumn();
    ImGui::Text("Music (Agahnim)");
    TableNextColumn();
    if (gui::InputHexByte("##Music3",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(3),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    HOVER_HINT("Music track for Agahnim defeated state");

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawSpritePropertiesTab(int current_map) {
  if (BeginTable("SpriteProperties", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text("Game State");
    TableNextColumn();
    static int game_state = 0;
    ImGui::SetNextItemWidth(100.f);
    if (ImGui::Combo("##GameState", &game_state, kGameStateNames, 3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Sprite Graphics 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_graphics(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Sprite Graphics 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_graphics(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Sprite Palette 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_palette(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    TableNextColumn();
    ImGui::Text("Sprite Palette 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_palette(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawCustomFeaturesTab(int current_map) {
  if (BeginTable("CustomFeatures", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text("Area Size");
    TableNextColumn();
    static const char* area_size_names[] = {"Small (1x1)", "Large (2x2)",
                                            "Wide (2x1)", "Tall (1x2)"};
    int current_area_size =
        static_cast<int>(overworld_->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("##AreaSize", &current_area_size, area_size_names, 4)) {
      overworld_->mutable_overworld_map(current_map)
          ->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
      RefreshOverworldMap();
    }

    static uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 2) {
      TableNextColumn();
      ImGui::Text("Main Palette");
      TableNextColumn();
      if (gui::InputHexByte("##MainPal",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        auto status = RefreshMapPalette();
        RefreshOverworldMap();
      }
    }

    if (asm_version >= 3) {
      TableNextColumn();
      ImGui::Text("Animated GFX");
      TableNextColumn();
      if (gui::InputHexByte("##AnimGfx",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }

      TableNextColumn();
      ImGui::Text("Subscreen Overlay");
      TableNextColumn();
      if (gui::InputHexWord("##SubOverlay",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_subscreen_overlay(),
                            kInputFieldSize + 20)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawTileGraphicsTab(int current_map) {
  ImGui::Text("Custom Tile Graphics (8 sheets per map):");
  Separator();

  if (BeginTable("TileGraphics", 4,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed, 80);

    for (int i = 0; i < 4; i++) {
      TableNextColumn();
      ImGui::Text("Sheet %d", i);
      TableNextColumn();
      if (gui::InputHexByte(absl::StrFormat("##TileGfx%d", i).c_str(),
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_custom_tileset(i),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }

      TableNextColumn();
      ImGui::Text("Sheet %d", i + 4);
      TableNextColumn();
      if (gui::InputHexByte(absl::StrFormat("##TileGfx%d", i + 4).c_str(),
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_custom_tileset(i + 4),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawMusicTab(int current_map) {
  ImGui::Text("Music Settings for Different Game States:");
  Separator();

  if (BeginTable("MusicSettings", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Game State", ImGuiTableColumnFlags_WidthFixed,
                            150);
    ImGui::TableSetupColumn("Music Track ID",
                            ImGuiTableColumnFlags_WidthStretch);

    const char* music_state_names[] = {"Beginning (Pre-Zelda)", "Zelda Rescued",
                                       "Master Sword Obtained",
                                       "Agahnim Defeated"};

    const char* music_descriptions[] = {
        "Music before rescuing Zelda",
        "Music after rescuing Zelda from Hyrule Castle",
        "Music after obtaining the Master Sword",
        "Music after defeating Agahnim (Dark World)"};

    for (int i = 0; i < 4; i++) {
      TableNextColumn();
      ImGui::Text("%s", music_state_names[i]);

      TableNextColumn();
      if (gui::InputHexByte(absl::StrFormat("##Music%d", i).c_str(),
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_area_music(i),
                            kInputFieldSize)) {
        RefreshMapProperties();

        // Update the ROM directly since music is not automatically saved
        int music_address = 0;
        switch (i) {
          case 0:
            music_address = zelda3::kOverworldMusicBeginning + current_map;
            break;
          case 1:
            music_address = zelda3::kOverworldMusicZelda + current_map;
            break;
          case 2:
            music_address = zelda3::kOverworldMusicMasterSword + current_map;
            break;
          case 3:
            music_address = zelda3::kOverworldMusicAgahnim + current_map;
            break;
        }

        if (music_address > 0) {
          (*rom_)[music_address] =
              *overworld_->mutable_overworld_map(current_map)
                   ->mutable_area_music(i);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", music_descriptions[i]);
      }
    }

    ImGui::EndTable();
  }

  Separator();
  ImGui::Text("Music tracks control the background music for different");
  ImGui::Text("game progression states on this overworld map.");

  // Show common music track IDs for reference
  Separator();
  ImGui::Text("Common Music Track IDs:");
  ImGui::BulletText("0x02 - Overworld Theme");
  ImGui::BulletText("0x05 - Kakariko Village");
  ImGui::BulletText("0x07 - Lost Woods");
  ImGui::BulletText("0x09 - Dark World Theme");
  ImGui::BulletText("0x0F - Ganon's Tower");
  ImGui::BulletText("0x11 - Death Mountain");
}

void MapPropertiesSystem::RefreshMapProperties() {
  // Implementation would refresh map properties
}

void MapPropertiesSystem::RefreshOverworldMap() {
  // Implementation would refresh the overworld map display
}

absl::Status MapPropertiesSystem::RefreshMapPalette() {
  // Implementation would refresh the map palette
  return absl::OkStatus();
}

void MapPropertiesSystem::DrawMosaicControls(int current_map) {
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version >= 2) {
    ImGui::Separator();
    ImGui::Text("Mosaic Effects (per direction):");

    auto* current_map_ptr = overworld_->mutable_overworld_map(current_map);
    std::array<bool, 4> mosaic_expanded = current_map_ptr->mosaic_expanded();
    const char* direction_names[] = {"North", "South", "East", "West"};

    for (int i = 0; i < 4; i++) {
      if (ImGui::Checkbox(direction_names[i], &mosaic_expanded[i])) {
        current_map_ptr->set_mosaic_expanded(i, mosaic_expanded[i]);
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }
  } else {
    if (ImGui::Checkbox(
            "Mosaic Effect",
            overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
  }
}

void MapPropertiesSystem::DrawOverlayControls(int current_map,
                                              bool& show_overlay_preview) {
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Determine if this is a special overworld map (0x80-0x9F)
  bool is_special_overworld_map = (current_map >= 0x80 && current_map < 0xA0);

  if (is_special_overworld_map) {
    // Special overworld maps (0x80-0x9F) do not support subscreen overlays
    ImGui::Text("Special overworld maps (0x80-0x9F) do not support");
    ImGui::Text("subscreen overlays (visual effects).");
    ImGui::Text("Map 0x%02X is a special overworld map", current_map);
  } else {
    // Light World (0x00-0x3F) and Dark World (0x40-0x7F) maps support subscreen overlays for all versions

    // Subscreen Overlay Section
    ImGui::Text("Subscreen Overlay (Visual Effects)");
    ImGui::SameLine();
    if (ImGui::Button("?")) {
      ImGui::OpenPopup("SubscreenOverlayHelp");
    }
    if (ImGui::BeginPopup("SubscreenOverlayHelp")) {
      ImGui::Text(
          "Subscreen overlays are visual effects like fog, rain, canopy,");
      ImGui::Text("and backgrounds that are displayed using tile16 graphics.");
      ImGui::Text(
          "They reference special area maps (0x80-0x9F) for their tile data.");
      ImGui::EndPopup();
    }

    uint16_t current_overlay =
        overworld_->mutable_overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord("Subscreen Overlay ID", &current_overlay,
                          kInputFieldSize + 20)) {
      overworld_->mutable_overworld_map(current_map)
          ->set_subscreen_overlay(current_overlay);
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT(
        "Subscreen overlay ID - visual effects like fog, rain, backgrounds");

    // Show subscreen overlay description
    std::string overlay_desc = GetOverlayDescription(current_overlay);
    ImGui::Text("Description: %s", overlay_desc.c_str());

    // Preview checkbox
    if (ImGui::Checkbox("Preview Subscreen Overlay on Map",
                        &show_overlay_preview)) {
      // Toggle subscreen overlay preview
    }
    HOVER_HINT("Show semi-transparent preview of subscreen overlay on the map");

    ImGui::Separator();

    // Interactive Overlay Section (for vanilla ROMs)
    if (asm_version == 0xFF) {
      ImGui::Text("Interactive Overlay (Holes/Changes)");
      ImGui::SameLine();
      if (ImGui::Button("?")) {
        ImGui::OpenPopup("InteractiveOverlayHelp");
      }
      if (ImGui::BeginPopup("InteractiveOverlayHelp")) {
        ImGui::Text(
            "Interactive overlays reveal holes or change elements on top");
        ImGui::Text("of the map. They use tile16 graphics and are present in");
        ImGui::Text(
            "vanilla ROMs. ZSCustomOverworld expands this functionality.");
        ImGui::EndPopup();
      }

      auto* current_map_ptr = overworld_->overworld_map(current_map);
      if (current_map_ptr->has_overlay()) {
        ImGui::Text("Interactive Overlay ID: 0x%04X",
                    current_map_ptr->overlay_id());
        ImGui::Text("Overlay Data Size: %d bytes",
                    static_cast<int>(current_map_ptr->overlay_data().size()));
      } else {
        ImGui::Text("No interactive overlay data for this map");
      }
      HOVER_HINT(
          "Interactive overlay for revealing holes/changing elements "
          "(read-only in vanilla)");
    }

    // Show version info
    if (asm_version == 0xFF) {
      ImGui::Text(
          "Vanilla ROM - subscreen overlays reference special area maps");
      ImGui::Text(
          "(0x80-0x9F) for visual effects like fog, rain, backgrounds.");
    } else {
      ImGui::Text("ZSCustomOverworld v%d", asm_version);
    }
  }
}

std::string MapPropertiesSystem::GetOverlayDescription(uint16_t overlay_id) {
  if (overlay_id == 0x0093) {
    return "Triforce Room Curtain";
  } else if (overlay_id == 0x0094) {
    return "Under the Bridge";
  } else if (overlay_id == 0x0095) {
    return "Sky Background (LW Death Mountain)";
  } else if (overlay_id == 0x0096) {
    return "Pyramid Background";
  } else if (overlay_id == 0x0097) {
    return "First Fog Overlay (Master Sword Area)";
  } else if (overlay_id == 0x009C) {
    return "Lava Background (DW Death Mountain)";
  } else if (overlay_id == 0x009D) {
    return "Second Fog Overlay (Lost Woods/Skull Woods)";
  } else if (overlay_id == 0x009E) {
    return "Tree Canopy (Forest)";
  } else if (overlay_id == 0x009F) {
    return "Rain Effect (Misery Mire)";
  } else if (overlay_id == 0x00FF) {
    return "No Overlay";
  } else {
    return "Custom overlay";
  }
}

void MapPropertiesSystem::DrawOverlayPreviewOnMap(int current_map,
                                                  int current_world,
                                                  bool show_overlay_preview) {
  gfx::ScopedTimer timer("map_properties_draw_overlay_preview");
  
  if (!show_overlay_preview || !maps_bmp_ || !canvas_)
    return;

  // Get subscreen overlay information based on ROM version and map type
  uint16_t overlay_id = 0x00FF;
  bool has_subscreen_overlay = false;

  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool is_special_overworld_map = (current_map >= 0x80 && current_map < 0xA0);

  if (is_special_overworld_map) {
    // Special overworld maps (0x80-0x9F) do not support subscreen overlays
    return;
  }

  // Light World (0x00-0x3F) and Dark World (0x40-0x7F) maps support subscreen overlays for all versions
  overlay_id = overworld_->overworld_map(current_map)->subscreen_overlay();
  has_subscreen_overlay = (overlay_id != 0x00FF);

  if (!has_subscreen_overlay)
    return;

  // Map subscreen overlay ID to special area map for bitmap
  int overlay_map_index = -1;
  if (overlay_id >= 0x80 && overlay_id < 0xA0) {
    overlay_map_index = overlay_id;
  }

  if (overlay_map_index < 0 || overlay_map_index >= zelda3::kNumOverworldMaps)
    return;

  // Get the subscreen overlay map's bitmap
  const auto& overlay_bitmap = (*maps_bmp_)[overlay_map_index];
  if (!overlay_bitmap.is_active())
    return;

  // Calculate position for subscreen overlay preview on the current map
  int current_map_x = current_map % 8;
  int current_map_y = current_map / 8;
  if (current_world == 1) {
    current_map_x = (current_map - 0x40) % 8;
    current_map_y = (current_map - 0x40) / 8;
  } else if (current_world == 2) {
    current_map_x = (current_map - 0x80) % 8;
    current_map_y = (current_map - 0x80) / 8;
  }

  int scale = static_cast<int>(canvas_->global_scale());
  int map_x = current_map_x * kOverworldMapSize * scale;
  int map_y = current_map_y * kOverworldMapSize * scale;

  // Determine if this is a background or foreground subscreen overlay
  bool is_background_overlay =
      (overlay_id == 0x0095 || overlay_id == 0x0096 || overlay_id == 0x009C);

  // Set alpha for semi-transparent preview
  ImU32 overlay_color =
      is_background_overlay ? IM_COL32(255, 255, 255, 128)
                            :  // Background subscreen overlays - lighter
          IM_COL32(255, 255, 255,
                   180);  // Foreground subscreen overlays - more opaque

  // Draw the subscreen overlay bitmap with semi-transparency
  canvas_->draw_list()->AddImage(
      (ImTextureID)(intptr_t)overlay_bitmap.texture(), ImVec2(map_x, map_y),
      ImVec2(map_x + kOverworldMapSize * scale,
             map_y + kOverworldMapSize * scale),
      ImVec2(0, 0), ImVec2(1, 1), overlay_color);
}

void MapPropertiesSystem::DrawViewPopup() {
  if (ImGui::BeginPopup("ViewPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text("View Controls");
    ImGui::Separator();

    // Horizontal layout for view controls
    if (ImGui::Button(ICON_MD_ZOOM_OUT, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the canvas zoom function
      // For now, just show the option
    }
    HOVER_HINT("Zoom out on the canvas");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ZOOM_IN, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the canvas zoom function
      // For now, just show the option
    }
    HOVER_HINT("Zoom in on the canvas");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_OPEN_IN_FULL, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the fullscreen toggle
      // For now, just show the option
    }
    HOVER_HINT("Toggle fullscreen canvas (F11)");

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawQuickAccessPopup() {
  if (ImGui::BeginPopup("QuickPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text("Quick Access");
    ImGui::Separator();

    // Horizontal layout for quick access buttons
    if (ImGui::Button(ICON_MD_GRID_VIEW, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the Tile16 editor toggle
      // For now, just show the option
    }
    HOVER_HINT("Open Tile16 Editor (Ctrl+T)");
    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_CONTENT_COPY, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the copy map function
      // For now, just show the option
    }
    HOVER_HINT("Copy current map to clipboard");
    ImGui::SameLine();

    if (ImGui::Button(ICON_MD_LOCK, ImVec2(kIconButtonWidth, 0))) {
      // This would need to be connected to the map lock toggle
      // For now, just show the option
    }
    HOVER_HINT("Lock/unlock current map (Ctrl+L)");

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace yaze

#include "app/editor/overworld/map_properties.h"

#include "app/gfx/performance/performance_profiler.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui_constants.h"
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
  (void)show_overlay_editor;  // Reserved for future use
  (void)current_mode;  // Reserved for future use
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
    // IMPORTANT: Don't cache - read fresh to reflect ROM upgrades
    uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    
    // ALL ROMs support Small/Large. Only v3+ supports Wide/Tall.
    int current_area_size =
        static_cast<int>(overworld_->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(kComboAreaSizeWidth);
    
    if (asm_version >= 3 && asm_version != 0xFF) {
      // v3+ ROM: Show all 4 area size options
      if (ImGui::Combo("##AreaSize", &current_area_size, kAreaSizeNames, 4)) {
        auto status = overworld_->ConfigureMultiAreaMap(
            current_map,
            static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
    } else {
      // Vanilla/v1/v2 ROM: Show only Small/Large (first 2 options)
      const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1) ? current_area_size : 0;
      
      if (ImGui::Combo("##AreaSize", &limited_size, limited_names, 2)) {
        // limited_size is 0 (Small) or 1 (Large)
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea 
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = overworld_->ConfigureMultiAreaMap(current_map, size);
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
      
      if (asm_version == 0xFF || asm_version < 3) {
        HOVER_HINT("Small (1x1) and Large (2x2) maps. Wide/Tall require v3+");
      }
    }

    TableNextColumn();
    if (ImGui::Button(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
                      ImVec2(40, 0))) {
      current_map_lock = !current_map_lock;
    }
    HOVER_HINT(current_map_lock ? "Unlock Map" : "Lock Map");

    TableNextColumn();
    if (ImGui::Button(ICON_MD_IMAGE " GFX", ImVec2(kTableButtonGraphics, 0))) {
      ImGui::OpenPopup("GraphicsPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Graphics Settings\n\n"
          "Configure:\n"
          "  • Area graphics (tileset)\n"
          "  • Sprite graphics sheets\n"
          "  • Animated graphics (v3+)\n"
          "  • Custom tile16 sheets (8 slots)");
    }
    DrawGraphicsPopup(current_map, game_state);

    TableNextColumn();
    if (ImGui::Button(ICON_MD_PALETTE " Palettes", ImVec2(kTableButtonPalettes, 0))) {
      ImGui::OpenPopup("PalettesPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Palette Settings\n\n"
          "Configure:\n"
          "  • Area palette (background colors)\n"
          "  • Main palette (v2+)\n"
          "  • Sprite palettes\n"
          "  • Custom background colors");
    }
    DrawPalettesPopup(current_map, game_state, show_custom_bg_color_editor);

    TableNextColumn();
    if (ImGui::Button(ICON_MD_TUNE " Config", ImVec2(kTableButtonProperties, 0))) {
      ImGui::OpenPopup("ConfigPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Area Configuration\n\n"
          "Quick access to:\n"
          "  • Message ID\n"
          "  • Game state settings\n"
          "  • Area size (v3+)\n"
          "  • Mosaic effects\n"
          "  • Visual effect overlays\n"
          "  • Map overlay info\n\n"
          "Click 'Full Configuration Panel' for\n"
          "comprehensive editing with all tabs.");
    }
    DrawPropertiesPopup(current_map, show_map_properties_panel,
                        show_overlay_preview, game_state);

    TableNextColumn();
    // View Controls
    if (ImGui::Button(ICON_MD_VISIBILITY " View", ImVec2(kTableButtonView, 0))) {
      ImGui::OpenPopup("ViewPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "View Controls\n\n"
          "Canvas controls:\n"
          "  • Zoom in/out\n"
          "  • Toggle fullscreen\n"
          "  • Reset view");
    }
    DrawViewPopup();

    TableNextColumn();
    // Quick Access Tools
    if (ImGui::Button(ICON_MD_BOLT " Quick", ImVec2(kTableButtonQuick, 0))) {
      ImGui::OpenPopup("QuickPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Quick Access Tools\n\n"
          "Shortcuts to:\n"
          "  • Tile16 editor (Ctrl+T)\n"
          "  • Copy current map\n"
          "  • Lock/unlock map (Ctrl+L)");
    }
    DrawQuickAccessPopup();

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawMapPropertiesPanel(
    int current_map, bool& show_map_properties_panel) {
  (void)show_map_properties_panel;  // Used by caller for window state
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
    uint8_t asm_version =
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
  (void)show_custom_bg_color_editor;  // Used by caller for window state
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  uint8_t asm_version =
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
  (void)show_overlay_editor;  // Used by caller for window state
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                     ICON_MD_LAYERS " Visual Effects Configuration");
  ImGui::Text("Map: 0x%02X", current_map);
  Separator();

  if (asm_version < 1) {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
                       ICON_MD_WARNING " Subscreen overlays require ZSCustomOverworld v1+");
    ImGui::Separator();
    ImGui::TextWrapped(
        "To use visual effect overlays, you need to upgrade your ROM to "
        "ZSCustomOverworld. This feature allows you to add atmospheric effects "
        "like fog, rain, forest canopy, and sky backgrounds to your maps.");
    return;
  }

  // Help section
  if (ImGui::CollapsingHeader(ICON_MD_HELP_OUTLINE " What are Visual Effects?",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent();
    ImGui::TextWrapped(
        "Visual effects (subscreen overlays) are semi-transparent layers drawn "
        "on top of or behind your map. They reference special area maps (0x80-0x9F) "
        "for their tile16 graphics data.");
    ImGui::Spacing();
    ImGui::Text("Common uses:");
    ImGui::BulletText("Fog effects (Lost Woods, Skull Woods)");
    ImGui::BulletText("Rain (Misery Mire)");
    ImGui::BulletText("Forest canopy (Lost Woods)");
    ImGui::BulletText("Sky backgrounds (Death Mountain)");
    ImGui::BulletText("Under bridge views");
    ImGui::Unindent();
    ImGui::Separator();
  }

  // Enable/disable subscreen overlay
  static bool use_subscreen_overlay = false;
  if (ImGui::Checkbox(ICON_MD_VISIBILITY " Enable Visual Effect for This Area", 
                      &use_subscreen_overlay)) {
    // Update ROM data
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
        use_subscreen_overlay ? 1 : 0;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enable/disable visual effect overlay for this map area");
  }

  if (use_subscreen_overlay) {
    ImGui::Spacing();
    uint16_t current_overlay =
        overworld_->overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord(ICON_MD_PHOTO " Visual Effect Map ID", &current_overlay,
                          kInputFieldSize + 30)) {
      overworld_->mutable_overworld_map(current_map)
          ->set_subscreen_overlay(current_overlay);

      // Update ROM
      int rom_address =
          zelda3::OverworldCustomSubscreenOverlayArray + (current_map * 2);
      (*rom_)[rom_address] = current_overlay & 0xFF;
      (*rom_)[rom_address + 1] = (current_overlay >> 8) & 0xFF;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "ID of the special area map (0x80-0x9F) to use for\n"
          "visual effects. That map's tile16 data will be drawn\n"
          "as a semi-transparent layer on this area.");
    }

    // Show description
    std::string overlay_desc = GetOverlayDescription(current_overlay);
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                       ICON_MD_INFO " %s", overlay_desc.c_str());

    ImGui::Separator();
    if (ImGui::CollapsingHeader(ICON_MD_LIGHTBULB " Common Visual Effect IDs")) {
      ImGui::Indent();
      ImGui::BulletText("0x0093 - Triforce Room Curtain");
      ImGui::BulletText("0x0094 - Under the Bridge");
      ImGui::BulletText("0x0095 - Sky Background (LW Death Mountain)");
      ImGui::BulletText("0x0096 - Pyramid Background");
      ImGui::BulletText("0x0097 - Fog Overlay (Master Sword Area)");
      ImGui::BulletText("0x009C - Lava Background (DW Death Mountain)");
      ImGui::BulletText("0x009D - Fog Overlay (Lost/Skull Woods)");
      ImGui::BulletText("0x009E - Tree Canopy (Forest)");
      ImGui::BulletText("0x009F - Rain Effect (Misery Mire)");
      ImGui::BulletText("0x00FF - No Overlay (Disabled)");
      ImGui::Unindent();
    }
  } else {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       ICON_MD_BLOCK " No visual effects enabled for this area");
  }
}

void MapPropertiesSystem::SetupCanvasContextMenu(
    gui::Canvas& canvas, int current_map, bool current_map_lock,
    bool& show_map_properties_panel, bool& show_custom_bg_color_editor,
    bool& show_overlay_editor) {
  (void)current_map;  // Used for future context-sensitive menu items
  // Clear any existing context menu items
  canvas.ClearContextMenuItems();

  // Add overworld-specific context menu items
  gui::Canvas::ContextMenuItem lock_item;
  lock_item.label = current_map_lock ? "Unlock Map" : "Lock to This Map";
  lock_item.callback = [&current_map_lock]() {
    current_map_lock = !current_map_lock;
  };
  canvas.AddContextMenuItem(lock_item);

  // Area Configuration
  gui::Canvas::ContextMenuItem properties_item;
  properties_item.label = ICON_MD_TUNE " Area Configuration";
  properties_item.callback = [&show_map_properties_panel]() {
    show_map_properties_panel = true;
  };
  canvas.AddContextMenuItem(properties_item);

  // Custom overworld features (only show if v3+)
  uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version >= 3 && asm_version != 0xFF) {
    // Custom Background Color
    gui::Canvas::ContextMenuItem bg_color_item;
    bg_color_item.label = ICON_MD_FORMAT_COLOR_FILL " Custom Background Color";
    bg_color_item.callback = [&show_custom_bg_color_editor]() {
      show_custom_bg_color_editor = true;
    };
    canvas.AddContextMenuItem(bg_color_item);

    // Visual Effects Editor
    gui::Canvas::ContextMenuItem overlay_item;
    overlay_item.label = ICON_MD_LAYERS " Visual Effects";
    overlay_item.callback = [&show_overlay_editor]() {
      show_overlay_editor = true;
    };
    canvas.AddContextMenuItem(overlay_item);
  }

  // Canvas controls
  gui::Canvas::ContextMenuItem reset_view_item;
  reset_view_item.label = ICON_MD_RESTORE " Reset View";
  reset_view_item.callback = [&canvas]() {
    canvas.set_global_scale(1.0f);
    canvas.set_scrolling(ImVec2(0, 0));
  };
  canvas.AddContextMenuItem(reset_view_item);

  gui::Canvas::ContextMenuItem zoom_in_item;
  zoom_in_item.label = ICON_MD_ZOOM_IN " Zoom In";
  zoom_in_item.callback = [&canvas]() {
    float scale = std::min(2.0f, canvas.global_scale() + 0.25f);
    canvas.set_global_scale(scale);
  };
  canvas.AddContextMenuItem(zoom_in_item);

  gui::Canvas::ContextMenuItem zoom_out_item;
  zoom_out_item.label = ICON_MD_ZOOM_OUT " Zoom Out";
  zoom_out_item.callback = [&canvas]() {
    float scale = std::max(0.25f, canvas.global_scale() - 0.25f);
    canvas.set_global_scale(scale);
  };
  canvas.AddContextMenuItem(zoom_out_item);
  
  // Entity Operations submenu will be added in future iteration
  // For now, users can use keyboard shortcuts (3-8) to activate entity editing
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

    // Area Graphics
    if (gui::InputHexByte(ICON_MD_IMAGE " Area Graphics",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_area_graphics(),
                                kHexByteInputWidth)) {
      // CORRECT ORDER: Properties first, then graphics reload
      
      // 1. Propagate properties to siblings FIRST (calls LoadAreaGraphics on siblings)
      RefreshMapProperties();
      
      // 2. Force immediate refresh of current map
      (*maps_bmp_)[current_map].set_modified(true);
      overworld_->mutable_overworld_map(current_map)->LoadAreaGraphics();
      
      // 3. Refresh siblings immediately
      RefreshSiblingMapGraphics(current_map);
      
      // 4. Update tile selector  
      RefreshTile16Blockset();
      
      // 5. Final refresh
      RefreshOverworldMap();
    }
    HOVER_HINT("Main tileset graphics for this map area");

    // Sprite Graphics
    if (gui::InputHexByte(
            absl::StrFormat(ICON_MD_PETS " Sprite GFX (%s)", kGameStateNames[game_state])
                .c_str(),
            overworld_->mutable_overworld_map(current_map)
                ->mutable_sprite_graphics(game_state),
            kHexByteInputWidth)) {
      ForceRefreshGraphics(current_map);
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Sprite graphics sheet for current game state");

    uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 3) {
      if (gui::InputHexByte(ICON_MD_ANIMATION " Animated GFX",
                                  overworld_->mutable_overworld_map(current_map)
                                      ->mutable_animated_gfx(),
                                  kHexByteInputWidth)) {
        ForceRefreshGraphics(current_map);
        RefreshMapProperties();
        RefreshTile16Blockset();
        RefreshOverworldMap();
      }
      HOVER_HINT("Animated tile graphics (water, lava, etc.)");
    }

    // Custom Tile Graphics - Only available for v1+ ROMs
    if (asm_version >= 1 && asm_version != 0xFF) {
      ImGui::Separator();
      ImGui::Text(ICON_MD_GRID_VIEW " Custom Tile Graphics");
      ImGui::Separator();

      // Show the 8 custom graphics IDs in a 2-column layout for density
      if (BeginTable("CustomTileGraphics", 2,
                     ImGuiTableFlags_SizingFixedFit)) {
        for (int i = 0; i < 8; i++) {
          TableNextColumn();
          std::string label = absl::StrFormat(ICON_MD_LAYERS " Sheet %d", i);
          if (gui::InputHexByte(label.c_str(),
                                      overworld_->mutable_overworld_map(current_map)
                                          ->mutable_custom_tileset(i),
                                      90.f)) {
            ForceRefreshGraphics(current_map);
            RefreshMapProperties();
            RefreshTile16Blockset();
            RefreshOverworldMap();
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Custom graphics sheet %d (0x00-0xFF)", i);
          }
        }
        ImGui::EndTable();
      }
    } else if (asm_version == 0xFF) {
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                         ICON_MD_INFO " Custom Tile Graphics");
      ImGui::TextWrapped(
          "Custom tile graphics require ZSCustomOverworld v1+.\n"
          "Upgrade your ROM to access 8 customizable graphics sheets.");
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

    // Area Palette
    if (gui::InputHexByte(ICON_MD_PALETTE " Area Palette",
                                overworld_->mutable_overworld_map(current_map)
                                    ->mutable_area_palette(),
                                kHexByteInputWidth)) {
      RefreshMapProperties();
      auto status = RefreshMapPalette();
      RefreshOverworldMap();
    }
    HOVER_HINT("Main color palette for background tiles");

    // Read fresh to reflect ROM upgrades
    uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 2) {
      if (gui::InputHexByte(ICON_MD_COLOR_LENS " Main Palette",
                                  overworld_->mutable_overworld_map(current_map)
                                      ->mutable_main_palette(),
                                  kHexByteInputWidth)) {
        RefreshMapProperties();
        auto status = RefreshMapPalette();
        RefreshOverworldMap();
      }
      HOVER_HINT("Extended main palette (ZSCustomOverworld v2+)");
    }

    // Sprite Palette
    if (gui::InputHexByte(
            absl::StrFormat(ICON_MD_COLORIZE " Sprite Pal (%s)", kGameStateNames[game_state])
                .c_str(),
            overworld_->mutable_overworld_map(current_map)
                ->mutable_sprite_palette(game_state),
            kHexByteInputWidth)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Color palette for sprites in current game state");

    ImGui::Separator();
    if (ImGui::Button(ICON_MD_FORMAT_COLOR_FILL " Custom Background Color", 
                     ImVec2(-1, 0))) {
      show_custom_bg_color_editor = !show_custom_bg_color_editor;
    }
    HOVER_HINT("Open custom background color editor (v2+)");

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawPropertiesPopup(int current_map,
                                              bool& show_map_properties_panel,
                                              bool& show_overlay_preview,
                                              int& game_state) {
  if (ImGui::BeginPopup("ConfigPopup")) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kCompactItemSpacing, kCompactFramePadding));

    ImGui::Text(ICON_MD_TUNE " Area Configuration");
    ImGui::Separator();

    // Basic Properties in 2-column layout for density
    if (BeginTable("BasicProps", 2, ImGuiTableFlags_SizingFixedFit)) {
      // Message ID
      TableNextColumn();
      ImGui::Text(ICON_MD_MESSAGE " Message");
      TableNextColumn();
      if (gui::InputHexWordCustom("##MsgId",
                                  overworld_->mutable_overworld_map(current_map)
                                      ->mutable_message_id(),
                                  kHexWordInputWidth)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Message ID shown when entering this area");
      }

      // Game State
      TableNextColumn();
      ImGui::Text(ICON_MD_GAMEPAD " Game State");
      TableNextColumn();
      ImGui::SetNextItemWidth(kComboGameStateWidth);
      if (ImGui::Combo("##GameState", &game_state, kGameStateNames, 3)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Affects sprite graphics/palettes based on story progress");
      }

      ImGui::EndTable();
    }

    // Area Configuration Section
    ImGui::Separator();
    ImGui::Text(ICON_MD_ASPECT_RATIO " Area Configuration");
    ImGui::Separator();

    // ALL ROMs support Small/Large. Only v3+ supports Wide/Tall.
    uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    
    int current_area_size =
        static_cast<int>(overworld_->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(kComboAreaSizeWidth);
    
    if (asm_version >= 3 && asm_version != 0xFF) {
      // v3+ ROM: Show all 4 area size options
      if (ImGui::Combo(ICON_MD_PHOTO_SIZE_SELECT_LARGE " Size", &current_area_size, kAreaSizeNames, 4)) {
        auto status = overworld_->ConfigureMultiAreaMap(
            current_map,
            static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
      HOVER_HINT("Map area size (1x1, 2x2, 2x1, 1x2 screens)");
    } else {
      // Vanilla/v1/v2 ROM: Show only Small/Large
      const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1) ? current_area_size : 0;
      
      if (ImGui::Combo(ICON_MD_PHOTO_SIZE_SELECT_LARGE " Size", &limited_size, limited_names, 2)) {
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea 
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = overworld_->ConfigureMultiAreaMap(current_map, size);
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
      HOVER_HINT("Small (1x1) and Large (2x2) maps. Wide/Tall require v3+");
    }

    // Visual Effects Section
    ImGui::Separator();
    ImGui::Text(ICON_MD_AUTO_FIX_HIGH " Visual Effects");
    ImGui::Separator();

    DrawMosaicControls(current_map);
    DrawOverlayControls(current_map, show_overlay_preview);

    // Advanced Options Section
    ImGui::Separator();
    if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Full Configuration Panel",
                      ImVec2(-1, 0))) {
      show_map_properties_panel = true;
      ImGui::CloseCurrentPopup();
    }
    HOVER_HINT("Open detailed area configuration with all settings tabs");

    ImGui::PopStyleVar(2);  // Pop the 2 style variables we pushed
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawBasicPropertiesTab(int current_map) {
  if (BeginTable("BasicProperties", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text(ICON_MD_IMAGE " Area Graphics");
    TableNextColumn();
    if (gui::InputHexByte("##AreaGfx",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_graphics(),
                          kInputFieldSize)) {
      // CORRECT ORDER: Properties first, then graphics reload
      RefreshMapProperties();
      (*maps_bmp_)[current_map].set_modified(true);
      overworld_->mutable_overworld_map(current_map)->LoadAreaGraphics();
      RefreshSiblingMapGraphics(current_map);
      RefreshTile16Blockset();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Main tileset graphics for this map area");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_PALETTE " Area Palette");
    TableNextColumn();
    if (gui::InputHexByte("##AreaPal",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      auto status = RefreshMapPalette();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color palette for background tiles");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MESSAGE " Message ID");
    TableNextColumn();
    if (gui::InputHexWord("##MsgId",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_message_id(),
                          kInputFieldSize + 20)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Message displayed when entering this area");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_BLUR_ON " Mosaic Effect");
    TableNextColumn();
    if (ImGui::Checkbox(
            "##mosaic",
            overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable pixelated mosaic transition effect");
    }

    // Add music editing controls with icons
    TableNextColumn();
    ImGui::Text(ICON_MD_MUSIC_NOTE " Music (Beginning)");
    TableNextColumn();
    if (gui::InputHexByte("##Music0",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(0),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Music track before rescuing Zelda");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MUSIC_NOTE " Music (Zelda)");
    TableNextColumn();
    if (gui::InputHexByte("##Music1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Music track after rescuing Zelda");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MUSIC_NOTE " Music (Master Sword)");
    TableNextColumn();
    if (gui::InputHexByte("##Music2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Music track after obtaining Master Sword");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_MUSIC_NOTE " Music (Agahnim)");
    TableNextColumn();
    if (gui::InputHexByte("##Music3",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_area_music(3),
                          kInputFieldSize)) {
      RefreshMapProperties();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Music track after defeating Agahnim (Dark World)");
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawSpritePropertiesTab(int current_map) {
  if (BeginTable("SpriteProperties", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text(ICON_MD_GAMEPAD " Game State");
    TableNextColumn();
    static int game_state = 0;
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("##GameState", &game_state, kGameStateNames, 3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Affects which sprite graphics/palettes are used");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_PETS " Sprite Graphics 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_graphics(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("First sprite graphics sheet for Zelda rescued state");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_PETS " Sprite Graphics 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_graphics(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Second sprite graphics sheet for Master Sword obtained state");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_COLORIZE " Sprite Palette 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal1",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_palette(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color palette for sprites - Zelda rescued state");
    }

    TableNextColumn();
    ImGui::Text(ICON_MD_COLORIZE " Sprite Palette 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal2",
                          overworld_->mutable_overworld_map(current_map)
                              ->mutable_sprite_palette(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Color palette for sprites - Master Sword obtained state");
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawCustomFeaturesTab(int current_map) {
  if (BeginTable("CustomFeatures", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    TableNextColumn();
    ImGui::Text(ICON_MD_PHOTO_SIZE_SELECT_LARGE " Area Size");
    TableNextColumn();
    // ALL ROMs support Small/Large. Only v3+ supports Wide/Tall.
    uint8_t asm_version =
        (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    
    int current_area_size =
        static_cast<int>(overworld_->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(130.f);
    
    if (asm_version >= 3 && asm_version != 0xFF) {
      // v3+ ROM: Show all 4 area size options
      static const char* all_sizes[] = {"Small (1x1)", "Large (2x2)",
                                        "Wide (2x1)", "Tall (1x2)"};
      if (ImGui::Combo("##AreaSize", &current_area_size, all_sizes, 4)) {
        auto status = overworld_->ConfigureMultiAreaMap(
            current_map,
            static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Map size: Small (1x1), Large (2x2), Wide (2x1), Tall (1x2)");
      }
    } else {
      // Vanilla/v1/v2 ROM: Show only Small/Large
      static const char* limited_sizes[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1) ? current_area_size : 0;
      
      if (ImGui::Combo("##AreaSize", &limited_size, limited_sizes, 2)) {
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea 
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = overworld_->ConfigureMultiAreaMap(current_map, size);
        if (status.ok()) {
          RefreshSiblingMapGraphics(current_map, true);
          RefreshOverworldMap();
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Map size: Small (1x1), Large (2x2). Wide/Tall require v3+");
      }
    }

    if (asm_version >= 2) {
      TableNextColumn();
      ImGui::Text(ICON_MD_COLOR_LENS " Main Palette");
      TableNextColumn();
      if (gui::InputHexByte("##MainPal",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        auto status = RefreshMapPalette();
        RefreshOverworldMap();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Extended main palette (ZSCustomOverworld v2+)");
      }
    }

    if (asm_version >= 3) {
      TableNextColumn();
      ImGui::Text(ICON_MD_ANIMATION " Animated GFX");
      TableNextColumn();
      if (gui::InputHexByte("##AnimGfx",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Animated tile graphics ID (water, lava, etc.)");
      }

      TableNextColumn();
      ImGui::Text(ICON_MD_LAYERS " Subscreen Overlay");
      TableNextColumn();
      if (gui::InputHexWord("##SubOverlay",
                            overworld_->mutable_overworld_map(current_map)
                                ->mutable_subscreen_overlay(),
                            kInputFieldSize + 20)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Visual effects overlay ID (fog, rain, backgrounds)");
      }
    }

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawTileGraphicsTab(int current_map) {
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  
  // Only show custom tile graphics for v1+ ROMs
  if (asm_version >= 1 && asm_version != 0xFF) {
    ImGui::Text(ICON_MD_GRID_VIEW " Custom Tile Graphics (8 sheets)");
    Separator();

    if (BeginTable("TileGraphics", 2,
                   ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

      for (int i = 0; i < 8; i++) {
        TableNextColumn();
        ImGui::Text(ICON_MD_LAYERS " Sheet %d", i);
        TableNextColumn();
        if (gui::InputHexByte(absl::StrFormat("##TileGfx%d", i).c_str(),
                              overworld_->mutable_overworld_map(current_map)
                                  ->mutable_custom_tileset(i),
                              kInputFieldSize)) {
          overworld_->mutable_overworld_map(current_map)->LoadAreaGraphics();
          ForceRefreshGraphics(current_map);
          RefreshSiblingMapGraphics(current_map);
          RefreshMapProperties();
          RefreshTile16Blockset();
          RefreshOverworldMap();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Custom graphics sheet %d (0x00-0xFF)", i);
        }
      }

      ImGui::EndTable();
    }
    
    Separator();
    ImGui::TextWrapped("These 8 sheets allow custom tile graphics per map. "
                       "Each sheet references a graphics ID loaded into VRAM.");
  } else {
    // Vanilla ROM - show info message
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       ICON_MD_INFO " Custom Tile Graphics");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Custom tile graphics are not available in vanilla ROMs.\n\n"
        "To enable this feature, upgrade your ROM to ZSCustomOverworld v1+, "
        "which provides 8 customizable graphics sheets per map for advanced "
        "tileset customization.");
  }
}

void MapPropertiesSystem::DrawMusicTab(int current_map) {
  ImGui::Text(ICON_MD_MUSIC_NOTE " Music Settings for Game States");
  Separator();

  if (BeginTable("MusicSettings", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Game State", ImGuiTableColumnFlags_WidthFixed,
                            220);
    ImGui::TableSetupColumn("Music Track ID",
                            ImGuiTableColumnFlags_WidthStretch);

    const char* music_state_names[] = {
        ICON_MD_PLAY_ARROW " Beginning (Pre-Zelda)", 
        ICON_MD_FAVORITE " Zelda Rescued",
        ICON_MD_OFFLINE_BOLT " Master Sword Obtained",
        ICON_MD_CASTLE " Agahnim Defeated"};

    const char* music_descriptions[] = {
        "Music before rescuing Zelda from the castle",
        "Music after rescuing Zelda from Hyrule Castle",
        "Music after obtaining the Master Sword from the Lost Woods",
        "Music after defeating Agahnim (Dark World music)"};

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
  ImGui::TextWrapped("Music tracks control the background music for different "
                     "game progression states on this overworld map.");

  // Show common music track IDs for reference in a collapsing section
  Separator();
  if (ImGui::CollapsingHeader(ICON_MD_HELP_OUTLINE " Common Music Track IDs", 
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent();
    ImGui::BulletText("0x02 - Overworld Theme");
    ImGui::BulletText("0x05 - Kakariko Village");
    ImGui::BulletText("0x07 - Lost Woods");
    ImGui::BulletText("0x09 - Dark World Theme");
    ImGui::BulletText("0x0F - Ganon's Tower");
    ImGui::BulletText("0x11 - Death Mountain");
    ImGui::Unindent();
  }
}

void MapPropertiesSystem::RefreshMapProperties() {
  if (refresh_map_properties_) {
    refresh_map_properties_();
  }
}

void MapPropertiesSystem::RefreshOverworldMap() {
  if (refresh_overworld_map_) {
    refresh_overworld_map_();
  }
}

absl::Status MapPropertiesSystem::RefreshMapPalette() {
  if (refresh_map_palette_) {
    return refresh_map_palette_();
  }
  return absl::OkStatus();
}

absl::Status MapPropertiesSystem::RefreshTile16Blockset() {
  if (refresh_tile16_blockset_) {
    return refresh_tile16_blockset_();
  }
  return absl::OkStatus();
}

void MapPropertiesSystem::ForceRefreshGraphics(int map_index) {
  if (force_refresh_graphics_) {
    force_refresh_graphics_(map_index);
  }
}

void MapPropertiesSystem::RefreshSiblingMapGraphics(int map_index, bool include_self) {
  if (!overworld_ || !maps_bmp_ || map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }
  
  auto* map = overworld_->mutable_overworld_map(map_index);
  if (map->area_size() == zelda3::AreaSizeEnum::SmallArea) {
    return;  // No siblings for small areas
  }
  
  int parent_id = map->parent();
  std::vector<int> siblings;
  
  switch (map->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      siblings = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;
    case zelda3::AreaSizeEnum::WideArea:
      siblings = {parent_id, parent_id + 1};
      break;
    case zelda3::AreaSizeEnum::TallArea:
      siblings = {parent_id, parent_id + 8};
      break;
    default:
      return;
  }
  
  for (int sibling : siblings) {
    if (sibling >= 0 && sibling < zelda3::kNumOverworldMaps) {
      // Skip self unless include_self is true
      if (sibling == map_index && !include_self) {
        continue;
      }
      
      // Mark as modified FIRST
      (*maps_bmp_)[sibling].set_modified(true);
      
      // Load graphics from ROM
      overworld_->mutable_overworld_map(sibling)->LoadAreaGraphics();
      
      // CRITICAL FIX: Force immediate refresh on the sibling
      // This will trigger the callback to OverworldEditor's RefreshChildMapOnDemand
      ForceRefreshGraphics(sibling);
    }
  }
  
  // After marking all siblings, trigger a refresh
  // This ensures all marked maps get processed
  RefreshOverworldMap();
}

void MapPropertiesSystem::DrawMosaicControls(int current_map) {
  uint8_t asm_version =
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
  uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Determine if this is a special overworld map (0x80-0x9F)
  bool is_special_overworld_map = (current_map >= 0x80 && current_map < 0xA0);

  if (is_special_overworld_map) {
    // Special overworld maps (0x80-0x9F) serve as visual effect sources
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), 
                       ICON_MD_INFO " Special Area Map (0x%02X)", current_map);
    ImGui::Separator();
    ImGui::TextWrapped(
        "This is a special area map (0x80-0x9F) used as a visual effect "
        "source. These maps provide the graphics data for subscreen overlays "
        "like fog, rain, forest canopy, and sky backgrounds that appear on "
        "normal maps (0x00-0x7F).");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "You can edit the tile16 data here to customize how the visual effects "
        "appear when referenced by other maps.");
  } else {
    // Light World (0x00-0x3F) and Dark World (0x40-0x7F) maps support subscreen overlays

    // Comprehensive help section
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), 
                       ICON_MD_HELP_OUTLINE " Visual Effects Overview");
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_INFO "##HelpButton")) {
      ImGui::OpenPopup("OverlayTypesHelp");
    }
    
    if (ImGui::BeginPopup("OverlayTypesHelp")) {
      ImGui::Text(ICON_MD_HELP " Understanding Overlay Types");
      ImGui::Separator();
      
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                         ICON_MD_LAYERS " 1. Subscreen Overlays (Visual Effects)");
      ImGui::Indent();
      ImGui::BulletText("Displayed as semi-transparent layers");
      ImGui::BulletText("Reference special area maps (0x80-0x9F)");
      ImGui::BulletText("Examples: fog, rain, forest canopy, sky");
      ImGui::BulletText("Purely visual - don't affect collision");
      ImGui::Unindent();
      
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), 
                         ICON_MD_EDIT_NOTE " 2. Map Overlays (Interactive)");
      ImGui::Indent();
      ImGui::BulletText("Dynamic tile16 changes on the map");
      ImGui::BulletText("Used for bridges appearing, holes opening");
      ImGui::BulletText("Stored as tile16 ID arrays");
      ImGui::BulletText("Affect collision and interaction");
      ImGui::BulletText("Triggered by game events/progression");
      ImGui::Unindent();
      
      ImGui::Separator();
      ImGui::TextWrapped(
          "Note: Subscreen overlays are what you configure here. "
          "Map overlays are event-driven and edited separately.");
      
      ImGui::EndPopup();
    }
    ImGui::Separator();

    // Subscreen Overlay Section
    ImGui::Text(ICON_MD_LAYERS " Subscreen Overlay (Visual Effects)");

    uint16_t current_overlay =
        overworld_->mutable_overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord("Visual Effect Map ID", &current_overlay,
                          kInputFieldSize + 20)) {
      overworld_->mutable_overworld_map(current_map)
          ->set_subscreen_overlay(current_overlay);
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "References a special area map (0x80-0x9F) for visual effects.\n"
          "The referenced map's tile16 data is drawn as a semi-transparent\n"
          "layer on top of or behind this area for atmospheric effects.");
    }

    // Show subscreen overlay description with color coding
    std::string overlay_desc = GetOverlayDescription(current_overlay);
    if (current_overlay == 0x00FF) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                         ICON_MD_CHECK " %s", overlay_desc.c_str());
    } else if (current_overlay >= 0x80 && current_overlay < 0xA0) {
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                         ICON_MD_VISIBILITY " %s", overlay_desc.c_str());
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), 
                         ICON_MD_HELP_OUTLINE " %s", overlay_desc.c_str());
    }

    // Preview checkbox with better labeling
    ImGui::Spacing();
    if (ImGui::Checkbox(ICON_MD_PREVIEW " Preview Visual Effect on Map",
                        &show_overlay_preview)) {
      // Toggle subscreen overlay preview
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Shows a semi-transparent preview of the visual effect overlay\n"
          "drawn on top of the current map in the editor canvas.\n\n"
          "This preview shows how the subscreen overlay will appear in-game.");
    }

    ImGui::Separator();

    // Interactive/Dynamic Map Overlay Section (for vanilla ROMs)
    if (asm_version == 0xFF) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                         ICON_MD_EDIT_NOTE " Map Overlay (Interactive)");
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_INFO "##MapOverlayHelp")) {
        ImGui::OpenPopup("InteractiveOverlayHelp");
      }
      if (ImGui::BeginPopup("InteractiveOverlayHelp")) {
        ImGui::Text(ICON_MD_HELP " Map Overlays (Interactive Tile Changes)");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Map overlays are different from visual effect overlays. "
            "They contain tile16 data that dynamically replaces tiles on "
            "the map based on game events or progression.");
        ImGui::Spacing();
        ImGui::Text("Common uses:");
        ImGui::BulletText("Bridges appearing over water");
        ImGui::BulletText("Holes revealing secret passages");
        ImGui::BulletText("Rocks/bushes being moved");
        ImGui::BulletText("Environmental changes from story events");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "These are triggered by game code and stored as separate "
            "tile data arrays in the ROM. ZSCustomOverworld v3+ provides "
            "extended control over these features.");
        ImGui::EndPopup();
      }

      auto* current_map_ptr = overworld_->overworld_map(current_map);
      ImGui::Spacing();
      if (current_map_ptr->has_overlay()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                           ICON_MD_CHECK " Overlay ID: 0x%04X",
                           current_map_ptr->overlay_id());
        ImGui::Text(ICON_MD_STORAGE " Data Size: %d bytes",
                    static_cast<int>(current_map_ptr->overlay_data().size()));
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f),
                           ICON_MD_INFO " Read-only in vanilla ROM");
      } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           ICON_MD_BLOCK " No map overlay data for this area");
      }
    }

    // Show version and capability info
    ImGui::Separator();
    if (asm_version == 0xFF) {
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                         ICON_MD_INFO " Vanilla ROM");
      ImGui::BulletText("Visual effects use maps 0x80-0x9F");
      ImGui::BulletText("Map overlays are read-only");
    } else {
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                         ICON_MD_UPGRADE " ZSCustomOverworld v%d", asm_version);
      ImGui::BulletText("Enhanced visual effect control");
      if (asm_version >= 3) {
        ImGui::BulletText("Extended overlay system");
        ImGui::BulletText("Custom area sizes support");
      }
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

  uint8_t asm_version =
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

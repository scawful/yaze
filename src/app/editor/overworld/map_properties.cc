#include "app/editor/overworld/map_properties.h"

#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginTable;
using ImGui::Button;
using ImGui::Checkbox;
using ImGui::EndTable;
// HOVER_HINT is defined in util/macro.h
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

// Static constants
constexpr const char* kWorldList[] = {"Light World", "Dark World", "Special World"};
constexpr const char* kGamePartComboString[] = {"Light World", "Dark World", "Special World"};

void MapPropertiesSystem::DrawSimplifiedMapSettings(int& current_world, int& current_map, 
                                                   bool& current_map_lock, bool& show_map_properties_panel,
                                                   bool& show_custom_bg_color_editor, bool& show_overlay_editor) {
  // Enhanced settings table with popup buttons for quick access
  if (BeginTable("SimplifiedMapSettings", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit, ImVec2(0, 0), -1)) {
    ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Area Size", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Graphics", ImGuiTableColumnFlags_WidthFixed, 70);
    ImGui::TableSetupColumn("Palettes", ImGuiTableColumnFlags_WidthFixed, 70);
    ImGui::TableSetupColumn("Overlays", ImGuiTableColumnFlags_WidthFixed, 70);
    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed, 80);

    TableNextColumn();
    ImGui::SetNextItemWidth(70.f);
    if (ImGui::Combo("##world", &current_world, kWorldList, 3)) {
      // World changed, update current map if needed
      if (current_map >= 0x40 && current_world == 0) {
        current_map -= 0x40;
      } else if (current_map < 0x40 && current_world == 1) {
        current_map += 0x40;
      } else if (current_map < 0x80 && current_world == 2) {
        current_map += 0x80;
      } else if (current_map >= 0x80 && current_world != 2) {
        current_map -= 0x80;
      }
    }

    TableNextColumn();
    ImGui::Text("%d (0x%02X)", current_map, current_map);

    TableNextColumn();
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF) {
      static const char *area_size_names[] = {"Small (1x1)", "Large (2x2)", "Wide (2x1)", "Tall (1x2)"};
      int current_area_size = static_cast<int>(overworld_->overworld_map(current_map)->area_size());
      ImGui::SetNextItemWidth(90.f);
      if (ImGui::Combo("##AreaSize", &current_area_size, area_size_names, 4)) {
        overworld_->mutable_overworld_map(current_map)->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
        RefreshOverworldMap();
      }
    } else {
      ImGui::Text("N/A");
    }

    TableNextColumn();
    if (ImGui::Button(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN, ImVec2(30, 0))) {
      current_map_lock = !current_map_lock;
    }
    HOVER_HINT(current_map_lock ? "Unlock Map" : "Lock Map");

    TableNextColumn();
    if (ImGui::Button("Graphics", ImVec2(60, 0))) {
      ImGui::OpenPopup("GraphicsPopup");
    }
    HOVER_HINT("Graphics Settings");
    DrawGraphicsPopup(current_map);

    TableNextColumn();
    if (ImGui::Button("Palettes", ImVec2(60, 0))) {
      ImGui::OpenPopup("PalettesPopup");
    }
    HOVER_HINT("Palette Settings");
    DrawPalettesPopup(current_map, show_custom_bg_color_editor);

    TableNextColumn();
    if (ImGui::Button("Overlays", ImVec2(60, 0))) {
      ImGui::OpenPopup("OverlaysPopup");
    }
    HOVER_HINT("Overlay Settings");
    DrawOverlaysPopup(current_map, show_overlay_editor);

    TableNextColumn();
    if (ImGui::Button("Properties", ImVec2(70, 0))) {
      ImGui::OpenPopup("PropertiesPopup");
    }
    HOVER_HINT("Map Properties");
    DrawPropertiesPopup(current_map, show_map_properties_panel);

    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawMapPropertiesPanel(int current_map, bool& show_map_properties_panel) {
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
  if (ImGui::BeginTabBar("MapPropertiesTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
    
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
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF && ImGui::BeginTabItem("Custom Features")) {
      DrawCustomFeaturesTab(current_map);
      ImGui::EndTabItem();
    }
    
    // Tile Graphics Tab
    if (ImGui::BeginTabItem("Tile Graphics")) {
      DrawTileGraphicsTab(current_map);
      ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
  }
}

void MapPropertiesSystem::DrawCustomBackgroundColorEditor(int current_map, bool& show_custom_bg_color_editor) {
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version < 2) {
    Text("Custom background colors require ZSCustomOverworld v2+");
    return;
  }

  Text("Custom Background Color Editor");
  Separator();

  // Enable/disable area-specific background color
  static bool use_area_specific_bg_color = false;
  if (ImGui::Checkbox("Use Area-Specific Background Color", &use_area_specific_bg_color)) {
    // Update ROM data
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] = use_area_specific_bg_color ? 1 : 0;
  }

  if (use_area_specific_bg_color) {
    // Get current color
    uint16_t current_color = overworld_->overworld_map(current_map)->area_specific_bg_color();
    gfx::SnesColor snes_color(current_color);
    
    // Convert to ImVec4 for color picker
    ImVec4 color_vec = gui::ConvertSnesColorToImVec4(snes_color);
    
    if (ImGui::ColorPicker4("Background Color", (float*)&color_vec, 
                           ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex)) {
      // Convert back to SNES color and update
      gfx::SnesColor new_snes_color = gui::ConvertImVec4ToSnesColor(color_vec);
      overworld_->mutable_overworld_map(current_map)->set_area_specific_bg_color(new_snes_color.snes());
      
      // Update ROM
      int rom_address = zelda3::OverworldCustomAreaSpecificBGPalette + (current_map * 2);
      (*rom_)[rom_address] = new_snes_color.snes() & 0xFF;
      (*rom_)[rom_address + 1] = (new_snes_color.snes() >> 8) & 0xFF;
    }
    
    Text("SNES Color: 0x%04X", current_color);
  }
}

void MapPropertiesSystem::DrawOverlayEditor(int current_map, bool& show_overlay_editor) {
  if (!overworld_->is_loaded()) {
    Text("No overworld loaded");
    return;
  }

  static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
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
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] = use_subscreen_overlay ? 1 : 0;
  }

  if (use_subscreen_overlay) {
    uint16_t current_overlay = overworld_->overworld_map(current_map)->subscreen_overlay();
    if (gui::InputHexWord("Overlay ID", &current_overlay, kInputFieldSize + 20)) {
      overworld_->mutable_overworld_map(current_map)->set_subscreen_overlay(current_overlay);
      
      // Update ROM
      int rom_address = zelda3::OverworldCustomSubscreenOverlayArray + (current_map * 2);
      (*rom_)[rom_address] = current_overlay & 0xFF;
      (*rom_)[rom_address + 1] = (current_overlay >> 8) & 0xFF;
    }
    
    Text("Common overlay IDs:");
    Text("0x0000 = None");
    Text("0x0001 = Map overlay");
    Text("0x0002 = Dungeon overlay");
  }
}

void MapPropertiesSystem::SetupCanvasContextMenu(gui::Canvas& canvas, int current_map, bool current_map_lock,
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
  static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
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
void MapPropertiesSystem::DrawGraphicsPopup(int current_map) {
  if (ImGui::BeginPopup("GraphicsPopup")) {
    ImGui::Text("Graphics Settings");
    ImGui::Separator();
    
    if (gui::InputHexByte("Area Graphics", 
                          overworld_->mutable_overworld_map(current_map)->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    if (gui::InputHexByte("Sprite GFX 1", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_graphics(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    if (gui::InputHexByte("Sprite GFX 2", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_graphics(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 3) {
      if (gui::InputHexByte("Animated GFX", 
                            overworld_->mutable_overworld_map(current_map)->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }
    
    ImGui::Separator();
    if (ImGui::Button("Tile Graphics (8 sheets)")) {
      // This would open the full properties panel
    }
    
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawPalettesPopup(int current_map, bool& show_custom_bg_color_editor) {
  if (ImGui::BeginPopup("PalettesPopup")) {
    ImGui::Text("Palette Settings");
    ImGui::Separator();
    
    if (gui::InputHexByte("Area Palette", 
                          overworld_->mutable_overworld_map(current_map)->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshMapPalette();
      RefreshOverworldMap();
    }
    
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 2) {
      if (gui::InputHexByte("Main Palette", 
                            overworld_->mutable_overworld_map(current_map)->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshMapPalette();
        RefreshOverworldMap();
      }
    }
    
    if (gui::InputHexByte("Sprite Palette 1", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_palette(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    if (gui::InputHexByte("Sprite Palette 2", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_palette(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    ImGui::Separator();
    if (ImGui::Button("Background Color")) {
      show_custom_bg_color_editor = !show_custom_bg_color_editor;
    }
    
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawOverlaysPopup(int current_map, bool& show_overlay_editor) {
  if (ImGui::BeginPopup("OverlaysPopup")) {
    ImGui::Text("Overlay Settings");
    ImGui::Separator();
    
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 3) {
      if (gui::InputHexWord("Subscreen Overlay", 
                            overworld_->mutable_overworld_map(current_map)->mutable_subscreen_overlay(),
                            kInputFieldSize + 20)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      
      ImGui::Separator();
      if (ImGui::Button("Overlay Settings")) {
        show_overlay_editor = !show_overlay_editor;
      }
    } else {
      ImGui::Text("Overlays require ZSCustomOverworld v3+");
    }
    
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawPropertiesPopup(int current_map, bool& show_map_properties_panel) {
  if (ImGui::BeginPopup("PropertiesPopup")) {
    ImGui::Text("Map Properties");
    ImGui::Separator();
    
    if (gui::InputHexWord("Message ID", 
                          overworld_->mutable_overworld_map(current_map)->mutable_message_id(),
                          kInputFieldSize + 20)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    if (ImGui::Checkbox("Mosaic Effect", 
                        overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    static int game_state = 0; // This should be passed in or stored
    ImGui::SetNextItemWidth(100.f);
    if (ImGui::Combo("Game State", &game_state, kGamePartComboString, 3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version != 0xFF) {
      static const char *area_size_names[] = {"Small (1x1)", "Large (2x2)", "Wide (2x1)", "Tall (1x2)"};
      int current_area_size = static_cast<int>(overworld_->overworld_map(current_map)->area_size());
      ImGui::SetNextItemWidth(120.f);
      if (ImGui::Combo("Area Size", &current_area_size, area_size_names, 4)) {
        overworld_->mutable_overworld_map(current_map)->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
        RefreshOverworldMap();
      }
    }
    
    ImGui::Separator();
    if (ImGui::Button("Full Properties Panel")) {
      show_map_properties_panel = true;
    }
    
    ImGui::EndPopup();
  }
}

void MapPropertiesSystem::DrawBasicPropertiesTab(int current_map) {
  if (BeginTable("BasicProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    
    TableNextColumn(); ImGui::Text("Area Graphics");
    TableNextColumn();
    if (gui::InputHexByte("##AreaGfx", 
                          overworld_->mutable_overworld_map(current_map)->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Area Palette");
    TableNextColumn();
    if (gui::InputHexByte("##AreaPal", 
                          overworld_->mutable_overworld_map(current_map)->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshMapPalette();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Message ID");
    TableNextColumn();
    if (gui::InputHexWord("##MsgId", 
                          overworld_->mutable_overworld_map(current_map)->mutable_message_id(),
                          kInputFieldSize + 20)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Mosaic Effect");
    TableNextColumn();
    if (ImGui::Checkbox("##mosaic", 
                        overworld_->mutable_overworld_map(current_map)->mutable_mosaic())) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    HOVER_HINT("Enable Mosaic effect for the current map");
    
    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawSpritePropertiesTab(int current_map) {
  if (BeginTable("SpriteProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    
    TableNextColumn(); ImGui::Text("Game State");
    TableNextColumn();
    static int game_state = 0;
    ImGui::SetNextItemWidth(100.f);
    if (ImGui::Combo("##GameState", &game_state, kGamePartComboString, 3)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Sprite Graphics 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx1", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_graphics(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Sprite Graphics 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprGfx2", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_graphics(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Sprite Palette 1");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal1", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_palette(1),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    TableNextColumn(); ImGui::Text("Sprite Palette 2");
    TableNextColumn();
    if (gui::InputHexByte("##SprPal2", 
                          overworld_->mutable_overworld_map(current_map)->mutable_sprite_palette(2),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    
    ImGui::EndTable();
  }
}

void MapPropertiesSystem::DrawCustomFeaturesTab(int current_map) {
  if (BeginTable("CustomFeatures", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    
    TableNextColumn(); ImGui::Text("Area Size");
    TableNextColumn();
    static const char *area_size_names[] = {"Small (1x1)", "Large (2x2)", "Wide (2x1)", "Tall (1x2)"};
    int current_area_size = static_cast<int>(overworld_->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("##AreaSize", &current_area_size, area_size_names, 4)) {
      overworld_->mutable_overworld_map(current_map)->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_area_size));
      RefreshOverworldMap();
    }
    
    static uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    if (asm_version >= 2) {
      TableNextColumn(); ImGui::Text("Main Palette");
      TableNextColumn();
      if (gui::InputHexByte("##MainPal", 
                            overworld_->mutable_overworld_map(current_map)->mutable_main_palette(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshMapPalette();
        RefreshOverworldMap();
      }
    }
    
    if (asm_version >= 3) {
      TableNextColumn(); ImGui::Text("Animated GFX");
      TableNextColumn();
      if (gui::InputHexByte("##AnimGfx", 
                            overworld_->mutable_overworld_map(current_map)->mutable_animated_gfx(),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      
      TableNextColumn(); ImGui::Text("Subscreen Overlay");
      TableNextColumn();
      if (gui::InputHexWord("##SubOverlay", 
                            overworld_->mutable_overworld_map(current_map)->mutable_subscreen_overlay(),
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
  
  if (BeginTable("TileGraphics", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Sheet", ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("GFX ID", ImGuiTableColumnFlags_WidthFixed, 80);
    
    for (int i = 0; i < 4; i++) {
      TableNextColumn();
      ImGui::Text("Sheet %d", i);
      TableNextColumn();
      if (gui::InputHexByte(absl::StrFormat("##TileGfx%d", i).c_str(),
                            overworld_->mutable_overworld_map(current_map)->mutable_custom_tileset(i),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      
      TableNextColumn();
      ImGui::Text("Sheet %d", i + 4);
      TableNextColumn();
      if (gui::InputHexByte(absl::StrFormat("##TileGfx%d", i + 4).c_str(),
                            overworld_->mutable_overworld_map(current_map)->mutable_custom_tileset(i + 4),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
    }
    
    ImGui::EndTable();
  }
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

}  // namespace editor
}  // namespace yaze

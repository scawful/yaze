#include "overworld_editor_manager.h"

#include "app/gfx/snes_color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/zelda3/overworld/overworld_map.h"

namespace yaze {
namespace editor {

using namespace ImGui;

absl::Status OverworldEditorManager::DrawV3SettingsPanel() {
  if (BeginTabItem("v3 Settings")) {
    Text("ZSCustomOverworld v3 Settings");
    Separator();
    
    // Check if custom ASM is applied
    uint8_t asm_version = GetCustomASMVersion();
    if (asm_version >= 3 && asm_version != 0xFF) {
      TextColored(ImVec4(0, 1, 0, 1), "Custom Overworld ASM v%d Applied", asm_version);
    } else if (asm_version == 0x00) {
      TextColored(ImVec4(1, 1, 0, 1), "Vanilla ROM - Custom features available via flag");
    } else {
      TextColored(ImVec4(1, 0, 0, 1), "Custom ASM v%d - Consider upgrading to v3", asm_version);
    }
    
    Separator();
    
    RETURN_IF_ERROR(DrawCustomOverworldSettings());
    RETURN_IF_ERROR(DrawAreaSpecificSettings());
    RETURN_IF_ERROR(DrawTransitionSettings());
    RETURN_IF_ERROR(DrawOverlaySettings());
    
    EndTabItem();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawCustomOverworldSettings() {
  if (TreeNode("Custom Overworld Features")) {
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Area-Specific Background Colors", 
                                       &enable_area_specific_bg_,
                                       "Allows each overworld area to have its own background color"));
    
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Main Palette Override", 
                                       &enable_main_palette_,
                                       "Allows each area to override the main palette"));
    
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Mosaic Transitions", 
                                       &enable_mosaic_,
                                       "Enables mosaic screen transitions between areas"));
    
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Custom GFX Groups", 
                                       &enable_gfx_groups_,
                                       "Allows each area to have custom tile GFX groups"));
    
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Subscreen Overlays", 
                                       &enable_subscreen_overlay_,
                                       "Enables custom subscreen overlays (fog, sky, etc.)"));
    
    RETURN_IF_ERROR(DrawBooleanSetting("Enable Animated GFX Override", 
                                       &enable_animated_gfx_,
                                       "Allows each area to have custom animated tiles"));
    
    Separator();
    
    if (Button("Apply Custom Overworld ASM")) {
      RETURN_IF_ERROR(ApplyCustomOverworldASM());
    }
    SameLine();
    HOVER_HINT("Writes the custom overworld settings to ROM");
    
    TreePop();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawAreaSpecificSettings() {
  if (TreeNode("Area-Specific Settings")) {
    // Map selection
    int map_count = zelda3::kNumOverworldMaps;
    SliderInt("Map Index", &current_map_index_, 0, map_count - 1);
    
    auto* current_map = overworld_->mutable_overworld_map(current_map_index_);
    
    // Area size controls
    RETURN_IF_ERROR(DrawAreaSizeControls());
    
    // Background color
    if (enable_area_specific_bg_) {
      uint16_t bg_color = current_map->area_specific_bg_color();
      RETURN_IF_ERROR(DrawColorPicker("Background Color", &bg_color));
      current_map->set_area_specific_bg_color(bg_color);
    }
    
    // Main palette
    if (enable_main_palette_) {
      uint8_t main_palette = current_map->main_palette();
      SliderInt("Main Palette", (int*)&main_palette, 0, 5);
      current_map->set_main_palette(main_palette);
    }
    
    // Mosaic settings
    if (enable_mosaic_) {
      RETURN_IF_ERROR(DrawMosaicControls());
    }
    
    // GFX groups
    if (enable_gfx_groups_) {
      RETURN_IF_ERROR(DrawGfxGroupControls());
    }
    
    // Subscreen overlay
    if (enable_subscreen_overlay_) {
      uint16_t overlay = current_map->subscreen_overlay();
      RETURN_IF_ERROR(DrawOverlaySetting("Subscreen Overlay", &overlay));
      current_map->set_subscreen_overlay(overlay);
    }
    
    // Animated GFX
    if (enable_animated_gfx_) {
      uint8_t animated_gfx = current_map->animated_gfx();
      RETURN_IF_ERROR(DrawGfxGroupSetting("Animated GFX", &animated_gfx));
      current_map->set_animated_gfx(animated_gfx);
    }
    
    TreePop();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawAreaSizeControls() {
  auto* current_map = overworld_->mutable_overworld_map(current_map_index_);
  
  const char* area_size_names[] = {"Small", "Large", "Wide", "Tall"};
  int current_size = static_cast<int>(current_map->area_size());
  
  if (Combo("Area Size", &current_size, area_size_names, 4)) {
    current_map->SetAreaSize(static_cast<zelda3::AreaSizeEnum>(current_size));
  }
  
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawMosaicControls() {
  auto* current_map = overworld_->mutable_overworld_map(current_map_index_);
  const auto& mosaic = current_map->mosaic_expanded();
  
  bool mosaic_up = mosaic[0];
  bool mosaic_down = mosaic[1];
  bool mosaic_left = mosaic[2];
  bool mosaic_right = mosaic[3];
  
  if (Checkbox("Mosaic Up", &mosaic_up)) {
    current_map->set_mosaic_expanded(0, mosaic_up);
  }
  SameLine();
  if (Checkbox("Mosaic Down", &mosaic_down)) {
    current_map->set_mosaic_expanded(1, mosaic_down);
  }
  if (Checkbox("Mosaic Left", &mosaic_left)) {
    current_map->set_mosaic_expanded(2, mosaic_left);
  }
  SameLine();
  if (Checkbox("Mosaic Right", &mosaic_right)) {
    current_map->set_mosaic_expanded(3, mosaic_right);
  }
  
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawGfxGroupControls() {
  auto* current_map = overworld_->mutable_overworld_map(current_map_index_);
  
  Text("Custom Tile GFX Groups:");
  for (int i = 0; i < 8; i++) {
    uint8_t gfx_id = current_map->custom_tileset(i);
    std::string label = "GFX " + std::to_string(i);
    RETURN_IF_ERROR(DrawGfxGroupSetting(label.c_str(), &gfx_id));
    current_map->set_custom_tileset(i, gfx_id);
    if (i < 7) SameLine();
  }
  
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawTransitionSettings() {
  if (TreeNode("Transition Settings")) {
    Text("Complex area transition calculations are automatically handled");
    Text("based on neighboring area sizes (Large, Wide, Tall, Small).");
    
    if (GetCustomASMVersion() >= 3) {
      TextColored(ImVec4(0, 1, 0, 1), "Using v3+ enhanced transitions");
    } else {
      TextColored(ImVec4(1, 1, 0, 1), "Using vanilla/v2 transitions");
    }
    
    TreePop();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawOverlaySettings() {
  if (TreeNode("Interactive Overlay Settings")) {
    Text("Interactive overlays reveal holes and change map elements.");
    
    auto* current_map = overworld_->mutable_overworld_map(current_map_index_);
    
    Text("Map %d has %s", current_map_index_, 
         current_map->has_overlay() ? "interactive overlay" : "no overlay");
    
    if (current_map->has_overlay()) {
      Text("Overlay ID: 0x%04X", current_map->overlay_id());
      Text("Overlay data size: %zu bytes", current_map->overlay_data().size());
    }
    
    TreePop();
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::ApplyCustomOverworldASM() {
  return overworld_->SaveCustomOverworldASM(
      enable_area_specific_bg_, enable_main_palette_, enable_mosaic_,
      enable_gfx_groups_, enable_subscreen_overlay_, enable_animated_gfx_);
}

bool OverworldEditorManager::ValidateV3Compatibility() {
  uint8_t asm_version = GetCustomASMVersion();
  return (asm_version >= 3 && asm_version != 0xFF);
}

bool OverworldEditorManager::CheckCustomASMApplied() {
  uint8_t asm_version = GetCustomASMVersion();
  return (asm_version != 0xFF && asm_version != 0x00);
}

uint8_t OverworldEditorManager::GetCustomASMVersion() {
  return (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
}

absl::Status OverworldEditorManager::DrawBooleanSetting(const char* label, bool* setting, 
                                                        const char* help_text) {
  Checkbox(label, setting);
  if (help_text && IsItemHovered()) {
    SetTooltip("%s", help_text);
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawColorPicker(const char* label, uint16_t* color) {
  gfx::SnesColor snes_color(*color);
  ImVec4 imgui_color = snes_color.rgb();
  
  if (ColorEdit3(label, &imgui_color.x)) {
    gfx::SnesColor new_color;
    new_color.set_rgb(imgui_color);
    *color = new_color.snes();
  }
  
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawOverlaySetting(const char* label, uint16_t* overlay) {
  int overlay_int = *overlay;
  if (InputInt(label, &overlay_int, 1, 16, ImGuiInputTextFlags_CharsHexadecimal)) {
    *overlay = static_cast<uint16_t>(overlay_int & 0xFFFF);
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawGfxGroupSetting(const char* label, uint8_t* gfx_id, 
                                                         int max_value) {
  int gfx_int = *gfx_id;
  if (SliderInt(label, &gfx_int, 0, max_value)) {
    *gfx_id = static_cast<uint8_t>(gfx_int);
  }
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawUnifiedSettingsTable() {
  // Create a comprehensive settings table that combines toolset and properties
  if (BeginTable("##UnifiedOverworldSettings", 6, 
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | 
                 ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit)) {
    
    // Setup columns with proper widths
    TableSetupColumn(ICON_MD_BUILD " Tools", ImGuiTableColumnFlags_WidthFixed, 120);
    TableSetupColumn(ICON_MD_MAP " World", ImGuiTableColumnFlags_WidthFixed, 100);
    TableSetupColumn(ICON_MD_IMAGE " Graphics", ImGuiTableColumnFlags_WidthFixed, 100);
    TableSetupColumn(ICON_MD_PALETTE " Palette", ImGuiTableColumnFlags_WidthFixed, 100);
    TableSetupColumn(ICON_MD_SETTINGS " Properties", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn(ICON_MD_EXTENSION " v3 Features", ImGuiTableColumnFlags_WidthFixed, 120);
    TableHeadersRow();
    
    TableNextRow();
    
    // Tools column
    TableNextColumn();
    RETURN_IF_ERROR(DrawToolsetInSettings());
    
    // World column  
    TableNextColumn();
    Text(ICON_MD_PUBLIC " Current World");
    SetNextItemWidth(80.f);
    // if (Combo("##world", &current_world_, kWorldList.data(), 3)) {
    //   // World change logic would go here
    // }
    
    // Graphics column
    TableNextColumn();
    Text(ICON_MD_IMAGE " Area Graphics");
    // Graphics controls would go here
    
    // Palette column
    TableNextColumn();
    Text(ICON_MD_PALETTE " Area Palette");
    // Palette controls would go here
    
    // Properties column
    TableNextColumn();
    Text(ICON_MD_SETTINGS " Map Properties");
    // Map properties would go here
    
    // v3 Features column
    TableNextColumn();
    uint8_t asm_version = GetCustomASMVersion();
    if (asm_version >= 3 && asm_version != 0xFF) {
      TextColored(ImVec4(0, 1, 0, 1), ICON_MD_NEW_RELEASES " v3 Active");
      if (Button(ICON_MD_TUNE " Settings")) {
        // Open v3 settings
      }
    } else {
      TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), ICON_MD_UPGRADE " v3 Available");
      if (Button(ICON_MD_UPGRADE " Upgrade")) {
        // Trigger upgrade
      }
    }
    
    EndTable();
  }
  
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::DrawToolsetInSettings() {
  // Compact toolset layout within the settings table
  BeginGroup();
  
  // Core editing tools in a compact grid
  if (Button(ICON_MD_PAN_TOOL_ALT, ImVec2(25, 25))) {
    // Set PAN mode
  }
  HOVER_HINT("Pan (1)");
  
  SameLine();
  if (Button(ICON_MD_DRAW, ImVec2(25, 25))) {
    // Set DRAW_TILE mode
  }
  HOVER_HINT("Draw Tile (2)");
  
  SameLine();
  if (Button(ICON_MD_DOOR_FRONT, ImVec2(25, 25))) {
    // Set ENTRANCES mode
  }
  HOVER_HINT("Entrances (3)");
  
  SameLine();
  if (Button(ICON_MD_DOOR_BACK, ImVec2(25, 25))) {
    // Set EXITS mode
  }
  HOVER_HINT("Exits (4)");
  
  // Second row
  if (Button(ICON_MD_GRASS, ImVec2(25, 25))) {
    // Set ITEMS mode
  }
  HOVER_HINT("Items (5)");
  
  SameLine();
  if (Button(ICON_MD_PEST_CONTROL_RODENT, ImVec2(25, 25))) {
    // Set SPRITES mode
  }
  HOVER_HINT("Sprites (6)");
  
  SameLine();
  if (Button(ICON_MD_ADD_LOCATION, ImVec2(25, 25))) {
    // Set TRANSPORTS mode
  }
  HOVER_HINT("Transports (7)");
  
  SameLine();
  if (Button(ICON_MD_MUSIC_NOTE, ImVec2(25, 25))) {
    // Set MUSIC mode
  }
  HOVER_HINT("Music (8)");
  
  EndGroup();
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::HandleCanvasSelectionTransfer() {
  // This could be called to manage bidirectional selection transfer
  // For now, it's a placeholder for future canvas interaction management
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::TransferOverworldSelectionToScratch() {
  // Transfer logic would go here to copy selections from overworld to scratch
  // This could be integrated with the editor's context system
  return absl::OkStatus();
}

absl::Status OverworldEditorManager::TransferScratchSelectionToOverworld() {
  // Transfer logic would go here to copy selections from scratch to overworld
  // This could be integrated with the editor's context system
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze

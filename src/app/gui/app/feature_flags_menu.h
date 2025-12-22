#ifndef YAZE_APP_GUI_FEATURE_FLAGS_MENU_H
#define YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

#include "core/features.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze {

class Rom;  // Forward declaration

namespace gui {

using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::EndMenu;
using ImGui::MenuItem;
using ImGui::Separator;

struct FlagsMenu {
  void DrawOverworldFlags() {
    Checkbox("Enable Overworld Sprites",
             &core::FeatureFlags::get().overworld.kDrawOverworldSprites);
    Separator();
    Checkbox("Save Overworld Maps",
             &core::FeatureFlags::get().overworld.kSaveOverworldMaps);
    Checkbox("Save Overworld Entrances",
             &core::FeatureFlags::get().overworld.kSaveOverworldEntrances);
    Checkbox("Save Overworld Exits",
             &core::FeatureFlags::get().overworld.kSaveOverworldExits);
    Checkbox("Save Overworld Items",
             &core::FeatureFlags::get().overworld.kSaveOverworldItems);
    Checkbox("Save Overworld Properties",
             &core::FeatureFlags::get().overworld.kSaveOverworldProperties);
    Checkbox("Enable Custom Overworld Features",
             &core::FeatureFlags::get().overworld.kLoadCustomOverworld);
    ImGui::SameLine();
    if (ImGui::Button("?")) {
      ImGui::OpenPopup("CustomOverworldHelp");
    }
    if (ImGui::BeginPopup("CustomOverworldHelp")) {
      ImGui::Text("This flag enables ZSCustomOverworld features.");
      ImGui::Text("If ZSCustomOverworld ASM is already applied to the ROM,");
      ImGui::Text("features are auto-enabled regardless of this flag.");
      ImGui::Text("For vanilla ROMs, enable this to use custom features.");
      ImGui::EndPopup();
    }
    Checkbox("Apply ZSCustomOverworld ASM",
             &core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM);
    
    Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Experimental");
    
    Checkbox("Enable Special World Tail (0xA0-0xBF)",
             &core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion);
    ImGui::SameLine();
    if (ImGui::Button("?##TailHelp")) {
      ImGui::OpenPopup("TailExpansionHelp");
    }
    if (ImGui::BeginPopup("TailExpansionHelp")) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "EXPERIMENTAL FEATURE");
      ImGui::Separator();
      ImGui::Text("Enables access to special world tail maps (0xA0-0xBF).");
      ImGui::Text("These are unused map slots that can be made editable.");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REQUIRES:");
      ImGui::BulletText("ZSCustomOverworld v3 ASM");
      ImGui::BulletText("Pointer table expansion ASM patch");
      ImGui::Spacing();
      ImGui::Text("Without proper ASM patches, tail maps will show");
      ImGui::Text("blank tiles (safe fallback behavior).");
      ImGui::EndPopup();
    }
  }

  void DrawDungeonFlags() {
    Checkbox("Save Dungeon Maps", &core::FeatureFlags::get().kSaveDungeonMaps);
    Checkbox("Enable Custom Objects", &core::FeatureFlags::get().kEnableCustomObjects);
    ImGui::SameLine();
    if (ImGui::Button("?##CustomObjHelp")) {
      ImGui::OpenPopup("CustomObjectsHelp");
    }
    if (ImGui::BeginPopup("CustomObjectsHelp")) {
      ImGui::Text("Enables custom dungeon object support:");
      ImGui::BulletText("Minecart track editor panel");
      ImGui::BulletText("Custom object graphics (0x31, 0x32)");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "REQUIRES:");
      ImGui::BulletText("custom_objects_folder set in project file");
      ImGui::BulletText("Custom object .bin files in that folder");
      ImGui::EndPopup();
    }
  }

  void DrawResourceFlags() {
    Checkbox("Save All Palettes", &core::FeatureFlags::get().kSaveAllPalettes);
    Checkbox("Save Gfx Groups", &core::FeatureFlags::get().kSaveGfxGroups);
    Checkbox("Save Graphics Sheets",
             &core::FeatureFlags::get().kSaveGraphicsSheet);
  }

  void DrawSystemFlags() {
    Checkbox("Enable Console Logging",
             &core::FeatureFlags::get().kLogToConsole);
    Checkbox("Enable Performance Monitoring",
             &core::FeatureFlags::get().kEnablePerformanceMonitoring);
    Checkbox("Enable Tiered GFX Architecture",
             &core::FeatureFlags::get().kEnableTieredGfxArchitecture);
    // REMOVED: "Log Instructions" - DisassemblyViewer is always active
    // Use the viewer's UI controls to enable/disable recording if needed
    Checkbox("Use Native File Dialog (NFD)",
             &core::FeatureFlags::get().kUseNativeFileDialog);
  }

  // ZSCustomOverworld ROM-level enable flags (requires loaded ROM)
  void DrawZSCustomOverworldFlags(Rom* rom) {
    if (!rom || !rom->is_loaded()) {
      ImGui::TextDisabled("Load a ROM to configure ZSCustomOverworld flags");
      return;
    }

    auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);
    if (!zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
      ImGui::TextDisabled("ROM does not support ZSCustomOverworld (v2+ required)");
      return;
    }

    ImGui::TextWrapped(
        "These flags globally enable/disable ZSCustomOverworld features. "
        "When disabled, the game uses vanilla behavior.");
    ImGui::Spacing();

    // Area-Specific Background Color
    bool bg_enabled =
        (*rom)[zelda3::OverworldCustomAreaSpecificBGEnabled] != 0x00;
    if (Checkbox("Area Background Colors", &bg_enabled)) {
      (*rom)[zelda3::OverworldCustomAreaSpecificBGEnabled] =
          bg_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area custom background color (v2+)");
    }

    // Main Palette
    bool main_pal_enabled =
        (*rom)[zelda3::OverworldCustomMainPaletteEnabled] != 0x00;
    if (Checkbox("Custom Main Palette", &main_pal_enabled)) {
      (*rom)[zelda3::OverworldCustomMainPaletteEnabled] =
          main_pal_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area custom main palette (v2+)");
    }

    // Mosaic
    bool mosaic_enabled =
        (*rom)[zelda3::OverworldCustomMosaicEnabled] != 0x00;
    if (Checkbox("Custom Mosaic Effects", &mosaic_enabled)) {
      (*rom)[zelda3::OverworldCustomMosaicEnabled] =
          mosaic_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area mosaic effect control (v2+)");
    }

    // Animated GFX
    bool anim_enabled =
        (*rom)[zelda3::OverworldCustomAnimatedGFXEnabled] != 0x00;
    if (Checkbox("Custom Animated GFX", &anim_enabled)) {
      (*rom)[zelda3::OverworldCustomAnimatedGFXEnabled] =
          anim_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area animated tile graphics (v3+)");
    }

    // Subscreen Overlay
    bool overlay_enabled =
        (*rom)[zelda3::OverworldCustomSubscreenOverlayEnabled] != 0x00;
    if (Checkbox("Custom Subscreen Overlay", &overlay_enabled)) {
      (*rom)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
          overlay_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area visual effect overlays (v3+)");
    }

    // Tile GFX Groups
    bool tile_gfx_enabled =
        (*rom)[zelda3::OverworldCustomTileGFXGroupEnabled] != 0x00;
    if (Checkbox("Custom Tile GFX Groups", &tile_gfx_enabled)) {
      (*rom)[zelda3::OverworldCustomTileGFXGroupEnabled] =
          tile_gfx_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Enable per-area custom tile graphics groups (v3+)");
    }
  }
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

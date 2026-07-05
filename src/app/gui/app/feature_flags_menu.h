#ifndef YAZE_APP_GUI_FEATURE_FLAGS_MENU_H
#define YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

#include "app/gui/core/theme_manager.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
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
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    Checkbox(tr("Enable Overworld Sprites"),
             &core::FeatureFlags::get().overworld.kDrawOverworldSprites);
    Separator();
    Checkbox(tr("Save Overworld Maps"),
             &core::FeatureFlags::get().overworld.kSaveOverworldMaps);
    Checkbox(tr("Save Overworld Entrances"),
             &core::FeatureFlags::get().overworld.kSaveOverworldEntrances);
    Checkbox(tr("Save Overworld Exits"),
             &core::FeatureFlags::get().overworld.kSaveOverworldExits);
    Checkbox(tr("Save Overworld Items"),
             &core::FeatureFlags::get().overworld.kSaveOverworldItems);
    Checkbox(tr("Save Overworld Properties"),
             &core::FeatureFlags::get().overworld.kSaveOverworldProperties);
    Checkbox(tr("Enable Custom Overworld Features"),
             &core::FeatureFlags::get().overworld.kLoadCustomOverworld);
    ImGui::SameLine();
    if (ImGui::Button("?")) {
      ImGui::OpenPopup("CustomOverworldHelp");
    }
    if (ImGui::BeginPopup("CustomOverworldHelp")) {
      ImGui::Text(tr("This flag enables ZSCustomOverworld features."));
      ImGui::Text(
          tr("If ZSCustomOverworld ASM is already applied to the ROM,"));
      ImGui::Text(tr("features are auto-enabled regardless of this flag."));
      ImGui::Text(tr("For vanilla ROMs, enable this to use custom features."));
      ImGui::EndPopup();
    }
    Checkbox(tr("Apply ZSCustomOverworld ASM"),
             &core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM);

    Separator();
    ImGui::TextColored(ConvertColorToImVec4(theme.warning), tr("Experimental"));

    Checkbox(tr("Enable Special World Tail (0xA0-0xBF)"),
             &core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion);
    ImGui::SameLine();
    if (ImGui::Button("?##TailHelp")) {
      ImGui::OpenPopup("TailExpansionHelp");
    }
    if (ImGui::BeginPopup("TailExpansionHelp")) {
      ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                         tr("EXPERIMENTAL FEATURE"));
      ImGui::Separator();
      ImGui::Text(tr("Enables access to special world tail maps (0xA0-0xBF)."));
      ImGui::Text(tr("These are unused map slots that can be made editable."));
      ImGui::Spacing();
      ImGui::TextColored(ConvertColorToImVec4(theme.error), tr("REQUIRES:"));
      ImGui::BulletText(tr("ZSCustomOverworld v3 ASM"));
      ImGui::BulletText(tr("Pointer table expansion ASM patch"));
      ImGui::Spacing();
      ImGui::Text(tr("Without proper ASM patches, tail maps will show"));
      ImGui::Text(tr("blank tiles (safe fallback behavior)."));
      ImGui::EndPopup();
    }
  }

  void DrawDungeonFlags() {
    const auto& theme = ThemeManager::Get().GetCurrentTheme();
    Checkbox(tr("Save Dungeon Maps"),
             &core::FeatureFlags::get().kSaveDungeonMaps);
    ImGui::Separator();
    ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary),
                       tr("Dungeon Save Controls"));
    Checkbox(tr("Save Objects"),
             &core::FeatureFlags::get().dungeon.kSaveObjects);
    Checkbox(tr("Save Sprites"),
             &core::FeatureFlags::get().dungeon.kSaveSprites);
    Checkbox(tr("Save Room Headers"),
             &core::FeatureFlags::get().dungeon.kSaveRoomHeaders);
    Checkbox(tr("Save Torches"),
             &core::FeatureFlags::get().dungeon.kSaveTorches);
    Checkbox(tr("Save Pits"), &core::FeatureFlags::get().dungeon.kSavePits);
    Checkbox(tr("Save Blocks"), &core::FeatureFlags::get().dungeon.kSaveBlocks);
    Checkbox(tr("Save Collision"),
             &core::FeatureFlags::get().dungeon.kSaveCollision);
    Checkbox(tr("Save Chests"), &core::FeatureFlags::get().dungeon.kSaveChests);
    Checkbox(tr("Save Pot Items"),
             &core::FeatureFlags::get().dungeon.kSavePotItems);
    Checkbox(tr("Save Palettes"),
             &core::FeatureFlags::get().dungeon.kSavePalettes);
    ImGui::Separator();
    Checkbox(tr("Enable Custom Objects"),
             &core::FeatureFlags::get().kEnableCustomObjects);
    ImGui::SameLine();
    if (ImGui::Button("?##CustomObjHelp")) {
      ImGui::OpenPopup("CustomObjectsHelp");
    }
    if (ImGui::BeginPopup("CustomObjectsHelp")) {
      ImGui::Text(tr("Enables custom dungeon object support:"));
      ImGui::BulletText(tr("Minecart track editor panel"));
      ImGui::BulletText(tr("Custom object graphics (0x31, 0x32)"));
      ImGui::Spacing();
      ImGui::TextColored(ConvertColorToImVec4(theme.warning), tr("REQUIRES:"));
      ImGui::BulletText(tr("custom_objects_folder set in project file"));
      ImGui::BulletText(tr("Custom object .bin files in that folder"));
      ImGui::EndPopup();
    }
  }

  void DrawResourceFlags() {
    Checkbox(tr("Save All Palettes"),
             &core::FeatureFlags::get().kSaveAllPalettes);
    Checkbox(tr("Save Gfx Groups"), &core::FeatureFlags::get().kSaveGfxGroups);
    Checkbox(tr("Save Graphics Sheets"),
             &core::FeatureFlags::get().kSaveGraphicsSheet);
    Checkbox(tr("Save Messages"), &core::FeatureFlags::get().kSaveMessages);
  }

  void DrawSystemFlags() {
    Checkbox(tr("Enable Console Logging"),
             &core::FeatureFlags::get().kLogToConsole);
    Checkbox(tr("Enable Performance Monitoring"),
             &core::FeatureFlags::get().kEnablePerformanceMonitoring);
    Checkbox(tr("Enable Tiered GFX Architecture"),
             &core::FeatureFlags::get().kEnableTieredGfxArchitecture);
    // REMOVED: "Log Instructions" - DisassemblyViewer is always active
    // Use the viewer's UI controls to enable/disable recording if needed
    Checkbox(tr("Use Native File Dialog (NFD)"),
             &core::FeatureFlags::get().kUseNativeFileDialog);
  }

  // ZSCustomOverworld ROM-level enable flags (requires loaded ROM)
  void DrawZSCustomOverworldFlags(Rom* rom) {
    if (!rom || !rom->is_loaded()) {
      ImGui::TextDisabled(
          tr("Load a ROM to configure ZSCustomOverworld flags"));
      return;
    }

    auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);
    if (!zelda3::OverworldVersionHelper::SupportsCustomBGColors(rom_version)) {
      ImGui::TextDisabled(
          tr("ROM does not support ZSCustomOverworld (v2+ required)"));
      return;
    }

    ImGui::TextWrapped(
        tr("These flags globally enable/disable ZSCustomOverworld features. "
           "When disabled, the game uses vanilla behavior."));
    ImGui::Spacing();

    // Area-Specific Background Color
    bool bg_enabled =
        (*rom)[zelda3::OverworldCustomAreaSpecificBGEnabled] != 0x00;
    if (Checkbox(tr("Area Background Colors"), &bg_enabled)) {
      (*rom)[zelda3::OverworldCustomAreaSpecificBGEnabled] =
          bg_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Enable per-area custom background color (v2+)"));
    }

    // Main Palette
    bool main_pal_enabled =
        (*rom)[zelda3::OverworldCustomMainPaletteEnabled] != 0x00;
    if (Checkbox(tr("Custom Main Palette"), &main_pal_enabled)) {
      (*rom)[zelda3::OverworldCustomMainPaletteEnabled] =
          main_pal_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Enable per-area custom main palette (v2+)"));
    }

    // Mosaic
    bool mosaic_enabled = (*rom)[zelda3::OverworldCustomMosaicEnabled] != 0x00;
    if (Checkbox(tr("Custom Mosaic Effects"), &mosaic_enabled)) {
      (*rom)[zelda3::OverworldCustomMosaicEnabled] =
          mosaic_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Enable per-area mosaic effect control (v2+)"));
    }

    // Animated GFX
    bool anim_enabled =
        (*rom)[zelda3::OverworldCustomAnimatedGFXEnabled] != 0x00;
    if (Checkbox(tr("Custom Animated GFX"), &anim_enabled)) {
      (*rom)[zelda3::OverworldCustomAnimatedGFXEnabled] =
          anim_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Enable per-area animated tile graphics (v3+)"));
    }

    // Subscreen Overlay
    bool overlay_enabled =
        (*rom)[zelda3::OverworldCustomSubscreenOverlayEnabled] != 0x00;
    if (Checkbox(tr("Custom Subscreen Overlay"), &overlay_enabled)) {
      (*rom)[zelda3::OverworldCustomSubscreenOverlayEnabled] =
          overlay_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Enable per-area visual effect overlays (v3+)"));
    }

    // Tile GFX Groups
    bool tile_gfx_enabled =
        (*rom)[zelda3::OverworldCustomTileGFXGroupEnabled] != 0x00;
    if (Checkbox(tr("Custom Tile GFX Groups"), &tile_gfx_enabled)) {
      (*rom)[zelda3::OverworldCustomTileGFXGroupEnabled] =
          tile_gfx_enabled ? 0x01 : 0x00;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Enable per-area custom tile graphics groups (v3+)"));
    }
  }
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

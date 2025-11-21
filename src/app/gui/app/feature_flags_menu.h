#ifndef YAZE_APP_GUI_FEATURE_FLAGS_MENU_H
#define YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

#include "core/features.h"
#include "imgui/imgui.h"

namespace yaze {
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
  }

  void DrawDungeonFlags() {
    Checkbox("Save Dungeon Maps", &core::FeatureFlags::get().kSaveDungeonMaps);
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
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_FEATURE_FLAGS_MENU_H

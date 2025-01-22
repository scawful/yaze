#ifndef YAZE_APP_EDITOR_UTILS_FLAGS_H
#define YAZE_APP_EDITOR_UTILS_FLAGS_H

#include "app/core/features.h"
#include "core/common.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using core::FeatureFlags;
using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::EndMenu;
using ImGui::MenuItem;
using ImGui::Separator;

struct FlagsMenu {
  void DrawOverworldFlags() {
    Checkbox("Enable Overworld Sprites",
             &FeatureFlags::get().overworld.kDrawOverworldSprites);
    Separator();
    Checkbox("Save Overworld Maps",
             &FeatureFlags::get().overworld.kSaveOverworldMaps);
    Checkbox("Save Overworld Entrances",
             &FeatureFlags::get().overworld.kSaveOverworldEntrances);
    Checkbox("Save Overworld Exits",
             &FeatureFlags::get().overworld.kSaveOverworldExits);
    Checkbox("Save Overworld Items",
             &FeatureFlags::get().overworld.kSaveOverworldItems);
    Checkbox("Save Overworld Properties",
             &FeatureFlags::get().overworld.kSaveOverworldProperties);
    Checkbox("Load Custom Overworld",
             &FeatureFlags::get().overworld.kLoadCustomOverworld);
  }

  void DrawDungeonFlags() {
    Checkbox("Draw Dungeon Room Graphics",
             &FeatureFlags::get().kDrawDungeonRoomGraphics);
    Separator();
    Checkbox("Save Dungeon Maps", &FeatureFlags::get().kSaveDungeonMaps);
  }

  void DrawResourceFlags() {
    Checkbox("Save All Palettes", &FeatureFlags::get().kSaveAllPalettes);
    Checkbox("Save Gfx Groups", &FeatureFlags::get().kSaveGfxGroups);
    Checkbox("Save Graphics Sheets", &FeatureFlags::get().kSaveGraphicsSheet);
  }

  void DrawSystemFlags() {
    Checkbox("Enable Console Logging", &FeatureFlags::get().kLogToConsole);
    Checkbox("Log Instructions to Emulator Debugger",
             &FeatureFlags::get().kLogInstructions);
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_FLAGS_H_

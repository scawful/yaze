#ifndef YAZE_APP_EDITOR_UTILS_FLAGS_H
#define YAZE_APP_EDITOR_UTILS_FLAGS_H

#include "core/common.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using core::ExperimentFlags;
using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::EndMenu;
using ImGui::MenuItem;
using ImGui::Separator;

struct FlagsMenu {
  void Draw() {
    if (BeginMenu("Overworld Flags")) {
      Checkbox("Enable Overworld Sprites",
               &ExperimentFlags::get().overworld.kDrawOverworldSprites);
      Separator();
      Checkbox("Save Overworld Maps",
               &ExperimentFlags::get().overworld.kSaveOverworldMaps);
      Checkbox("Save Overworld Entrances",
               &ExperimentFlags::get().overworld.kSaveOverworldEntrances);
      Checkbox("Save Overworld Exits",
               &ExperimentFlags::get().overworld.kSaveOverworldExits);
      Checkbox("Save Overworld Items",
               &ExperimentFlags::get().overworld.kSaveOverworldItems);
      Checkbox("Save Overworld Properties",
               &ExperimentFlags::get().overworld.kSaveOverworldProperties);
      Checkbox("Load Custom Overworld",
               &ExperimentFlags::get().overworld.kLoadCustomOverworld);
      ImGui::EndMenu();
    }

    if (BeginMenu("Dungeon Flags")) {
      Checkbox("Draw Dungeon Room Graphics",
               &ExperimentFlags::get().kDrawDungeonRoomGraphics);
      Separator();
      Checkbox("Save Dungeon Maps", &ExperimentFlags::get().kSaveDungeonMaps);
      ImGui::EndMenu();
    }

    Checkbox("Use built-in file dialog",
             &ExperimentFlags::get().kNewFileDialogWrapper);
    Checkbox("Enable Console Logging", &ExperimentFlags::get().kLogToConsole);
    Checkbox("Enable Texture Streaming",
             &ExperimentFlags::get().kLoadTexturesAsStreaming);
    Checkbox("Log Instructions to Debugger",
             &ExperimentFlags::get().kLogInstructions);
    Checkbox("Save All Palettes", &ExperimentFlags::get().kSaveAllPalettes);
    Checkbox("Save Gfx Groups", &ExperimentFlags::get().kSaveGfxGroups);
    Checkbox("Save Graphics Sheets",
             &ExperimentFlags::get().kSaveGraphicsSheet);
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_FLAGS_H_

#ifndef YAZE_APP_EDITOR_UTILS_FLAGS_H
#define YAZE_APP_EDITOR_UTILS_FLAGS_H

#include "core/common.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::EndMenu;
using ImGui::MenuItem;
using ImGui::Separator;

struct FlagsMenu : public core::ExperimentFlags {
  void Draw() {
    if (BeginMenu("Overworld Flags")) {
      Checkbox("Enable Overworld Sprites",
               &mutable_flags()->overworld.kDrawOverworldSprites);
      Separator();
      Checkbox("Save Overworld Maps",
               &mutable_flags()->overworld.kSaveOverworldMaps);
      Checkbox("Save Overworld Entrances",
               &mutable_flags()->overworld.kSaveOverworldEntrances);
      Checkbox("Save Overworld Exits",
               &mutable_flags()->overworld.kSaveOverworldExits);
      Checkbox("Save Overworld Items",
               &mutable_flags()->overworld.kSaveOverworldItems);
      Checkbox("Save Overworld Properties",
               &mutable_flags()->overworld.kSaveOverworldProperties);
      Checkbox("Load Custom Overworld",
               &mutable_flags()->overworld.kLoadCustomOverworld);
      ImGui::EndMenu();
    }

    if (BeginMenu("Dungeon Flags")) {
      Checkbox("Draw Dungeon Room Graphics",
               &mutable_flags()->kDrawDungeonRoomGraphics);
      Separator();
      Checkbox("Save Dungeon Maps", &mutable_flags()->kSaveDungeonMaps);
      ImGui::EndMenu();
    }

    Checkbox("Use built-in file dialog",
             &mutable_flags()->kNewFileDialogWrapper);
    Checkbox("Enable Console Logging", &mutable_flags()->kLogToConsole);
    Checkbox("Enable Texture Streaming",
             &mutable_flags()->kLoadTexturesAsStreaming);
    Checkbox("Log Instructions to Debugger",
             &mutable_flags()->kLogInstructions);
    Checkbox("Save All Palettes", &mutable_flags()->kSaveAllPalettes);
    Checkbox("Save Gfx Groups", &mutable_flags()->kSaveGfxGroups);
    Checkbox("Save Graphics Sheets", &mutable_flags()->kSaveGraphicsSheet);
    Checkbox("Use New ImGui Input", &mutable_flags()->kUseNewImGuiInput);
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_FLAGS_H_

#ifndef YAZE_APP_EDITOR_UTILS_FLAGS_H
#define YAZE_APP_EDITOR_UTILS_FLAGS_H

#include <core/common.h>
#include <imgui/imgui.h>

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginMenu;
using ImGui::Checkbox;
using ImGui::EndMenu;
using ImGui::MenuItem;
using ImGui::Separator;

struct FlagsMenu : public core::ExperimentFlags {
  void Draw() {
    if (BeginMenu("Experiment Flags")) {
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
        ImGui::EndMenu();
      }

      if (BeginMenu("Dungeon Flags")) {
        Checkbox("Draw Dungeon Room Graphics",
                 &mutable_flags()->kDrawDungeonRoomGraphics);
        Separator();
        Checkbox("Save Dungeon Maps", &mutable_flags()->kSaveDungeonMaps);
        ImGui::EndMenu();
      }

      if (BeginMenu("Emulator Flags")) {
        Checkbox("Load Audio Device", &mutable_flags()->kLoadAudioDevice);
        ImGui::EndMenu();
      }

      Checkbox("Use built-in file dialog",
               &mutable_flags()->kNewFileDialogWrapper);
      Checkbox("Enable Console Logging", &mutable_flags()->kLogToConsole);
      Checkbox("Enable Texture Streaming",
               &mutable_flags()->kLoadTexturesAsStreaming);
      Checkbox("Use Bitmap Manager", &mutable_flags()->kUseBitmapManager);
      Checkbox("Log Instructions to Debugger",
               &mutable_flags()->kLogInstructions);
      Checkbox("Save All Palettes", &mutable_flags()->kSaveAllPalettes);
      Checkbox("Save Gfx Groups", &mutable_flags()->kSaveGfxGroups);
      Checkbox("Use New ImGui Input", &mutable_flags()->kUseNewImGuiInput);
      ImGui::EndMenu();
    }
  }
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_FLAGS_H_
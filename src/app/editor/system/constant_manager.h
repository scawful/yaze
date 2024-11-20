#ifndef YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H

#include <cstddef>

#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

class ConstantManager {
 public:
  void ShowConstantManager() {
    ImGui::Begin("Constant Manager");

    // Tab bar for the different types of constants
    // Overworld, dungeon, graphics, expanded
    ImGui::TextWrapped(
        "This is the constant manager. It allows you to view and edit "
        "constants in the ROM. You should only edit these if you know what "
        "you're doing.");

    if (ImGui::BeginTabBar("Constant Manager Tabs")) {
      if (ImGui::BeginTabItem("Overworld")) {
        ImGui::Text("Overworld constants");
        ImGui::Separator();
        ImGui::Text("OverworldCustomASMHasBeenApplied: %d",
                    zelda3::overworld::OverworldCustomASMHasBeenApplied);
        ImGui::Text("OverworldCustomAreaSpecificBGPalette: %d",
                    zelda3::overworld::OverworldCustomAreaSpecificBGPalette);
        ImGui::Text("OverworldCustomAreaSpecificBGEnabled: %d",
                    zelda3::overworld::OverworldCustomAreaSpecificBGEnabled);
        ImGui::Text("OverworldCustomMainPaletteArray: %d",
                    zelda3::overworld::OverworldCustomMainPaletteArray);
        ImGui::Text("OverworldCustomMainPaletteEnabled: %d",
                    zelda3::overworld::OverworldCustomMainPaletteEnabled);
        ImGui::Text("OverworldCustomMosaicArray: %d",
                    zelda3::overworld::OverworldCustomMosaicArray);
        ImGui::Text("OverworldCustomMosaicEnabled: %d",
                    zelda3::overworld::OverworldCustomMosaicEnabled);
        ImGui::Text("OverworldCustomAnimatedGFXArray: %d",
                    zelda3::overworld::OverworldCustomAnimatedGFXArray);
        ImGui::Text("OverworldCustomAnimatedGFXEnabled: %d",
                    zelda3::overworld::OverworldCustomAnimatedGFXEnabled);

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Dungeon")) {
        ImGui::Text("Dungeon constants");
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Graphics")) {
        ImGui::Text("Graphics constants");
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Expanded")) {
        ImGui::Text("Expanded constants");
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::End();
  }
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H

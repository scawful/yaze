#ifndef YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H

#include <cstddef>

#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"

namespace yaze {
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
                    zelda3::OverworldCustomASMHasBeenApplied);
        ImGui::Text("OverworldCustomAreaSpecificBGPalette: %d",
                    zelda3::OverworldCustomAreaSpecificBGPalette);
        ImGui::Text("OverworldCustomAreaSpecificBGEnabled: %d",
                    zelda3::OverworldCustomAreaSpecificBGEnabled);
        ImGui::Text("OverworldCustomMainPaletteArray: %d",
                    zelda3::OverworldCustomMainPaletteArray);
        ImGui::Text("OverworldCustomMainPaletteEnabled: %d",
                    zelda3::OverworldCustomMainPaletteEnabled);
        ImGui::Text("OverworldCustomMosaicArray: %d",
                    zelda3::OverworldCustomMosaicArray);
        ImGui::Text("OverworldCustomMosaicEnabled: %d",
                    zelda3::OverworldCustomMosaicEnabled);
        ImGui::Text("OverworldCustomAnimatedGFXArray: %d",
                    zelda3::OverworldCustomAnimatedGFXArray);
        ImGui::Text("OverworldCustomAnimatedGFXEnabled: %d",
                    zelda3::OverworldCustomAnimatedGFXEnabled);

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
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_CONSTANT_MANAGER_H

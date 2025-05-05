#ifndef YAZE_APP_CORE_FEATURES_H
#define YAZE_APP_CORE_FEATURES_H

#include <string>

#include "imgui/imgui.h"

namespace yaze {
namespace core {

/**
 * @class FeatureFlags
 * @brief A class to manage experimental feature flags.
 */
class FeatureFlags {
 public:
  struct Flags {
    // Log instructions to the GUI debugger.
    bool kLogInstructions = true;

    // Flag to enable the saving of all palettes to the Rom.
    bool kSaveAllPalettes = false;

    // Flag to enable the saving of gfx groups to the rom.
    bool kSaveGfxGroups = false;

    // Flag to enable the change queue, which could have any anonymous
    // save routine for the Rom. In practice, just the overworld tilemap
    // and tile32 save.
    bool kSaveWithChangeQueue = false;

    // Save dungeon map edits to the Rom.
    bool kSaveDungeonMaps = false;

    // Save graphics sheet to the Rom.
    bool kSaveGraphicsSheet = false;

    // Log to the console.
    bool kLogToConsole = false;

    // Overworld flags
    struct Overworld {
      // Load and render overworld sprites to the screen. Unstable.
      bool kDrawOverworldSprites = false;

      // Save overworld map edits to the Rom.
      bool kSaveOverworldMaps = true;

      // Save overworld entrances to the Rom.
      bool kSaveOverworldEntrances = true;

      // Save overworld exits to the Rom.
      bool kSaveOverworldExits = true;

      // Save overworld items to the Rom.
      bool kSaveOverworldItems = true;

      // Save overworld properties to the Rom.
      bool kSaveOverworldProperties = true;

      // Load custom overworld data from the ROM and enable UI.
      bool kLoadCustomOverworld = false;
    } overworld;
  };

  static Flags &get() {
    static Flags instance;
    return instance;
  }

  std::string Serialize() const {
    std::string result;
    result +=
        "kLogInstructions: " + std::to_string(get().kLogInstructions) + "\n";
    result +=
        "kSaveAllPalettes: " + std::to_string(get().kSaveAllPalettes) + "\n";
    result += "kSaveGfxGroups: " + std::to_string(get().kSaveGfxGroups) + "\n";
    result +=
        "kSaveWithChangeQueue: " + std::to_string(get().kSaveWithChangeQueue) +
        "\n";
    result +=
        "kSaveDungeonMaps: " + std::to_string(get().kSaveDungeonMaps) + "\n";
    result += "kLogToConsole: " + std::to_string(get().kLogToConsole) + "\n";
    result += "kDrawOverworldSprites: " +
              std::to_string(get().overworld.kDrawOverworldSprites) + "\n";
    result += "kSaveOverworldMaps: " +
              std::to_string(get().overworld.kSaveOverworldMaps) + "\n";
    result += "kSaveOverworldEntrances: " +
              std::to_string(get().overworld.kSaveOverworldEntrances) + "\n";
    result += "kSaveOverworldExits: " +
              std::to_string(get().overworld.kSaveOverworldExits) + "\n";
    result += "kSaveOverworldItems: " +
              std::to_string(get().overworld.kSaveOverworldItems) + "\n";
    result += "kSaveOverworldProperties: " +
              std::to_string(get().overworld.kSaveOverworldProperties) + "\n";
    return result;
  }
};

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

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_FEATURES_H
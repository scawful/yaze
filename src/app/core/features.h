#ifndef YAZE_APP_CORE_FEATURES_H
#define YAZE_APP_CORE_FEATURES_H

#include <string>

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
    // WARNING: Setting this to true causes SEVERE performance degradation
    bool kLogInstructions = false;

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

    // Enable performance monitoring and timing.
    bool kEnablePerformanceMonitoring = true;

    // Use NFD (Native File Dialog) instead of bespoke file dialog implementation.
#if defined(YAZE_ENABLE_NFD) && YAZE_ENABLE_NFD
    bool kUseNativeFileDialog = true;
#else
    bool kUseNativeFileDialog = false;
#endif

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

      // Enable custom overworld features for vanilla ROMs or override detection.
      // If ZSCustomOverworld ASM is already applied, features are auto-enabled.
      bool kLoadCustomOverworld = false;
      
      // Apply ZSCustomOverworld ASM patches when upgrading ROM versions.
      bool kApplyZSCustomOverworldASM = false;
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
    result += "kLoadCustomOverworld: " +
              std::to_string(get().overworld.kLoadCustomOverworld) + "\n";
    result += "kApplyZSCustomOverworldASM: " +
              std::to_string(get().overworld.kApplyZSCustomOverworldASM) + "\n";
    result += "kUseNativeFileDialog: " +
              std::to_string(get().kUseNativeFileDialog) + "\n";
    return result;
  }
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_FEATURES_H
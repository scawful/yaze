#ifndef YAZE_CORE_FEATURES_H
#define YAZE_CORE_FEATURES_H

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
    // REMOVED: kLogInstructions - DisassemblyViewer is now always enabled
    // It uses sparse address-map recording (Mesen-style) with zero performance
    // impact Recording can be disabled per-viewer via UI if needed

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

    // Dungeon save controls (granular safety)
    struct Dungeon {
      bool kSaveObjects = true;
      bool kSaveSprites = true;
      bool kSaveRoomHeaders = true;
      bool kSaveTorches = true;
      bool kSavePits = true;
      bool kSaveBlocks = true;
      bool kSaveCollision = true;
      bool kSaveChests = true;
      bool kSavePotItems = true;
      bool kSavePalettes = true;

      // UI/UX
      // When enabled, the dungeon editor uses a single stable "Workbench"
      // window instead of spawning a panel per open room.
      bool kUseWorkbench = true;
    } dungeon;

    // Save graphics sheet to the Rom.
    bool kSaveGraphicsSheet = false;

    // Save message text to the Rom.
    bool kSaveMessages = true;

    // Log to the console.
    bool kLogToConsole = false;

    // Enable performance monitoring and timing.
    bool kEnablePerformanceMonitoring = true;

    // Enable the new tiered graphics architecture.
    bool kEnableTieredGfxArchitecture = true;

    // Enable custom object panels (Custom Objects, Minecart Editor)
    bool kEnableCustomObjects = false;

    // Use NFD (Native File Dialog) instead of bespoke file dialog
    // implementation.
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

      // Enable custom overworld features for vanilla ROMs or override
      // detection. If ZSCustomOverworld ASM is already applied, features are
      // auto-enabled.
      bool kLoadCustomOverworld = false;

      // Apply ZSCustomOverworld ASM patches when upgrading ROM versions.
      bool kApplyZSCustomOverworldASM = false;

      // Enable experimental special-world tail expansion (maps 0xA0-0xBF).
      // When disabled, the editor/runtime will ignore those maps and fall back
      // to blanks for safety.
      bool kEnableSpecialWorldExpansion = false;
    } overworld;
  };

  static Flags& get() {
    static Flags instance;
    return instance;
  }

  std::string Serialize() const {
    std::string result;
    // REMOVED: kLogInstructions (deprecated)
    result +=
        "kSaveAllPalettes: " + std::to_string(get().kSaveAllPalettes) + "\n";
    result += "kSaveGfxGroups: " + std::to_string(get().kSaveGfxGroups) + "\n";
    result +=
        "kSaveWithChangeQueue: " + std::to_string(get().kSaveWithChangeQueue) +
        "\n";
    result +=
        "kSaveDungeonMaps: " + std::to_string(get().kSaveDungeonMaps) + "\n";
    result += "kSaveDungeonObjects: " +
              std::to_string(get().dungeon.kSaveObjects) + "\n";
    result += "kSaveDungeonSprites: " +
              std::to_string(get().dungeon.kSaveSprites) + "\n";
    result += "kSaveDungeonRoomHeaders: " +
              std::to_string(get().dungeon.kSaveRoomHeaders) + "\n";
    result += "kSaveDungeonTorches: " +
              std::to_string(get().dungeon.kSaveTorches) + "\n";
    result += "kSaveDungeonPits: " +
              std::to_string(get().dungeon.kSavePits) + "\n";
    result += "kSaveDungeonBlocks: " +
              std::to_string(get().dungeon.kSaveBlocks) + "\n";
    result += "kSaveDungeonCollision: " +
              std::to_string(get().dungeon.kSaveCollision) + "\n";
    result += "kSaveDungeonChests: " +
              std::to_string(get().dungeon.kSaveChests) + "\n";
    result += "kSaveDungeonPotItems: " +
              std::to_string(get().dungeon.kSavePotItems) + "\n";
    result += "kSaveDungeonPalettes: " +
              std::to_string(get().dungeon.kSavePalettes) + "\n";
    result += "kDungeonUseWorkbench: " +
              std::to_string(get().dungeon.kUseWorkbench) + "\n";
    result += "kSaveMessages: " + std::to_string(get().kSaveMessages) + "\n";
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
    result += "kEnableSpecialWorldExpansion: " +
              std::to_string(get().overworld.kEnableSpecialWorldExpansion) +
              "\n";
    result +=
        "kUseNativeFileDialog: " + std::to_string(get().kUseNativeFileDialog) +
        "\n";
    result += "kEnableTieredGfxArchitecture: " +
              std::to_string(get().kEnableTieredGfxArchitecture) + "\n";
    result += "kEnableCustomObjects: " +
              std::to_string(get().kEnableCustomObjects) + "\n";
    return result;
  }
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_FEATURES_H

#ifndef YAZE_ZELDA3_OVERWORLD_VERSION_HELPER_H
#define YAZE_ZELDA3_OVERWORLD_VERSION_HELPER_H

#include <cstdint>

#include "rom/rom.h"
#include "zelda3/common.h"

namespace yaze::zelda3 {

// =============================================================================
// ZSCustomOverworld Version System
// =============================================================================
//
// OVERVIEW:
// ---------
// ZSCustomOverworld is an ASM patch system that extends the overworld
// capabilities of A Link to the Past ROMs. This header provides centralized
// version detection to enable feature gating throughout the editor.
//
// VERSION HISTORY:
// ----------------
// - Vanilla (0xFF/0x00): Original game, no patches applied
// - v1: Basic expanded pointers for map data overflow
// - v2: Custom BG colors per area, main palette selection
// - v3: Area enum system, wide/tall areas, mosaic, animated GFX, overlays
//
// ROM MARKER LOCATION:
// --------------------
// The version byte is stored at OverworldCustomASMHasBeenApplied (0x140145):
//   - 0xFF or 0x00: Vanilla ROM (no ZScream ASM applied)
//   - 0x01: ZSCustomOverworld v1
//   - 0x02: ZSCustomOverworld v2
//   - 0x03+: ZSCustomOverworld v3
//
// FEATURE TESTING:
// ----------------
// To test a specific version's features:
//   1. Load a ROM with that version applied
//   2. Use GetVersion() to confirm detection works
//   3. Check feature flags with SupportsXXX() methods
//   4. Enable/edit the feature in the editor
//   5. Save and verify in an emulator
//
// UPGRADE WORKFLOW:
// -----------------
// In OverworldEditor::ApplyZSCustomOverworldASM(int target_version):
//   1. Validate current ROM version (can only upgrade, not downgrade)
//   2. Load appropriate ASM file (yaze.asm for v3, ZSCustomOverworld.asm for v2)
//   3. Apply patch via AsarWrapper
//   4. Update version markers via UpdateROMVersionMarkers()
//   5. Enable feature flags in ROM
//   6. Reload overworld data to reflect changes
//
// See overworld/README.md for complete documentation.
// =============================================================================

/// @brief Area size enumeration for v3+ ROMs
/// 
/// v3 introduces explicit area size types beyond the simple large_map_ flag:
/// - SmallArea: Standard 1x1 screen (512x512 pixels)
/// - LargeArea: 2x2 screens (1024x1024 pixels)
/// - WideArea: 2x1 screens (1024x512 pixels) - v3 only
/// - TallArea: 1x2 screens (512x1024 pixels) - v3 only
enum class AreaSizeEnum {
  SmallArea = 0,
  LargeArea = 1,
  WideArea = 2,
  TallArea = 3,
};

/**
 * @brief ROM version detection for overworld features
 *
 * Centralizes version checks to distinguish between:
 * - Vanilla: No ZScream patches (uses parent system for large maps only)
 * - v1: Basic custom overworld features
 * - v2: Parent system + BG colors + main palettes
 * - v3: Area enum system + wide/tall areas + all features
 *
 * The version is stored at ROM address 0x140145 (OverworldCustomASMHasBeenApplied).
 */
enum class OverworldVersion {
  kVanilla = 0,     ///< 0xFF in ROM, no ZScream ASM applied
  kZSCustomV1 = 1,  ///< Basic features, expanded pointers
  kZSCustomV2 = 2,  ///< Parent system, BG colors, main palettes
  kZSCustomV3 = 3   ///< Area enum, wide/tall areas, all features
};

/**
 * @brief Helper for ROM version detection and feature gating
 *
 * Provides consistent version checking across the codebase to replace
 * scattered inline checks like:
 *   @code
 *   if (asm_version >= 3 && asm_version != 0xFF) { ... }
 *   @endcode
 *
 * With semantic helpers:
 *   @code
 *   auto version = OverworldVersionHelper::GetVersion(*rom);
 *   if (OverworldVersionHelper::SupportsAreaEnum(version)) { ... }
 *   @endcode
 *
 * USAGE EXAMPLES:
 * ---------------
 * @code
 * // Check if ROM supports custom background colors
 * auto version = OverworldVersionHelper::GetVersion(*rom_);
 * if (OverworldVersionHelper::SupportsCustomBGColors(version)) {
 *   // Enable BG color editing UI
 *   DrawBGColorEditor(current_map);
 * }
 *
 * // Log version for debugging
 * LOG_DEBUG("ROM Version: %s", GetVersionName(version));
 *
 * // Conditional feature loading
 * if (SupportsAreaEnum(version)) {
 *   AssignMapSizes(overworld_maps_);  // v3 area enum system
 * } else {
 *   FetchLargeMaps();  // Legacy parent system
 * }
 * @endcode
 *
 * FEATURE MATRIX:
 * ---------------
 * | Feature              | Vanilla | v1 | v2 | v3 |
 * |----------------------|---------|----|----|----| 
 * | Expanded Pointers    | No      | Yes| Yes| Yes|
 * | Custom BG Colors     | No      | No | Yes| Yes|
 * | Main Palette Select  | No      | No | Yes| Yes|
 * | Area Size Enum       | No      | No | No | Yes|
 * | Wide/Tall Areas      | No      | No | No | Yes|
 * | Custom Tile GFX      | No      | No | No | Yes|
 * | Animated GFX         | No      | No | No | Yes|
 * | Subscreen Overlays   | No      | No | No | Yes|
 * | Mosaic Effect        | No      | No | No | Yes|
 */
class OverworldVersionHelper {
 public:
  // ===========================================================================
  // Version Detection
  // ===========================================================================
  
  /**
   * @brief Detect ROM version from ASM marker byte
   * @param rom ROM to check
   * @return Detected overworld version enum
   *
   * Reads the version marker at OverworldCustomASMHasBeenApplied (0x140145).
   * Returns kVanilla for unpatched ROMs (marker = 0xFF or 0x00).
   */
  static OverworldVersion GetVersion(const Rom& rom) {
    if (rom.size() <= OverworldCustomASMHasBeenApplied) {
      return OverworldVersion::kVanilla;
    }
    uint8_t asm_version = rom.data()[OverworldCustomASMHasBeenApplied];

    // 0xFF = vanilla ROM (no ZScream ASM applied)
    if (asm_version == 0xFF || asm_version == 0x00) {
      return OverworldVersion::kVanilla;
    }

    if (asm_version == 1) {
      return OverworldVersion::kZSCustomV1;
    }
    if (asm_version == 2) {
      return OverworldVersion::kZSCustomV2;
    }
    if (asm_version >= 3) {
      return OverworldVersion::kZSCustomV3;
    }

    // Fallback for unknown values
    return OverworldVersion::kVanilla;
  }

  /**
   * @brief Get raw ASM version byte from ROM
   * @param rom ROM to check
   * @return Raw version byte (0x01, 0x02, 0x03, or 0xFF for vanilla)
   */
  static uint8_t GetAsmVersion(const Rom& rom) {
    return rom.data()[OverworldCustomASMHasBeenApplied];
  }
  
  // ===========================================================================
  // Feature Checks
  // ===========================================================================

  /**
   * @brief Check if ROM supports area enum system (v3+ only)
   * @param version Detected ROM version
   * @return true if v3+
   *
   * The area enum system (v3+) replaces the simple large_map_ boolean with
   * an AreaSizeEnum that supports:
   * - SmallArea (1x1): Standard single screen
   * - LargeArea (2x2): Four screens (classic Link to the Past large areas)
   * - WideArea (2x1): Two horizontal screens (v3 only)
   * - TallArea (1x2): Two vertical screens (v3 only)
   *
   * Vanilla/v1/v2 ROMs use the legacy parent system with only large_map_ flag.
   * When this returns false, use FetchLargeMaps() instead of AssignMapSizes().
   */
  static bool SupportsAreaEnum(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM uses expanded ROM space for overworld data
   * @param version Detected ROM version
   * @return true if v1+
   *
   * v1+ ROMs use expanded pointers to store data beyond vanilla limits:
   * - Map data overflow: 0x130000+
   * - Sprite data: 0x141438+
   * - Message IDs: 0x1417F8+
   * - Parent IDs: 0x140998+
   *
   * Vanilla ROMs store all data within original game space.
   */
  static bool SupportsExpandedSpace(OverworldVersion version) {
    return version != OverworldVersion::kVanilla;
  }

  /**
   * @brief Check if ROM supports custom background colors per area (v2+)
   * @param version Detected ROM version
   * @return true if v2+
   *
   * v2+ enables per-area background color customization stored at:
   * - Color data: OverworldCustomAreaSpecificBGPalette (0x140000)
   * - Enable flag: OverworldCustomAreaSpecificBGEnabled (0x140140)
   *
   * When supported, each overworld area can have a unique BG color.
   */
  static bool SupportsCustomBGColors(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV2 ||
           version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports custom tile GFX groups (v3+)
   * @param version Detected ROM version
   * @return true if v3+
   *
   * v3 enables per-area tile graphics group customization stored at:
   * - Group data: OverworldCustomTileGFXGroupArray (0x140480)
   * - Enable flag: OverworldCustomTileGFXGroupEnabled (0x140148)
   *
   * Each area can use a different set of 8 graphics sheets.
   */
  static bool SupportsCustomTileGFX(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports animated GFX selection (v3+)
   * @param version Detected ROM version
   * @return true if v3+
   *
   * v3 enables per-area animated graphics customization stored at:
   * - Anim data: OverworldCustomAnimatedGFXArray (0x1402A0)
   * - Enable flag: OverworldCustomAnimatedGFXEnabled (0x140143)
   *
   * Controls which animated tile set (waterfalls, flowers, etc.) to use.
   */
  static bool SupportsAnimatedGFX(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports subscreen overlays (v3+)
   * @param version Detected ROM version
   * @return true if v3+
   *
   * v3 enables per-area subscreen overlay customization stored at:
   * - Overlay data: OverworldCustomSubscreenOverlayArray (0x140340)
   * - Enable flag: OverworldCustomSubscreenOverlayEnabled (0x140144)
   *
   * Overlays provide additional visual layers (rain, fog, etc.).
   */
  static bool SupportsSubscreenOverlay(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  // ===========================================================================
  // Utility Methods
  // ===========================================================================

  /**
   * @brief Get human-readable version name for display/logging
   * @param version Detected ROM version
   * @return Version name string (e.g., "ZSCustomOverworld v3")
   */
  static const char* GetVersionName(OverworldVersion version) {
    switch (version) {
      case OverworldVersion::kVanilla:
        return "Vanilla";
      case OverworldVersion::kZSCustomV1:
        return "ZSCustomOverworld v1";
      case OverworldVersion::kZSCustomV2:
        return "ZSCustomOverworld v2";
      case OverworldVersion::kZSCustomV3:
        return "ZSCustomOverworld v3";
      default:
        return "Unknown";
    }
  }
};

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_VERSION_HELPER_H

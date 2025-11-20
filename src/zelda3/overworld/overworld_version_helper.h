#ifndef YAZE_ZELDA3_OVERWORLD_VERSION_HELPER_H
#define YAZE_ZELDA3_OVERWORLD_VERSION_HELPER_H

#include <cstdint>
#include "app/rom.h"
#include "zelda3/common.h"

namespace yaze::zelda3 {

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
 */
enum class OverworldVersion {
  kVanilla = 0,     // 0xFF in ROM, no ZScream ASM applied
  kZSCustomV1 = 1,  // Basic features, expanded pointers
  kZSCustomV2 = 2,  // Parent system, BG colors, main palettes
  kZSCustomV3 = 3   // Area enum, wide/tall areas, all features
};

/**
 * @brief Helper for ROM version detection and feature gating
 * 
 * Provides consistent version checking across the codebase to replace
 * scattered inline checks like:
 *   if (asm_version >= 3 && asm_version != 0xFF) { ... }
 * 
 * With semantic helpers:
 *   if (OverworldVersionHelper::SupportsAreaEnum(version)) { ... }
 */
class OverworldVersionHelper {
 public:
  /**
   * @brief Detect ROM version from ASM marker
   * @param rom ROM to check
   * @return Detected overworld version
   */
  static OverworldVersion GetVersion(const Rom& rom) {
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

  static uint8_t GetAsmVersion(const Rom& rom) {
    return rom.data()[OverworldCustomASMHasBeenApplied];
  }

  /**
   * @brief Check if ROM supports area enum system (v3+ only)
   * 
   * Area enum system allows:
   * - Wide areas (2x1 screens)
   * - Tall areas (1x2 screens)
   * - Direct area size queries
   * 
   * Vanilla/v1/v2 use parent system with large_map_ flag only.
   */
  static bool SupportsAreaEnum(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM uses expanded space for overworld data
   * 
   * v1+ ROMs use expanded pointers for:
   * - Map data (0x130000+)
   * - Sprite data (0x141438+)
   * - Message IDs (0x1417F8+)
   */
  static bool SupportsExpandedSpace(OverworldVersion version) {
    return version != OverworldVersion::kVanilla;
  }

  /**
   * @brief Check if ROM supports custom background colors (v2+)
   */
  static bool SupportsCustomBGColors(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV2 ||
           version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports custom tile GFX groups (v3+)
   */
  static bool SupportsCustomTileGFX(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports animated GFX (v3+)
   */
  static bool SupportsAnimatedGFX(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Check if ROM supports subscreen overlays (v3+)
   */
  static bool SupportsSubscreenOverlay(OverworldVersion version) {
    return version == OverworldVersion::kZSCustomV3;
  }

  /**
   * @brief Get human-readable version name
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

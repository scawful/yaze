#ifndef YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_PATCH_H
#define YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_PATCH_H

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "zelda3/overworld/diggable_tiles.h"

namespace yaze::zelda3 {

/**
 * @brief Configuration for diggable tiles ASM patch generation.
 *
 * Allows customization of hook addresses for ZSCustomOverworld compatibility.
 */
struct DiggableTilesPatchConfig {
  // Hook location - default is vanilla, but ZS may move this
  uint32_t hook_address = 0x1BBDF4;        // Where to insert JSL
  uint32_t diggable_handler = 0x1BBE3E;    // OverworldTileAction_Diggable
  uint32_t exit_address = 0x1BBE3B;        // .not_digging exit point

  // Table location in expanded ROM space
  uint32_t table_address = 0x140980;       // 64 bytes for bitfield

  // ZS feature detection
  bool detect_zs_hooks = true;             // Auto-detect if ZS modified this area
  bool use_zs_compatible_mode = false;     // Use alternate hook strategy for ZS ROMs

  // Code placement
  uint32_t freespace_address = 0x1BF000;   // Where to put new lookup routine
};

/**
 * @brief Generates ASM patches for table-based diggable tile lookup.
 *
 * Replaces the vanilla hardcoded CMP chain at $1BBDF4 with a bitfield table
 * lookup, allowing dynamic configuration of which Map16 tiles are diggable.
 */
class DiggableTilesPatch {
 public:
  /**
   * @brief Generate ASM patch code for the diggable tiles table.
   *
   * @param diggable_tiles The diggable tiles bitfield data
   * @param config Patch configuration (addresses, mode, etc.)
   * @return ASM patch code as a string
   */
  static std::string GeneratePatch(
      const DiggableTiles& diggable_tiles,
      const DiggableTilesPatchConfig& config = {});

  /**
   * @brief Export ASM patch to a file.
   *
   * @param diggable_tiles The diggable tiles bitfield data
   * @param output_path Path to write the .asm file
   * @param config Patch configuration
   * @return Status of the file write operation
   */
  static absl::Status ExportPatchFile(
      const DiggableTiles& diggable_tiles,
      const std::string& output_path,
      const DiggableTilesPatchConfig& config = {});

  /**
   * @brief Detect if ROM has ZSCustomOverworld modifications to digging code.
   *
   * Checks if the bytes at the hook address differ from vanilla expectations.
   *
   * @param rom The ROM to check
   * @return true if ZS modifications are detected
   */
  static bool DetectZSDiggingHooks(const Rom& rom);

  /**
   * @brief Get recommended patch configuration based on ROM analysis.
   *
   * Analyzes the ROM to determine if ZS modifications are present and
   * returns an appropriate configuration.
   *
   * @param rom The ROM to analyze
   * @return Recommended patch configuration
   */
  static DiggableTilesPatchConfig GetRecommendedConfig(const Rom& rom);

 private:
  /**
   * @brief Generate the vanilla-mode lookup routine ASM.
   */
  static std::string GenerateVanillaRoutine(
      const DiggableTiles& diggable_tiles,
      const DiggableTilesPatchConfig& config);

  /**
   * @brief Generate the ZS-compatible lookup routine ASM.
   */
  static std::string GenerateZSCompatibleRoutine(
      const DiggableTiles& diggable_tiles,
      const DiggableTilesPatchConfig& config);

  /**
   * @brief Generate the data table section of the patch.
   */
  static std::string GenerateDataTable(const DiggableTiles& diggable_tiles,
                                       uint32_t table_address);
};

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_PATCH_H

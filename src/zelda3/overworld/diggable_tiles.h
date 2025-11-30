#ifndef YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_H
#define YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_H

#include <array>
#include <cstdint>
#include <vector>

#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

// Tile types that indicate diggable ground
constexpr uint8_t kTileTypeDiggable1 = 0x48;
constexpr uint8_t kTileTypeDiggable2 = 0x4A;

// Bitfield size: 512 bits = 64 bytes (one bit per Map16 tile ID 0-511)
constexpr int kDiggableTilesBitfieldSize = 64;
constexpr int kMaxDiggableTileId = 512;

// ROM addresses for custom diggable tiles table
constexpr int kOverworldCustomDiggableTilesArray = 0x140980;
constexpr int kOverworldCustomDiggableTilesEnabled = 0x140149;

// Vanilla diggable Map16 tile IDs (from bank 1B at $1BBDF4-$1BBE24)
constexpr uint16_t kVanillaDiggableTiles[] = {
    0x0034, 0x0071, 0x0035, 0x010D, 0x010F,
    0x00E1, 0x00E2, 0x00DA, 0x00F8, 0x010E
};
constexpr int kNumVanillaDiggableTiles = 10;

/**
 * @brief Manages diggable tile state as a 512-bit bitfield.
 *
 * Each Map16 tile ID (0-511) has one bit indicating if it's diggable.
 * Used to replace the hardcoded CMP chain in bank 1B with a table lookup.
 */
class DiggableTiles {
 public:
  DiggableTiles() = default;

  /**
   * @brief Check if a Map16 tile ID is marked as diggable.
   */
  bool IsDiggable(uint16_t tile_id) const;

  /**
   * @brief Set or clear the diggable bit for a Map16 tile ID.
   */
  void SetDiggable(uint16_t tile_id, bool diggable);

  /**
   * @brief Clear all diggable bits.
   */
  void Clear();

  /**
   * @brief Reset to vanilla diggable tiles.
   */
  void SetVanillaDefaults();

  /**
   * @brief Get all tile IDs that are currently marked as diggable.
   */
  std::vector<uint16_t> GetAllDiggableTileIds() const;

  /**
   * @brief Get the count of tiles marked as diggable.
   */
  int GetDiggableCount() const;

  /**
   * @brief Load bitfield from raw bytes (64 bytes).
   */
  void FromBytes(const uint8_t* data);

  /**
   * @brief Write bitfield to raw bytes (64 bytes).
   */
  void ToBytes(uint8_t* data) const;

  /**
   * @brief Get raw bitfield data for direct ROM writing.
   */
  const std::array<uint8_t, kDiggableTilesBitfieldSize>& GetRawData() const {
    return bitfield_;
  }

  /**
   * @brief Check if a Tile16 should be diggable based on its component tiles.
   *
   * A Tile16 is considered diggable if ALL 4 of its component Tile8s have
   * tile type 0x48 or 0x4A (diggable ground) in the tile types array.
   *
   * @param tile16 The Tile16 to check
   * @param all_tiles_types The tile types array from ROM (512 entries)
   * @return true if all 4 component tiles are diggable types
   */
  static bool IsTile16Diggable(
      const gfx::Tile16& tile16,
      const std::array<uint8_t, 0x200>& all_tiles_types);

 private:
  std::array<uint8_t, kDiggableTilesBitfieldSize> bitfield_ = {};
};

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_DIGGABLE_TILES_H

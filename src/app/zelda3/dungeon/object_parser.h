#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_PARSER_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_PARSER_H

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Object routine information
 */
struct ObjectRoutineInfo {
  uint32_t routine_ptr;
  uint32_t tile_ptr;
  int tile_count;
  bool is_repeatable;
  bool is_orientation_dependent;

  ObjectRoutineInfo()
      : routine_ptr(0),
        tile_ptr(0),
        tile_count(0),
        is_repeatable(false),
        is_orientation_dependent(false) {}
};

/**
 * @brief Object subtype information
 */
struct ObjectSubtypeInfo {
  int subtype;
  uint32_t subtype_ptr;
  uint32_t routine_ptr;
  int max_tile_count;

  ObjectSubtypeInfo()
      : subtype(0), subtype_ptr(0), routine_ptr(0), max_tile_count(0) {}
};

/**
 * @brief Object size and orientation information
 */
struct ObjectSizeInfo {
  int width_tiles;
  int height_tiles;
  bool is_horizontal;
  bool is_repeatable;
  int repeat_count;

  ObjectSizeInfo()
      : width_tiles(0),
        height_tiles(0),
        is_horizontal(true),
        is_repeatable(false),
        repeat_count(1) {}
};

/**
 * @brief Direct ROM parser for dungeon objects
 *
 * This class replaces the SNES emulation approach with direct ROM parsing,
 * providing better performance and reliability for object rendering.
 */
class ObjectParser {
 public:
  explicit ObjectParser(Rom* rom) : rom_(rom) {}

  /**
   * @brief Parse object data directly from ROM
   *
   * @param object_id The object ID to parse
   * @return StatusOr containing the parsed tile data
   */
  absl::StatusOr<std::vector<gfx::Tile16>> ParseObject(int16_t object_id);

  /**
   * @brief Parse object routine data
   *
   * @param object_id The object ID
   * @return StatusOr containing routine information
   */
  absl::StatusOr<ObjectRoutineInfo> ParseObjectRoutine(int16_t object_id);

  /**
   * @brief Get object subtype information
   *
   * @param object_id The object ID
   * @return StatusOr containing subtype information
   */
  absl::StatusOr<ObjectSubtypeInfo> GetObjectSubtype(int16_t object_id);

  /**
   * @brief Parse object size and orientation
   *
   * @param object_id The object ID
   * @param size_byte The size byte from object data
   * @return StatusOr containing size and orientation info
   */
  absl::StatusOr<ObjectSizeInfo> ParseObjectSize(int16_t object_id,
                                                 uint8_t size_byte);

  /**
   * @brief Determine object subtype from ID
   */
  int DetermineSubtype(int16_t object_id) const;

 private:
  /**
   * @brief Parse subtype 1 objects (0x00-0xFF)
   */
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype1(int16_t object_id);

  /**
   * @brief Parse subtype 2 objects (0x100-0x1FF)
   */
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype2(int16_t object_id);

  /**
   * @brief Parse subtype 3 objects (0x200+)
   */
  absl::StatusOr<std::vector<gfx::Tile16>> ParseSubtype3(int16_t object_id);

  /**
   * @brief Read tile data from ROM
   *
   * @param address The address to read from
   * @param tile_count Number of tiles to read
   * @return StatusOr containing tile data
   */
  absl::StatusOr<std::vector<gfx::Tile16>> ReadTileData(int address,
                                                        int tile_count);

  Rom* rom_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_PARSER_H
#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_DIMENSIONS_H
#define YAZE_ZELDA3_DUNGEON_OBJECT_DIMENSIONS_H

#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

struct RoomObject;

/**
 * @brief ROM-based object dimension lookup table
 *
 * Provides accurate object dimensions for hit-testing and selection.
 * Loads dimension data from ROM tables at:
 * - Bank $01: Subtype 1/2/3 tile pointers and routine pointers
 *
 * Usage:
 *   auto& table = ObjectDimensionTable::Get();
 *   table.LoadFromRom(rom);
 *   auto [w, h] = table.GetDimensions(object_id, size);
 */
class ObjectDimensionTable {
 public:
  static ObjectDimensionTable& Get();

  struct SelectionBounds {
    int offset_x = 0;   // Offset from object x in tiles
    int offset_y = 0;   // Offset from object y in tiles
    int width = 1;      // Width in tiles
    int height = 1;     // Height in tiles
  };

  // Load dimension data from ROM
  absl::Status LoadFromRom(Rom* rom);

  // Get base dimensions for an object (without size extension)
  std::pair<int, int> GetBaseDimensions(int object_id) const;

  // Get full dimensions accounting for size parameter
  std::pair<int, int> GetDimensions(int object_id, int size) const;

  // Get dimensions for selection bounds (without size=0 inflation)
  // This version caps the size and doesn't use 32-when-zero for display
  std::pair<int, int> GetSelectionDimensions(int object_id, int size) const;

  // Get selection bounds with offsets in tile coordinates
  SelectionBounds GetSelectionBounds(int object_id, int size) const;

  // Get hit-test bounds in tile coordinates (x, y, width, height)
  std::tuple<int, int, int, int> GetHitTestBounds(const RoomObject& obj) const;

  // Check if loaded
  bool IsLoaded() const { return loaded_; }

  // Reset state (for testing)
  void Reset() {
    dimensions_.clear();
    loaded_ = false;
  }

 private:
  ObjectDimensionTable() = default;

  // Dimension entry: base width/height and extension direction
  struct DimensionEntry {
    int base_width = 1;   // Base width in tiles
    int base_height = 1;  // Base height in tiles
    // SuperSquare: Uses both size nibbles independently
    // width = (size_x + 1) * multiplier, height = (size_y + 1) * multiplier
    enum class ExtendDir { None, Horizontal, Vertical, Both, Diagonal, SuperSquare } extend_dir = ExtendDir::None;
    int extend_multiplier = 1;  // Tiles added per size unit
    bool use_32_when_zero = false;  // ASM: GetSize_1to15or32 uses 32 when size=0
    int zero_size_override = -1;  // Optional override (e.g., 26) when size=0
  };

  // Object ID -> dimension entry
  std::unordered_map<int, DimensionEntry> dimensions_;
  bool loaded_ = false;

  int ResolveEffectiveSize(const DimensionEntry& entry, int size) const;

  // Initialize default dimensions based on draw routine patterns
  void InitializeDefaults();

  // Parse ROM tables for more accurate dimensions
  void ParseSubtype1Tables(Rom* rom);
  void ParseSubtype2Tables(Rom* rom);
  void ParseSubtype3Tables(Rom* rom);
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_DIMENSIONS_H

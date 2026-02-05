#ifndef YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_
#define YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Represents a decoded custom object (from binary format)
 * 
 * Binary Format (matches Oracle-of-Secrets object_handler.asm):
 * Header (2 bytes, little-endian):
 *   Low 5 bits: Tile Count (number of tiles in this segment)
 *   High Byte: Jump Offset (added to row start position for next segment)
 * Data (Tile Count * 2 bytes):
 *   Word: vhopppcc cccccccc (SNES tilemap entry: flip, priority, palette, tile ID)
 * Repeats until Header is 0x0000.
 * 
 * Buffer Layout:
 *   Stride = 128 bytes (64 tiles per row, 2 bytes per tile)
 *   Jump offset of 0x80 (128) advances by 1 row
 */
struct CustomObject {
  struct TileMapEntry {
    int rel_x;
    int rel_y;
    uint16_t tile_data; // vhopppcc cccccccc
  };
  
  std::vector<TileMapEntry> tiles;
  
  bool IsEmpty() const { return tiles.empty(); }
};

/**
 * @brief Manages loading and caching of custom object binary files.
 */
class CustomObjectManager {
 public:
  static CustomObjectManager& Get();

  // Initialize with the full path to the custom objects folder
  // e.g., "/path/to/project/Dungeons/Objects/Data"
  void Initialize(const std::string& custom_objects_folder);

  // Override object/subtype filename mapping from project config.
  void SetObjectFileMap(
      const std::unordered_map<int, std::vector<std::string>>& map);
  void ClearObjectFileMap();
  bool HasCustomFileMap() const { return !custom_file_map_.empty(); }

  // Load a custom object from a binary file
  absl::StatusOr<std::shared_ptr<CustomObject>> LoadObject(const std::string& filename);
  
  // Get an object by ID/Subtype mapping (0x31 or 0x32)
  // Subtype index maps to the .ObjOffset table
  absl::StatusOr<std::shared_ptr<CustomObject>> GetObjectInternal(int object_id, int subtype);

  // Get number of subtypes for a custom object ID
  int GetSubtypeCount(int object_id) const;

  // Reload all cached objects (useful for editor)
  void ReloadAll();

 private:
  CustomObjectManager() = default;

  absl::StatusOr<CustomObject> ParseBinaryData(const std::vector<uint8_t>& data);
  const std::vector<std::string>* ResolveFileList(int object_id) const;

  std::string base_path_;
  std::unordered_map<std::string, std::shared_ptr<CustomObject>> cache_;
  std::unordered_map<int, std::vector<std::string>> custom_file_map_;
  
  // Mapping from subtype index to filename for ID 0x31
  static const std::vector<std::string> kSubtype1Filenames;
  // Mapping from subtype index to filename for ID 0x32
  static const std::vector<std::string> kSubtype2Filenames;
};

} // namespace zelda3
} // namespace yaze

#endif // YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_

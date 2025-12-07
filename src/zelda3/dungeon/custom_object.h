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
 * Format:
 * Header (2 bytes):
 *   Low 5 bits: Tile Count
 *   High Byte: Row Stride (offset to add to Y pointer after drawing row)
 * Data (Tile Count * 2 bytes):
 *   Word: vhopppcc cccccccc (attributes + tile ID)
 * Repeats until Header is 0x0000.
 */
struct CustomObject {
  struct TileRow {
    uint8_t count;
    uint8_t stride; // In bytes (usually 0x80 for 1 row down)
    std::vector<gfx::TileInfo> tiles;
  };

  std::vector<TileRow> rows;
  int width = 0;
  int height = 0;
  
  bool IsEmpty() const { return rows.empty(); }
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

  std::string base_path_;
  std::unordered_map<std::string, std::shared_ptr<CustomObject>> cache_;
  
  // Mapping from subtype index to filename for ID 0x31
  static const std::vector<std::string> kSubtype1Filenames;
  // Mapping from subtype index to filename for ID 0x32
  static const std::vector<std::string> kSubtype2Filenames;
};

} // namespace zelda3
} // namespace yaze

#endif // YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_

#ifndef YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_
#define YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
 *   Low 5 bits: Tile Count (0 encodes 32 tiles for a nonzero header)
 *   High Byte: Jump Offset (added to row start position for next segment)
 * Data (Tile Count * 2 bytes):
 *   Word: vhopppcc cccccccc (SNES tilemap entry: flip, priority, palette, tile ID)
 *   A zero word advances the destination without writing a tile.
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
    uint16_t tile_data;  // vhopppcc cccccccc

    bool operator==(const TileMapEntry&) const = default;
  };

  struct BoundingBox {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int width() const { return (max_x - min_x) + 1; }
    int height() const { return (max_y - min_y) + 1; }
  };

  std::vector<TileMapEntry> tiles;

  bool IsEmpty() const { return tiles.empty(); }

  // Compute the bounding box from all tile entries.
  // If tiles is empty, returns the default-initialized box (min/max=0).
  BoundingBox GetBoundingBox() const {
    if (tiles.empty())
      return {};
    BoundingBox bb;
    bb.min_x = tiles[0].rel_x;
    bb.max_x = tiles[0].rel_x;
    bb.min_y = tiles[0].rel_y;
    bb.max_y = tiles[0].rel_y;
    for (size_t i = 1; i < tiles.size(); ++i) {
      if (tiles[i].rel_x < bb.min_x)
        bb.min_x = tiles[i].rel_x;
      if (tiles[i].rel_x > bb.max_x)
        bb.max_x = tiles[i].rel_x;
      if (tiles[i].rel_y < bb.min_y)
        bb.min_y = tiles[i].rel_y;
      if (tiles[i].rel_y > bb.max_y)
        bb.max_y = tiles[i].rel_y;
    }
    return bb;
  }
};

struct CustomObjectAsset {
  CustomObject object;
  std::vector<uint8_t> source_bytes;
  std::filesystem::path resolved_path;
};

// Strict codec for the Oracle custom-object segment format. Encoding preserves
// sparse positions when the bytecode can represent them and rejects layouts
// that would otherwise move or overlap tiles.
absl::StatusOr<CustomObject> DecodeCustomObjectBinary(
    const std::vector<uint8_t>& data);
absl::StatusOr<std::vector<uint8_t>> EncodeCustomObjectBinary(
    const CustomObject& object);

// Resolves a project-relative .bin path using forward-slash separators through
// its canonical parent. Rejects absolute paths, parent traversal, symlink
// targets, and paths outside the configured custom-object folder.
absl::StatusOr<std::filesystem::path> ResolveCustomObjectAssetPath(
    const std::string& custom_objects_folder, const std::string& filename);

// Loads both the decoded object and the exact source bytes needed for a later
// stale-write check.
absl::StatusOr<CustomObjectAsset> LoadCustomObjectAsset(
    const std::string& custom_objects_folder, const std::string& filename);

// Publishes one existing asset using exact source-byte compare-and-swap,
// rollback-protected atomic replacement, and strict decoded readback. Returns
// the canonical bytes that were committed.
absl::StatusOr<std::vector<uint8_t>> PublishCustomObjectBinary(
    const std::string& custom_objects_folder, const std::string& filename,
    const CustomObject& object,
    const std::vector<uint8_t>& expected_source_bytes,
    const std::filesystem::path& expected_resolved_path);

// Applies the per-family tilemap transform used by Oracle at draw time while
// preserving zero payload words as transparent/no-op entries. Source assets
// and editor write-back deliberately retain their untransformed words.
uint16_t CustomObjectRuntimeTileWord(int object_id, uint16_t source_word);

enum class CustomObjectMappingOrigin {
  kDefaultFilename,
  kConfiguredFilename,
  kConfiguredSlotUnmapped,
};

struct CustomObjectSlotBinding {
  std::string filename;
  CustomObjectMappingOrigin origin =
      CustomObjectMappingOrigin::kConfiguredSlotUnmapped;
};

/**
 * @brief Manages loading and caching of custom object binary files.
 */
class CustomObjectManager {
 public:
  struct State {
    std::string base_path;
    std::unordered_map<int, std::vector<std::string>> custom_file_map;

    bool operator==(const State&) const = default;
  };

  static CustomObjectManager& Get();

  // Initialize with the full path to the custom objects folder
  // e.g., "/path/to/project/Dungeons/Objects/Data"
  void Initialize(const std::string& custom_objects_folder);

  // Override object/subtype filename mapping from project config.
  void SetObjectFileMap(
      const std::unordered_map<int, std::vector<std::string>>& map);
  void ClearObjectFileMap();
  bool HasCustomFileMap() const;

  // Editor sessions share the manager entry point but retain independent
  // project paths, mappings, decoded assets, and generation tokens.
  void ActivateRuntimeContext(uint64_t context_id, const State& state);
  void ActivateStandaloneContext();
  void RemoveRuntimeContext(uint64_t context_id);
  std::optional<uint64_t> active_runtime_context_id() const {
    return active_runtime_context_id_;
  }

  // Load a custom object from a binary file. Successes and failures are cached
  // in the active context until its assets or configuration are refreshed.
  absl::StatusOr<std::shared_ptr<CustomObject>> LoadObject(
      const std::string& filename);

  // Get an object by fixed runtime ID/subtype mapping.
  // Subtype index maps to the .ObjOffset table
  absl::StatusOr<std::shared_ptr<CustomObject>> GetObjectInternal(int object_id,
                                                                  int subtype);

  // Get number of subtypes for a custom object ID
  int GetSubtypeCount(int object_id) const;

  // The current Oracle runtime dispatch tables have fixed capacities. Project
  // filename mappings may replace assets within these slots but cannot add
  // new runtime subtypes.
  static int RuntimeSubtypeCountForObject(int object_id);

  // Canonical Oracle object IDs backed by fixed external runtime assets.
  static const std::array<int, 3>& RuntimeObjectIds();

  // Reload all cached objects (useful for editor)
  void ReloadAll();

  // Get the resolved file list for an object_id (empty if none)
  std::vector<std::string> GetEffectiveFileList(int object_id) const;

  // Returns the built-in subtype filename list for supported object IDs.
  // Returns an empty list for other IDs.
  static const std::vector<std::string>& DefaultSubtypeFilenamesForObject(
      int object_id);

  // Accessors for tile editor write-back
  const std::string& GetBasePath() const;
  uint64_t asset_generation() const;
  absl::StatusOr<CustomObjectSlotBinding> ResolveSlotBinding(int object_id,
                                                             int subtype) const;
  std::string ResolveFilename(int object_id, int subtype) const;

  // Snapshot/restore helpers for scoped CLI/runtime feature application.
  State SnapshotState() const;
  void RestoreState(const State& state);

 private:
  struct RuntimeContext {
    State state;
    std::unordered_map<std::string,
                       absl::StatusOr<std::shared_ptr<CustomObject>>>
        cache;
    uint64_t asset_generation = 0;
  };

  CustomObjectManager();

  RuntimeContext& ActiveContext();
  const RuntimeContext& ActiveContext() const;
  uint64_t NextAssetGeneration();
  void InvalidateCaches();
  const std::vector<std::string>* ResolveFileList(int object_id) const;
  RuntimeContext standalone_context_;
  std::unordered_map<uint64_t, RuntimeContext> runtime_contexts_;
  std::optional<uint64_t> active_runtime_context_id_;
  uint64_t next_asset_generation_ = 1;

  // Mapping from subtype index to filename for ID 0x31
  static const std::vector<std::string> kSubtype1Filenames;
  // Mapping from subtype index to filename for ID 0x32
  static const std::vector<std::string> kSubtype2Filenames;
  // Mapping from subtype index to filename for ID 0x54
  static const std::vector<std::string> kSubtype54Filenames;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_CUSTOM_OBJECT_H_

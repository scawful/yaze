#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief One contiguous ROM-backed source span for an object's tile words.
 */
struct ObjectTileSourceSpan {
  uint32_t pc_address = 0;
  std::vector<uint16_t> expected_words;
};

/**
 * @brief ROM descriptor and source snapshot authorizing an editable capture.
 *
 * Generic preview captures deliberately omit this structure. Only an
 * object-specific resolver may attach provenance to a standard-object layout.
 */
struct ObjectTileSourceProvenance {
  int16_t object_id = -1;
  uint32_t descriptor_pc_address = 0;
  uint16_t expected_descriptor_word = 0;
  std::vector<ObjectTileSourceSpan> spans;
};

/**
 * @brief A cell's exact word within an authorized source span.
 */
struct ObjectTileSourceRef {
  size_t span_index = 0;
  size_t word_index = 0;
};

/**
 * @brief Editable tile8 layout captured from an object's draw trace
 *
 * Built by running an object draw in trace_only mode and normalizing
 * the resulting TileTrace entries into a 0-based coordinate grid.
 */
struct ObjectTileLayout {
  int16_t object_id = 0;
  int origin_tile_x = 0;
  int origin_tile_y = 0;

  struct Cell {
    int rel_x = 0;            // 0-based tile X within bounding box
    int rel_y = 0;            // 0-based tile Y within bounding box
    gfx::TileInfo tile_info;  // Decomposed SNES tilemap word
    uint16_t original_word = 0;
    // Legacy name retained for preview/test compatibility. This is draw-trace
    // order only and never authorizes a ROM write; draw order is not generally
    // source order.
    int write_index = -1;
    std::optional<ObjectTileSourceRef> source_ref;
    bool modified = false;
  };

  std::vector<Cell> cells;
  int bounds_width = 0;   // Bounding box width in 8px tiles
  int bounds_height = 0;  // Bounding box height in 8px tiles

  // Legacy display/shared-data metadata. Standard writes are authorized only
  // by source_provenance + per-cell source_ref, never by this base address.
  int tile_data_address = -1;
  std::optional<ObjectTileSourceProvenance> source_provenance;
  bool is_custom = false;
  std::string custom_filename;

  static ObjectTileLayout FromTraces(
      const std::vector<ObjectDrawer::TileTrace>& traces);

  // Create an empty layout for new custom object creation
  static ObjectTileLayout CreateEmpty(int width, int height, int16_t object_id,
                                      const std::string& filename);

  Cell* FindCell(int rel_x, int rel_y);
  const Cell* FindCell(int rel_x, int rel_y) const;

  bool HasModifications() const;
  void RevertAll();
};

/**
 * @brief Fully resolved standard-object ROM writes.
 *
 * The exact half-open PC ranges are derived from the same entries that are
 * applied, allowing callers to run Hack Manifest preflight before mutation.
 */
class ObjectTileWritePlan {
 public:
  struct Write {
    int address = -1;
    uint16_t expected_word = 0;
    uint16_t word = 0;
  };

  struct WordPrecondition {
    int address = -1;
    uint16_t expected_word = 0;
  };

  ObjectTileWritePlan(const ObjectTileWritePlan&) = default;
  ObjectTileWritePlan(ObjectTileWritePlan&&) noexcept = default;
  ObjectTileWritePlan& operator=(const ObjectTileWritePlan&) = default;
  ObjectTileWritePlan& operator=(ObjectTileWritePlan&&) noexcept = default;

  const std::vector<Write>& writes() const { return writes_; }
  const std::vector<WordPrecondition>& preconditions() const {
    return preconditions_;
  }
  const std::vector<std::pair<uint32_t, uint32_t>>& write_ranges() const {
    return write_ranges_;
  }

 private:
  friend class ObjectTileEditor;

  ObjectTileWritePlan() = default;

  std::vector<Write> writes_;
  // Includes the pointer descriptor and every captured source word. Apply
  // rechecks these immediately before opening the transaction.
  std::vector<WordPrecondition> preconditions_;
  std::vector<std::pair<uint32_t, uint32_t>> write_ranges_;
};

/**
 * @brief Captures and edits the tile8 composition of dungeon objects
 *
 * Uses ObjectDrawer's trace_only mode to capture every WriteTile8 call,
 * producing an editable ObjectTileLayout. Supports rendering previews
 * and writing changes back to ROM or custom .bin files.
 */
class ObjectTileEditor {
 public:
  static constexpr int kAtlasTilesPerRow = 16;
  static constexpr int kAtlasTileRows = 64;
  static constexpr int kAtlasTileCount = kAtlasTilesPerRow * kAtlasTileRows;
  static constexpr int kAtlasWidthPx = kAtlasTilesPerRow * 8;
  static constexpr int kAtlasHeightPx = kAtlasTileRows * 8;

  explicit ObjectTileEditor(Rom* rom);

  // Capture: run object draw in trace_only mode, build editable layout
  absl::StatusOr<ObjectTileLayout> CaptureObjectLayout(
      int16_t object_id, const Room& room, const gfx::PaletteGroup& palette);
  absl::StatusOr<ObjectTileLayout> CaptureObjectLayout(
      int16_t object_id, const Room& room, const gfx::PaletteGroup& palette,
      uint8_t object_size);

  // Capture a standard object with exact ROM source provenance. The initial
  // fail-closed allowlist is intentionally limited to the two audited fixed
  // 2x2 subtype-2 objects (0x11F and 0x120).
  absl::StatusOr<ObjectTileLayout> CaptureEditableObjectLayout(
      int16_t object_id, const Room& room, const gfx::PaletteGroup& palette);

  static bool IsEditableStandardObject(int16_t object_id);

  // Render: draw layout to preview bitmap using room's gfx buffer
  absl::Status RenderLayoutToBitmap(const ObjectTileLayout& layout,
                                    gfx::Bitmap& bitmap,
                                    const uint8_t* room_gfx_buffer,
                                    const gfx::PaletteGroup& palette);

  // Build tile8 atlas from room graphics buffer.
  absl::Status BuildTile8Atlas(gfx::Bitmap& atlas,
                               const uint8_t* room_gfx_buffer,
                               const gfx::PaletteGroup& palette,
                               int display_palette = 2);

  // Write-back: standard objects patch ROM, custom objects write .bin
  absl::Status WriteBack(const ObjectTileLayout& layout);

  // Resolve and validate all standard-object writes before any ROM mutation.
  absl::StatusOr<ObjectTileWritePlan> BuildStandardWritePlan(
      const ObjectTileLayout& layout) const;

  // Atomically apply a previously validated standard-object write plan.
  absl::Status ApplyStandardWritePlan(const ObjectTileWritePlan& plan);

  // Check how many objects share the same tile data pointer
  int CountObjectsSharingTileData(int16_t object_id) const;

 private:
  Rom* rom_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_

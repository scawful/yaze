#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_

#include <cstdint>
#include <string>
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
    int rel_x = 0;           // 0-based tile X within bounding box
    int rel_y = 0;           // 0-based tile Y within bounding box
    gfx::TileInfo tile_info; // Decomposed SNES tilemap word
    uint16_t original_word = 0;
    bool modified = false;
  };

  std::vector<Cell> cells;
  int bounds_width = 0;   // Bounding box width in 8px tiles
  int bounds_height = 0;  // Bounding box height in 8px tiles

  int tile_data_address = -1;  // ROM address for write-back
  bool is_custom = false;
  std::string custom_filename;

  static ObjectTileLayout FromTraces(
      const std::vector<ObjectDrawer::TileTrace>& traces);

  // Create an empty layout for new custom object creation
  static ObjectTileLayout CreateEmpty(int width, int height,
                                      int16_t object_id,
                                      const std::string& filename);

  Cell* FindCell(int rel_x, int rel_y);
  const Cell* FindCell(int rel_x, int rel_y) const;

  bool HasModifications() const;
  void RevertAll();
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
      int16_t object_id, const Room& room,
      const gfx::PaletteGroup& palette);

  // Render: draw layout to preview bitmap using room's gfx buffer
  absl::Status RenderLayoutToBitmap(
      const ObjectTileLayout& layout, gfx::Bitmap& bitmap,
      const uint8_t* room_gfx_buffer,
      const gfx::PaletteGroup& palette);

  // Build tile8 atlas from room graphics buffer.
  absl::Status BuildTile8Atlas(
      gfx::Bitmap& atlas, const uint8_t* room_gfx_buffer,
      const gfx::PaletteGroup& palette, int display_palette = 2);

  // Write-back: standard objects patch ROM, custom objects write .bin
  absl::Status WriteBack(const ObjectTileLayout& layout);

  // Check how many objects share the same tile data pointer
  int CountObjectsSharingTileData(int16_t object_id) const;

 private:
  Rom* rom_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_TILE_EDITOR_H_

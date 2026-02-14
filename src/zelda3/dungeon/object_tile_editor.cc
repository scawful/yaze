#include "zelda3/dungeon/object_tile_editor.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>

#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

// =============================================================================
// ObjectTileLayout
// =============================================================================

ObjectTileLayout ObjectTileLayout::FromTraces(
    const std::vector<ObjectDrawer::TileTrace>& traces) {
  ObjectTileLayout layout;
  if (traces.empty()) return layout;

  layout.object_id = static_cast<int16_t>(traces[0].object_id);

  // Find bounding box
  int min_x = traces[0].x_tile;
  int min_y = traces[0].y_tile;
  int max_x = min_x;
  int max_y = min_y;
  for (const auto& t : traces) {
    min_x = std::min(min_x, static_cast<int>(t.x_tile));
    min_y = std::min(min_y, static_cast<int>(t.y_tile));
    max_x = std::max(max_x, static_cast<int>(t.x_tile));
    max_y = std::max(max_y, static_cast<int>(t.y_tile));
  }

  layout.origin_tile_x = min_x;
  layout.origin_tile_y = min_y;
  layout.bounds_width = max_x - min_x + 1;
  layout.bounds_height = max_y - min_y + 1;

  layout.cells.reserve(traces.size());
  for (const auto& t : traces) {
    Cell cell;
    cell.rel_x = t.x_tile - min_x;
    cell.rel_y = t.y_tile - min_y;

    // Reconstruct TileInfo from trace fields
    bool h_mirror = (t.flags & 0x1) != 0;
    bool v_mirror = (t.flags & 0x2) != 0;
    bool priority = (t.flags & 0x4) != 0;
    uint8_t palette = (t.flags >> 3) & 0x7;
    cell.tile_info = gfx::TileInfo(t.tile_id, palette, v_mirror, h_mirror, priority);
    cell.original_word = gfx::TileInfoToWord(cell.tile_info);
    cell.modified = false;

    layout.cells.push_back(cell);
  }

  return layout;
}

ObjectTileLayout ObjectTileLayout::CreateEmpty(int width, int height,
                                                int16_t object_id,
                                                const std::string& filename) {
  ObjectTileLayout layout;
  layout.object_id = object_id;
  layout.origin_tile_x = 0;
  layout.origin_tile_y = 0;
  layout.bounds_width = width;
  layout.bounds_height = height;
  layout.tile_data_address = -1;
  layout.is_custom = true;
  layout.custom_filename = filename;

  layout.cells.reserve(width * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      Cell cell;
      cell.rel_x = x;
      cell.rel_y = y;
      cell.tile_info = gfx::TileInfo(0, 2, false, false, false);
      cell.original_word = gfx::TileInfoToWord(cell.tile_info);
      cell.modified = true;
      layout.cells.push_back(cell);
    }
  }

  return layout;
}

ObjectTileLayout::Cell* ObjectTileLayout::FindCell(int rel_x, int rel_y) {
  for (auto& cell : cells) {
    if (cell.rel_x == rel_x && cell.rel_y == rel_y) return &cell;
  }
  return nullptr;
}

const ObjectTileLayout::Cell* ObjectTileLayout::FindCell(int rel_x,
                                                          int rel_y) const {
  for (const auto& cell : cells) {
    if (cell.rel_x == rel_x && cell.rel_y == rel_y) return &cell;
  }
  return nullptr;
}

bool ObjectTileLayout::HasModifications() const {
  for (const auto& cell : cells) {
    if (cell.modified) return true;
  }
  return false;
}

void ObjectTileLayout::RevertAll() {
  for (auto& cell : cells) {
    if (cell.modified) {
      cell.tile_info = gfx::WordToTileInfo(cell.original_word);
      cell.modified = false;
    }
  }
}

// =============================================================================
// ObjectTileEditor
// =============================================================================

ObjectTileEditor::ObjectTileEditor(Rom* rom) : rom_(rom) {}

absl::StatusOr<ObjectTileLayout> ObjectTileEditor::CaptureObjectLayout(
    int16_t object_id, const Room& room,
    const gfx::PaletteGroup& palette) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Create a temporary object at a known position for tracing
  RoomObject obj(object_id, 2, 2, 0x12, 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();

  // Check if this is a custom object
  bool is_custom = false;
  std::string custom_filename;
  int subtype = obj.size_ & 0x1F;
  if (core::FeatureFlags::get().kEnableCustomObjects) {
    auto custom_result =
        CustomObjectManager::Get().GetObjectInternal(object_id, subtype);
    if (custom_result.ok()) {
      is_custom = true;
      custom_filename =
          CustomObjectManager::Get().ResolveFilename(object_id, subtype);
    }
  }

  // Create drawer and set up trace collection
  ObjectDrawer drawer(rom_, room.id(), room.get_gfx_buffer().data());

  std::vector<ObjectDrawer::TileTrace> traces;
  traces.reserve(256);
  drawer.SetTraceCollector(&traces, /*trace_only=*/true);

  // Draw the object to collect traces
  gfx::BackgroundBuffer dummy_bg1(512, 512);
  gfx::BackgroundBuffer dummy_bg2(512, 512);
  auto status = drawer.DrawObject(obj, dummy_bg1, dummy_bg2, palette);
  drawer.ClearTraceCollector();

  if (!status.ok()) {
    return status;
  }

  if (traces.empty()) {
    return absl::NotFoundError("Object produced no tile traces");
  }

  // Build layout from traces
  ObjectTileLayout layout = ObjectTileLayout::FromTraces(traces);
  layout.tile_data_address = obj.tile_data_ptr_;
  layout.is_custom = is_custom;
  layout.custom_filename = custom_filename;

  return layout;
}

absl::Status ObjectTileEditor::RenderLayoutToBitmap(
    const ObjectTileLayout& layout, gfx::Bitmap& bitmap,
    const uint8_t* room_gfx_buffer,
    const gfx::PaletteGroup& palette) {
  if (!room_gfx_buffer) {
    return absl::FailedPreconditionError("No room graphics buffer");
  }
  if (layout.cells.empty()) {
    return absl::OkStatus();
  }

  int bmp_w = layout.bounds_width * 8;
  int bmp_h = layout.bounds_height * 8;

  // Create or resize bitmap
  std::vector<uint8_t> pixel_data(bmp_w * bmp_h, 0);
  bitmap.Create(bmp_w, bmp_h, 8, pixel_data);

  // Apply palette from the room's palette group (use dungeon palette index 2)
  if (palette.size() > 2) {
    bitmap.SetPalette(palette[2]);
  } else if (palette.size() > 0) {
    bitmap.SetPalette(palette[0]);
  }

  // Use a temporary ObjectDrawer just for its DrawTileToBitmap utility
  ObjectDrawer drawer(rom_, 0, room_gfx_buffer);

  for (const auto& cell : layout.cells) {
    int px = cell.rel_x * 8;
    int py = cell.rel_y * 8;
    drawer.DrawTileToBitmap(bitmap, cell.tile_info, px, py, room_gfx_buffer);
  }

  return absl::OkStatus();
}

absl::Status ObjectTileEditor::BuildTile8Atlas(
    gfx::Bitmap& atlas, const uint8_t* room_gfx_buffer,
    const gfx::PaletteGroup& palette, int display_palette) {
  if (!room_gfx_buffer) {
    return absl::FailedPreconditionError("No room graphics buffer");
  }

  std::vector<uint8_t> pixel_data(
      kAtlasWidthPx * kAtlasHeightPx, 0);
  atlas.Create(kAtlasWidthPx, kAtlasHeightPx, 8, pixel_data);

  if (palette.size() > static_cast<size_t>(display_palette)) {
    atlas.SetPalette(palette[display_palette]);
  } else if (palette.size() > 0) {
    atlas.SetPalette(palette[0]);
  }

  ObjectDrawer drawer(rom_, 0, room_gfx_buffer);

  for (int tile_id = 0; tile_id < kAtlasTileCount; ++tile_id) {
    int col = tile_id % kAtlasTilesPerRow;
    int row = tile_id / kAtlasTilesPerRow;
    int px = col * 8;
    int py = row * 8;

    gfx::TileInfo info(static_cast<uint16_t>(tile_id),
                        static_cast<uint8_t>(display_palette),
                        false, false, false);
    drawer.DrawTileToBitmap(atlas, info, px, py, room_gfx_buffer);
  }

  return absl::OkStatus();
}

absl::Status ObjectTileEditor::WriteBack(const ObjectTileLayout& layout) {
  if (!layout.HasModifications()) {
    return absl::OkStatus();
  }

  if (layout.is_custom) {
    // Custom object: serialize to binary format and write .bin file
    if (layout.custom_filename.empty()) {
      return absl::FailedPreconditionError(
          "Custom object has no filename for write-back");
    }

    // Group cells by rel_y to form segments
    std::map<int, std::vector<const ObjectTileLayout::Cell*>> rows;
    for (const auto& cell : layout.cells) {
      rows[cell.rel_y].push_back(&cell);
    }

    // Serialize to binary matching CustomObjectManager::ParseBinaryData format
    std::vector<uint8_t> binary;
    constexpr int kBufferStride = 128;

    int prev_buffer_pos = 0;
    for (auto& [row_y, row_cells] : rows) {
      // Sort cells by rel_x
      std::sort(row_cells.begin(), row_cells.end(),
                [](const auto* a, const auto* b) {
                  return a->rel_x < b->rel_x;
                });

      int count = static_cast<int>(row_cells.size());
      int buffer_pos_for_row = row_y * kBufferStride + row_cells[0]->rel_x * 2;
      int jump_offset =
          (row_y == rows.rbegin()->first)
              ? 0
              : kBufferStride;  // Jump to next row

      // Header: low 5 bits = count, high byte = jump_offset
      uint16_t header = (count & 0x1F) | ((jump_offset & 0xFF) << 8);
      binary.push_back(header & 0xFF);
      binary.push_back((header >> 8) & 0xFF);

      for (const auto* cell : row_cells) {
        uint16_t word = gfx::TileInfoToWord(cell->tile_info);
        binary.push_back(word & 0xFF);
        binary.push_back((word >> 8) & 0xFF);
      }

      prev_buffer_pos = buffer_pos_for_row + count * 2;
    }

    // Terminator
    binary.push_back(0);
    binary.push_back(0);

    // Write to file
    auto& mgr = CustomObjectManager::Get();
    std::filesystem::path full_path =
        std::filesystem::path(mgr.GetBasePath()) / layout.custom_filename;
    std::ofstream file(full_path, std::ios::binary);
    if (!file) {
      return absl::InternalError("Failed to open file for writing: " +
                                 full_path.string());
    }
    file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    file.close();

    // Reload cache
    mgr.ReloadAll();
    return absl::OkStatus();
  }

  // Standard object: patch ROM at tile_data_address
  if (layout.tile_data_address < 0) {
    return absl::FailedPreconditionError(
        "No tile data address for ROM write-back");
  }

  for (size_t i = 0; i < layout.cells.size(); ++i) {
    const auto& cell = layout.cells[i];
    if (!cell.modified) continue;

    int addr = layout.tile_data_address + static_cast<int>(i) * 2;
    uint16_t word = gfx::TileInfoToWord(cell.tile_info);

    // Write 2 bytes (little-endian SNES tilemap word)
    auto low = static_cast<uint8_t>(word & 0xFF);
    auto high = static_cast<uint8_t>((word >> 8) & 0xFF);
    RETURN_IF_ERROR(rom_->WriteByte(addr, low));
    RETURN_IF_ERROR(rom_->WriteByte(addr + 1, high));
  }

  return absl::OkStatus();
}

int ObjectTileEditor::CountObjectsSharingTileData(int16_t object_id) const {
  if (!rom_ || !rom_->is_loaded()) return 0;

  // Create temporary object to get its tile_data_ptr_
  RoomObject test_obj(object_id, 0, 0, 0, 0);
  test_obj.SetRom(rom_);
  test_obj.EnsureTilesLoaded();
  int target_ptr = test_obj.tile_data_ptr_;
  if (target_ptr < 0) return 0;

  // Scan type 1 objects (0x00-0xFF) for shared pointers
  int count = 0;
  for (int id = 0; id < 0x100; ++id) {
    RoomObject scan_obj(static_cast<int16_t>(id), 0, 0, 0, 0);
    scan_obj.SetRom(rom_);
    scan_obj.EnsureTilesLoaded();
    if (scan_obj.tile_data_ptr_ == target_ptr) {
      ++count;
    }
  }

  return count;
}

}  // namespace zelda3
}  // namespace yaze

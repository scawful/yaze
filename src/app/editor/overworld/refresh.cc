#include "app/core/platform/renderer.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze {
namespace app {
namespace editor {

using core::Renderer;

void OverworldEditor::RefreshChildMap(int map_index) {
  overworld_.mutable_overworld_map(map_index)->LoadAreaGraphics();
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTiles16Gfx(
      overworld_.tiles16().size());
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildBitmap(
      overworld_.GetMapTiles(current_world_));
  maps_bmp_[map_index].set_data(
      overworld_.mutable_overworld_map(map_index)->bitmap_data());
  maps_bmp_[map_index].set_modified(true);
  PRINT_IF_ERROR(status_);
}

void OverworldEditor::RefreshOverworldMap() {
  std::vector<std::future<void>> futures;
  int indices[4];

  auto refresh_map_async = [this](int map_index) {
    RefreshChildMap(map_index);
  };

  int source_map_id = current_map_;
  bool is_large = overworld_.overworld_map(current_map_)->is_large_map();
  if (is_large) {
    source_map_id = current_parent_;
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(source_map_id)->parent() + i;
      if (i >= 2) sibling_index += 6;
      futures.push_back(
          std::async(std::launch::async, refresh_map_async, sibling_index));
      indices[i] = sibling_index;
    }
  }
  indices[0] = source_map_id;
  futures.push_back(
      std::async(std::launch::async, refresh_map_async, source_map_id));

  for (auto &each : futures) {
    each.get();
  }
  int n = is_large ? 4 : 1;
  // We do texture updating on the main thread
  for (int i = 0; i < n; ++i) {
    Renderer::GetInstance().UpdateBitmap(&maps_bmp_[indices[i]]);
  }
}

absl::Status OverworldEditor::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_.mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_.AreaPalette();

  if (overworld_.overworld_map(current_map_)->is_large_map()) {
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(current_map_)->parent() + i;
      if (i >= 2) sibling_index += 6;
      RETURN_IF_ERROR(
          overworld_.mutable_overworld_map(sibling_index)->LoadPalette());
      RETURN_IF_ERROR(
          maps_bmp_[sibling_index].ApplyPalette(current_map_palette));
    }
  }

  RETURN_IF_ERROR(maps_bmp_[current_map_].ApplyPalette(current_map_palette));
  return absl::OkStatus();
}

void OverworldEditor::RefreshMapProperties() {
  auto &current_ow_map = *overworld_.mutable_overworld_map(current_map_);
  if (current_ow_map.is_large_map()) {
    // We need to copy the properties from the parent map to the children
    for (int i = 1; i < 4; i++) {
      int sibling_index = current_ow_map.parent() + i;
      if (i >= 2) {
        sibling_index += 6;
      }
      auto &map = *overworld_.mutable_overworld_map(sibling_index);
      map.set_area_graphics(current_ow_map.area_graphics());
      map.set_area_palette(current_ow_map.area_palette());
      map.set_sprite_graphics(game_state_,
                              current_ow_map.sprite_graphics(game_state_));
      map.set_sprite_palette(game_state_,
                             current_ow_map.sprite_palette(game_state_));
      map.set_message_id(current_ow_map.message_id());
    }
  }
}

absl::Status OverworldEditor::RefreshTile16Blockset() {
  if (current_blockset_ ==
      overworld_.overworld_map(current_map_)->area_graphics()) {
    return absl::OkStatus();
  }
  current_blockset_ = overworld_.overworld_map(current_map_)->area_graphics();

  overworld_.set_current_map(current_map_);
  palette_ = overworld_.AreaPalette();
  // Create the tile16 blockset image
  Renderer::GetInstance().UpdateBitmap(&tile16_blockset_bmp_);
  RETURN_IF_ERROR(tile16_blockset_bmp_.ApplyPalette(palette_));

  // Copy the tile16 data into individual tiles.
  auto tile16_data = overworld_.Tile16Blockset();

  std::vector<std::future<void>> futures;
  // Loop through the tiles and copy their pixel data into separate vectors
  for (int i = 0; i < 4096; i++) {
    futures.push_back(std::async(
        std::launch::async,
        [&](int index) {
          // Create a new vector for the pixel data of the current tile
          Bytes tile_data(16 * 16, 0x00);  // More efficient initialization

          // Copy the pixel data for the current tile into the vector
          for (int ty = 0; ty < 16; ty++) {
            for (int tx = 0; tx < 16; tx++) {
              int position = tx + (ty * 0x10);
              uint8_t value =
                  tile16_data[(index % 8 * 16) + (index / 8 * 16 * 0x80) +
                              (ty * 0x80) + tx];
              tile_data[position] = value;
            }
          }

          // Add the vector for the current tile to the vector of tile pixel
          // data
          tile16_individual_[index].set_data(tile_data);
        },
        i));
  }

  for (auto &future : futures) {
    future.get();
  }

  // Render the bitmaps of each tile.
  for (int id = 0; id < 4096; id++) {
    RETURN_IF_ERROR(tile16_individual_[id].ApplyPalette(palette_));
    Renderer::GetInstance().UpdateBitmap(&tile16_individual_[id]);
  }

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
#include "app/editor/graphics/catalog_object_transfer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

using TileKey = std::pair<uint16_t, uint16_t>;
using TilePixels = std::array<uint8_t, 64>;
constexpr int kTileSize = 8;
constexpr int kSheetWidth = zelda3::GraphicsSheetStore::kSheetWidth;
constexpr int kTilesPerRow = kSheetWidth / kTileSize;
constexpr int kTilesPerSheet = zelda3::GraphicsSheetStore::kSheetBytes / 64;
constexpr int kMaxDimension = 2048;
constexpr size_t kMaxPixels = 1024 * 1024;

bool ValidDimensions(int width, int height) {
  return width > 0 && height > 0 && width <= kMaxDimension &&
         height <= kMaxDimension && width % kTileSize == 0 &&
         height % kTileSize == 0 &&
         static_cast<size_t>(width) * height <= kMaxPixels;
}

size_t SheetOffset(uint16_t tile, int x, int y) {
  return (tile / kTilesPerRow * kTileSize + y) * kSheetWidth +
         tile % kTilesPerRow * kTileSize + x;
}

}  // namespace

absl::StatusOr<PixelIndexPlan> PlanCatalogObjectTransfer(
    const GraphicsEditorState& state, const CatalogAssetPreview& asset,
    const CatalogArtRect& crop) {
  const auto* store = ActiveSheetStore(state);
  if (!store || !state.source_context ||
      state.source_context->owner != state.game_data ||
      !state.source_context->resolve_live_source) {
    return absl::FailedPreconditionError(
        "Open Graphics from a supported object in this session");
  }
  const auto& source = *state.source_context;
  if (!ValidDimensions(source.pattern_width, source.pattern_height) ||
      source.pattern_cells.empty()) {
    return absl::FailedPreconditionError(
        "The object's source pattern geometry is unavailable");
  }
  if (asset.bpp != 3 || !ValidDimensions(asset.width, asset.height) ||
      asset.indexed_pixels.size() !=
          static_cast<size_t>(asset.width) * asset.height ||
      std::any_of(asset.indexed_pixels.begin(), asset.indexed_pixels.end(),
                  [](uint8_t pixel) { return pixel > 7; })) {
    return absl::InvalidArgumentError(
        "Object artwork requires a complete 3bpp image with indices 0..7");
  }
  if (crop.x < 0 || crop.y < 0 || !ValidDimensions(crop.width, crop.height) ||
      crop.x % kTileSize || crop.y % kTileSize || crop.width > asset.width ||
      crop.height > asset.height || crop.x > asset.width - crop.width ||
      crop.y > asset.height - crop.height) {
    return absl::InvalidArgumentError(
        "Select a tile-aligned crop inside the source artwork");
  }
  if (crop.width > source.pattern_width ||
      crop.height > source.pattern_height) {
    return absl::InvalidArgumentError(
        "Selected artwork exceeds the object's source pattern");
  }

  std::map<std::pair<int, int>, const GraphicsSourceCell*> cells;
  for (const auto& cell : source.pattern_cells) {
    if (cell.x < 0 || cell.y < 0 || cell.x >= source.pattern_width / 8 ||
        cell.y >= source.pattern_height / 8 ||
        cell.target >= source.targets.size() ||
        !cells.emplace(std::make_pair(cell.x, cell.y), &cell).second) {
      return absl::FailedPreconditionError(
          "The object's source pattern has invalid or ambiguous cells");
    }
    const auto& target = source.targets[cell.target];
    if (target.tile_index >= kTilesPerSheet ||
        !store->HasSheet(target.sheet_id)) {
      return absl::FailedPreconditionError(
          "An object source tile is unavailable in this session");
    }
  }

  for (int y = 0; y < crop.height; ++y) {
    for (int x = 0; x < crop.width; ++x) {
      if (asset.indexed_pixels[(crop.y + y) * asset.width + crop.x + x] != 0 &&
          !cells.contains({x / 8, y / 8})) {
        return absl::FailedPreconditionError(
            "Selected artwork needs a tile in a hole of the object pattern");
      }
    }
  }

  // Constraints include unselected pattern cells. Their original physical
  // glyph must survive when it aliases a tile selected by a smaller crop.
  std::map<TileKey, TilePixels> demands;
  std::set<TileKey> selected;
  for (const auto& [position, cell] : cells) {
    const auto& target = source.targets[cell->target];
    const TileKey key{target.sheet_id, target.tile_index};
    const bool inside = cell->x < crop.width / 8 && cell->y < crop.height / 8;
    TilePixels desired;
    const auto before = store->Sheet(target.sheet_id);
    for (int y = 0; y < kTileSize; ++y) {
      for (int x = 0; x < kTileSize; ++x) {
        if (inside) {
          const int raw_x = cell->hflip ? 7 - x : x;
          const int raw_y = cell->vflip ? 7 - y : y;
          desired[raw_y * 8 + raw_x] =
              asset.indexed_pixels[(crop.y + cell->y * 8 + y) * asset.width +
                                   crop.x + cell->x * 8 + x];
        } else {
          desired[y * 8 + x] = before[SheetOffset(target.tile_index, x, y)];
        }
      }
    }
    const auto [prior, inserted] = demands.emplace(key, desired);
    if (!inserted && prior->second != desired) {
      return absl::FailedPreconditionError(
          "Selected artwork conflicts with another use of a shared source "
          "tile in this object");
    }
    if (inside) {
      selected.insert(key);
    }
  }
  if (selected.empty()) {
    return absl::FailedPreconditionError(
        "Selected artwork does not address an object source tile");
  }

  std::map<uint16_t, std::vector<uint8_t>> after_sheets;
  std::vector<PixelIndexTarget> targets;
  for (const auto& [sheet, tile] : selected) {
    if (!after_sheets.contains(sheet)) {
      const auto before = store->Sheet(sheet);
      after_sheets.emplace(sheet,
                           std::vector<uint8_t>(before.begin(), before.end()));
    }
    const auto& desired = demands.at({sheet, tile});
    auto& after = after_sheets.at(sheet);
    for (int y = 0; y < kTileSize; ++y) {
      for (int x = 0; x < kTileSize; ++x) {
        after[SheetOffset(tile, x, y)] = desired[y * 8 + x];
      }
    }
    targets.push_back({sheet, tile});
  }
  return PlanPixelIndexReplacement(state, targets, after_sheets,
                                   "Apply catalog art to object source");
}

}  // namespace yaze::editor

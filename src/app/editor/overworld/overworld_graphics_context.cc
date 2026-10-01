#include "app/editor/overworld/overworld_graphics_context.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {

// Resolve the raw sheet pixel source of one Tile16 quadrant. The map must be
// the selected, built screen: its static slots and palette already include
// parent-area, special-world, and alternate-palette resolution. Sheet pixels
// remain owned by GameData::sheet_store; these colors are display metadata.
absl::StatusOr<GraphicsEditTarget> ResolveOverworldQuadrantTarget(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner) {
  if (owner == nullptr) {
    return absl::FailedPreconditionError(
        "Overworld graphics session is missing");
  }
  if (quadrant < 0 || quadrant >= 4 || tile16_id < 0 ||
      tile16_id >= zelda3::kNumTile16Individual) {
    return absl::InvalidArgumentError("Invalid Tile16 quadrant");
  }
  const auto& tile = zelda3::Tile16QuadrantInfo(tile16, quadrant);
  if (tile.id_ >= 8 * 64) {
    return absl::UnimplementedError(
        "This Tile16 quadrant does not address an overworld background sheet");
  }
  const int slot = tile.id_ / 64;
  const int tile_index = tile.id_ % 64;
  if (slot == 7 && tile_index < 32) {
    return absl::UnimplementedError(
        "Animated slot 7 tiles need an explicit frame source");
  }
  const uint16_t sheet = map.static_graphics(slot);
  if (sheet == 0 || sheet >= zelda3::kGfxSheetCount ||
      zelda3::GetGfxSheetStorageKind(sheet) ==
          zelda3::GfxSheetStorageKind::kCompressed2bpp ||
      !owner->sheet_store.HasSheet(sheet)) {
    return absl::UnimplementedError(
        "This quadrant has no loaded editable 3bpp sheet");
  }

  // BuildTileset adds $88 to slots 0, 3, 4, and 5. BuildTiles16Gfx masks
  // with $0F and adds the quadrant's palette row. Thus raw pixel indices 0-7
  // resolve to the right palette half for those slots, left for the others.
  const bool right = slot == 0 || slot == 3 || slot == 4 || slot == 5;
  const size_t base = static_cast<size_t>(tile.palette_) * 16 + (right ? 8 : 0);
  const auto& colors = map.current_palette();
  if (tile.palette_ >= 8 || colors.size() < base + 8) {
    return absl::FailedPreconditionError(
        "Selected map palette is not resolved");
  }
  std::vector<gfx::SnesColor> display_colors;
  display_colors.reserve(8);
  for (int i = 0; i < 8; ++i) {
    display_colors.push_back(colors[base + i]);
  }
  return GraphicsEditTarget{
      .sheet_id = sheet,
      .tile_index = static_cast<uint16_t>(tile_index),
      .label = absl::StrFormat(
          "Tile16 %03X / quadrant %d / tile %03X / slot %d / sheet %02X / "
          "row %d %s",
          tile16_id, quadrant + 1, tile.id_, slot, sheet, tile.palette_,
          right ? "right" : "left"),
      .palette = gfx::SnesPalette(display_colors),
  };
}

absl::StatusOr<GraphicsEditContext> MakeOverworldGraphicsEditContext(
    const zelda3::OverworldMap& map, const gfx::Tile16& tile16, int tile16_id,
    int quadrant, const zelda3::GameData* owner) {
  auto target =
      ResolveOverworldQuadrantTarget(map, tile16, tile16_id, quadrant, owner);
  if (!target.ok()) {
    return target.status();
  }
  GraphicsEditContext context;
  context.owner = owner;
  context.label = absl::StrFormat(
      "Map %02X (area %02X) / Tile16 %03X / quadrant %d / palette %02X",
      map.index(), map.parent(), tile16_id, quadrant + 1,
      map.render_main_palette());
  context.limitation =
      "Map assignments and palette are captured on entry; reopen from the "
      "overworld after changing either.";
  context.resolved = zelda3::DescribeOverworldGraphicsContext(map);
  context.targets.push_back(std::move(*target));
  return context;
}

}  // namespace yaze::editor

#include "app/editor/overworld/overworld_graphics_context.h"

#include <cstdint>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_preview_image.h"
#include "app/editor/graphics/usage_preview.h"
#include "util/macro.h"
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
  if (owner == nullptr || (map.game_data() && map.game_data() != owner)) {
    return absl::FailedPreconditionError(
        "Overworld graphics session is missing or mismatched");
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
  GraphicsEditTarget target{
      .sheet_id = sheet,
      .tile_index = static_cast<uint16_t>(tile_index),
      .label = absl::StrFormat(
          "Tile16 %03X / quadrant %d / tile %03X / slot %d / sheet %02X / "
          "row %d %s",
          tile16_id, quadrant + 1, tile.id_, slot, sheet, tile.palette_,
          right ? "right" : "left"),
      .palette = gfx::SnesPalette(display_colors),
      .source_orientation_mask =
          static_cast<uint8_t>(1 << ((tile.horizontal_mirror_ ? 1 : 0) |
                                     (tile.vertical_mirror_ ? 2 : 0))),
  };
  if (map.game_data() == owner) {
    ResolveGraphicsPaletteAddressing(target, owner, map.palette_addresses(),
                                     tile.palette_, right ? 8 : 0);
  }
  return target;
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
      "Map assignments are captured on entry; reopen from the overworld "
      "after changing assignments.";
  context.resolved = zelda3::DescribeOverworldGraphicsContext(map);
  context.targets.push_back(std::move(*target));
  // Focus the requested quadrant, but keep the rest of this Tile16 available
  // without another trip through the source editor. Each quadrant retains its
  // own palette and orientation, even when two use the same physical tile.
  for (int other = 0; other < 4; ++other) {
    if (other == quadrant) {
      continue;
    }
    auto neighbor =
        ResolveOverworldQuadrantTarget(map, tile16, tile16_id, other, owner);
    if (!neighbor.ok()) {
      if (neighbor.status().code() != absl::StatusCode::kUnimplemented) {
        return neighbor.status();
      }
      context.limitation +=
          absl::StrFormat(" Quadrant %d is omitted: %s.", other + 1,
                          neighbor.status().message());
      continue;
    }
    context.targets.push_back(std::move(*neighbor));
  }
  return context;
}

namespace {

bool LiveSessionMatches(const OverworldGraphicsLiveSource& source) {
  return source.current_game_data() == source.owner &&
         source.overworld->is_loaded();
}

bool LiveTileAvailable(const OverworldGraphicsLiveSource& source) {
  return LiveSessionMatches(source) &&
         source.tile16_id <
             static_cast<int>(source.overworld->tiles16().size());
}

}  // namespace

void AttachOverworldLiveSource(GraphicsEditContext& context,
                               OverworldGraphicsLiveSource source) {
  auto src =
      std::make_shared<const OverworldGraphicsLiveSource>(std::move(source));
  context.resolve_live_source = [src](GraphicsEditContext& fresh) {
    if (!LiveTileAvailable(*src)) {
      return absl::FailedPreconditionError("Overworld source session changed");
    }
    RETURN_IF_ERROR(src->sync_project_preview());
    auto& overworld = *src->overworld;
    overworld.SyncAreaProperties(src->source_map_id);
    auto* live = overworld.mutable_overworld_map(src->source_map_id);
    if (!live || live->HasGraphicsPreview()) {
      return absl::UnimplementedError(
          "Project graphics overrides have no writable source");
    }
    live->LoadAreaGraphics();
    RETURN_IF_ERROR(live->LoadPalette());
    auto resolved = MakeOverworldGraphicsEditContext(
        *live, overworld.tiles16()[src->tile16_id], src->tile16_id,
        src->quadrant, src->owner);
    if (!resolved.ok())
      return resolved.status();
    fresh = std::move(*resolved);
    return absl::OkStatus();
  };
  context.limitation += " Palette colors follow live edits.";
  context.refresh_palettes =
      [src](GraphicsEditContext& target) -> absl::Status {
    if (!LiveTileAvailable(*src)) {
      return absl::FailedPreconditionError("Overworld source session changed");
    }
    RETURN_IF_ERROR(src->preview_map->LoadPalette());
    auto refreshed = MakeOverworldGraphicsEditContext(
        *src->preview_map, src->overworld->tiles16()[src->tile16_id],
        src->tile16_id, src->quadrant, src->owner);
    if (!refreshed.ok())
      return refreshed.status();
    return UpdateGraphicsContextPalettes(target, std::move(*refreshed));
  };
  context.preview_revision = [src]() -> absl::StatusOr<uint64_t> {
    if (!LiveSessionMatches(*src))
      return absl::FailedPreconditionError("Overworld source session changed");
    constexpr uint64_t kPrime = 1099511628211ULL;
    uint64_t hash = 1469598103934665603ULL;
    auto add = [&](uint64_t value) {
      hash = (hash ^ value) * kPrime;
    };
    for (const auto& tile : src->overworld->tiles16())
      for (const auto& quadrant : tile.tiles_info)
        add(gfx::TileInfoToWord(quadrant));
    const int world = zelda3::WorldForOverworldMap(src->source_map_id);
    const auto& placements = src->overworld->GetMapTiles(world);
    const int local = src->source_map_id - world * 0x40;
    const int x0 = (local % 8) * 32;
    const int y0 = (local / 8) * 32;
    for (int x = x0; x < x0 + 32; ++x) {
      if (x >= static_cast<int>(placements.size()) ||
          y0 + 32 > static_cast<int>(placements[x].size()))
        return absl::FailedPreconditionError("Overworld source grid changed");
      for (int y = y0; y < y0 + 32; ++y)
        add(placements[x][y]);
    }
    return hash;
  };
  context.render_preview =
      [src]() -> absl::StatusOr<usage_preview::PreviewImage> {
    if (!LiveSessionMatches(*src)) {
      return absl::FailedPreconditionError(
          "Overworld graphics source session has changed");
    }
    auto& preview_map = *src->preview_map;
    std::set<uint16_t> ids;
    for (int slot = 0; slot < 8; ++slot) {
      ids.insert(preview_map.static_graphics(slot));
    }
    ids.insert(preview_map.animated_sheet());
    preview_map.SetGraphicsSheetOverrides(
        usage_preview::SnapshotSessionSheets(ids, src->owner));
    RETURN_IF_ERROR(preview_map.BuildTileset());
    RETURN_IF_ERROR(preview_map.BuildTiles16Gfx(
        *src->overworld->mutable_tiles16(),
        static_cast<int>(src->overworld->tiles16().size())));
    const int world = zelda3::WorldForOverworldMap(src->source_map_id);
    RETURN_IF_ERROR(
        preview_map.BuildBitmap(src->overworld->GetMapTiles(world)));
    usage_preview::PreviewImage image;
    image.width = 512;
    image.height = 512;
    image.pixels = preview_map.bitmap_data();
    image.index0_transparent = false;
    const auto& palette = preview_map.current_palette();
    for (size_t i = 0; i < palette.size() && i < image.colors.size(); ++i) {
      const auto rgb = palette[i].rgb();
      image.colors[i] =
          SDL_Color{static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                    static_cast<uint8_t>(rgb.z), 255};
    }
    return image;
  };
}

}  // namespace yaze::editor

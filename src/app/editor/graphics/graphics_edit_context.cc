#include "app/editor/graphics/graphics_edit_context.h"

#include <set>
#include <utility>

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

absl::StatusOr<GraphicsEditContext> MakeDungeonGraphicsEditContext(
    const zelda3::Room& room, const zelda3::RoomObject& object,
    const zelda3::GameData* owner) {
  auto tiles = object.GetTiles();
  if (!tiles.ok()) {
    return tiles.status();
  }
  const auto patterns = zelda3::CustomObjectManager::Get().GetTilePatterns();
  if (!patterns.ok())
    return patterns.status();
  std::vector<gfx::TileInfo> source_tiles;
  if (*patterns && !zelda3::HasActiveCustomObjectOverride(object)) {
    auto parsed = zelda3::ObjectParser(room.rom(), patterns->get())
                      .ParseObject(object.id_);
    if (!parsed.ok())
      return parsed.status();
    source_tiles = std::move(*parsed);
    tiles = std::span<const gfx::TileInfo>(source_tiles);
  }
  GraphicsEditContext context;
  context.owner = owner;
  context.label =
      absl::StrFormat("Room %03X / Object %03X / Main %02X", room.id(),
                      object.id_, room.resolved_main_blockset());
  context.resolved = zelda3::DescribeDungeonGraphicsContext(room);
  const auto& colors = room.rendered_palette();
  const auto blocks = room.blocks();
  std::set<std::pair<uint16_t, uint8_t>> seen;
  for (const auto& tile : *tiles) {
    if (!seen.emplace(tile.id_, tile.palette_).second) {
      continue;
    }
    if (tile.id_ >= 512 || tile.palette_ >= 8 ||
        (tile.id_ >= 0x1B0 && tile.id_ < 0x1D0)) {
      context.limitation =
          "Animated or unmapped tiles are omitted from pixel editing.";
      continue;
    }
    const int slot = tile.id_ / 64;
    const int offset = tile.palette_ * 16;
    if (colors.size() < static_cast<size_t>(offset + 16)) {
      return absl::FailedPreconditionError("Room palette is not rendered");
    }
    const int half = room.BackgroundPaletteOffset(slot);
    std::vector<gfx::SnesColor> palette;
    for (int index = 0; index < 8; ++index) {
      const auto& color = colors[offset + (index == 0 ? 0 : half + index)];
      palette.emplace_back(color.r, color.g, color.b);
    }
    context.targets.push_back({
        .sheet_id = blocks[slot],
        .tile_index = static_cast<uint16_t>(tile.id_ % 64),
        .label = absl::StrFormat("Tile %03X / slot %d / sheet %02X / row %d %s",
                                 tile.id_, slot, blocks[slot], tile.palette_,
                                 half == 8 ? "right" : "left"),
        .palette = gfx::SnesPalette(palette),
    });
  }
  if (context.targets.empty()) {
    return absl::UnimplementedError(
        "This object's tiles do not yet have an editable graphics source");
  }
  return context;
}

}  // namespace yaze::editor

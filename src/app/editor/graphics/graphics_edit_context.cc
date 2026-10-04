#include "app/editor/graphics/graphics_edit_context.h"

#include <map>
#include <utility>

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

std::string GraphicsEditTarget::SourceOrientationLabel() const {
  constexpr const char* kLabels[] = {"Unflipped", "Horizontal", "Vertical",
                                     "Horizontal + vertical"};
  std::string label;
  for (int orientation = 0; orientation < 4; ++orientation) {
    if ((source_orientation_mask & (1 << orientation)) == 0) {
      continue;
    }
    if (!label.empty()) {
      label += ", ";
    }
    label += kLabels[orientation];
  }
  return label;
}

absl::Status UpdateGraphicsContextPalettes(GraphicsEditContext& context,
                                           GraphicsEditContext refreshed) {
  if (context.owner != refreshed.owner ||
      context.targets.size() != refreshed.targets.size()) {
    return absl::FailedPreconditionError(
        "Graphics source changed; reopen Edit Graphics from the source.");
  }
  for (size_t i = 0; i < context.targets.size(); ++i) {
    const auto& old = context.targets[i];
    const auto& next = refreshed.targets[i];
    if (old.sheet_id != next.sheet_id || old.tile_index != next.tile_index ||
        old.label != next.label ||
        old.source_orientation_mask != next.source_orientation_mask ||
        next.palette.size() != 8) {
      return absl::FailedPreconditionError(
          "Graphics source changed; reopen Edit Graphics from the source.");
    }
  }
  for (size_t i = 0; i < context.targets.size(); ++i) {
    context.targets[i].palette = std::move(refreshed.targets[i].palette);
  }
  context.resolved = std::move(refreshed.resolved);
  return absl::OkStatus();
}

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
  context.drawing_description = zelda3::ObjectRenderRoutingDisplayLabel(
      zelda3::GetEffectiveObjectLayerSemantics(object));
  const auto& colors = room.rendered_palette();
  const auto blocks = room.blocks();
  std::map<std::pair<uint16_t, uint8_t>, size_t> target_indices;
  for (const auto& tile : *tiles) {
    if (tile.id_ >= 512 || tile.palette_ >= 8 ||
        (tile.id_ >= 0x1B0 && tile.id_ < 0x1D0)) {
      context.limitation =
          "Animated or unmapped tiles are omitted from pixel editing.";
      continue;
    }
    const uint8_t orientation =
        (tile.horizontal_mirror_ ? 1 : 0) | (tile.vertical_mirror_ ? 2 : 0);
    const uint8_t orientation_bit = 1 << orientation;
    const auto [entry, inserted] = target_indices.emplace(
        std::make_pair(tile.id_, tile.palette_), context.targets.size());
    if (!inserted) {
      context.targets[entry->second].source_orientation_mask |= orientation_bit;
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
        .source_orientation_mask = orientation_bit,
    });
  }
  if (context.targets.empty()) {
    return absl::UnimplementedError(
        "This object's tiles do not yet have an editable graphics source");
  }
  return context;
}

}  // namespace yaze::editor

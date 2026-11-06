#include "zelda3/overworld/overworld.h"

#include <algorithm>

#include "util/macro.h"

namespace yaze::zelda3 {
absl::StatusOr<OverworldSpriteEdits> Overworld::PrepareSpriteEdits() const {
  if (!rom_)
    return absl::FailedPreconditionError("No ROM for overworld sprites");
  if (materialized_map_count() > 192)
    return absl::FailedPreconditionError("Unsupported sprite map capacity");
  OverworldSpriteEdits edits;
  bool loaded = false;
  const auto& baseline = native_nonterrain_baseline_[4];
  size_t baseline_cursor = 0;
  for (int state = 0; state < 3; ++state) {
    const size_t old_count =
        baseline_cursor < baseline.size()
            ? static_cast<size_t>(baseline[baseline_cursor++])
            : 0;
    const size_t old_begin = baseline_cursor;
    baseline_cursor += old_count * 7;
    size_t row = 0;
    for (int map = 0; map < materialized_map_count(); ++map) {
      if (sprite_maps_loaded_[state][map]) {
        edits[state][map] = OverworldSpriteBytes{};
        loaded = true;
      }
    }
    for (const auto& sprite : all_sprites_[state]) {
      const size_t old = old_begin + row++ * 7;
      // Existing bank09 data can contain offscreen legacy placements.
      // Preserve an unchanged prefix position; new or moved positions must fit.
      const bool retained_position =
          sprite.map_id() < 160 && row <= old_count &&
          old + 6 < baseline.size() && baseline[old] == sprite.map_id() &&
          baseline[old + 2] == sprite.x() && baseline[old + 3] == sprite.y();
      if (sprite.deleted())
        continue;
      const int map = sprite.map_id();
      if (map < 0 || map >= materialized_map_count() || !edits[state][map]) {
        return absl::FailedPreconditionError(
            "Sprite belongs to an unloaded or unsupported map/game state");
      }
      // World-space position is authoritative: drag/property edits do not
      // update Sprite::nx()/ny(). Encode relative to the owning parent map.
      const int x = sprite.x() - (map % 8) * 512;
      const int y = sprite.y() - ((map % 64) / 8) * 512;
      if (x < 0 || x >= 1024 || y < 0 || y >= 1024 || x % 16 || y % 16) {
        return absl::InvalidArgumentError(
            "Overworld sprite must be on a 16-pixel grid within its parent "
            "area");
      }
      if (descriptor_layout_.revision == 3) {
        const auto& area = overworld_maps_[map];
        const auto size = area.area_size();
        const int width =
            (size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::WideArea)
                ? 1024
                : 512;
        const int height =
            (size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::TallArea)
                ? 1024
                : 512;
        if (!retained_position &&
            (area.parent() != map || x >= width || y >= height))
          return absl::InvalidArgumentError(
              "Native sprite is outside its owning area");
      }
      auto& bytes = *edits[state][map];
      bytes.push_back(static_cast<uint8_t>(y / 16) |
                      sprite.overworld_y_flags());
      bytes.push_back(static_cast<uint8_t>(x / 16) |
                      sprite.overworld_x_flags());
      bytes.push_back(sprite.id());
    }
    for (auto& bytes : edits[state])
      if (bytes)
        bytes->push_back(0xFF);
  }
  if (!loaded)
    return OverworldSpriteEdits{};
  return edits;
}

absl::StatusOr<std::vector<rom::NativeOverworldSpriteEdit>>
Overworld::PrepareNativeSpriteSave() const {
  ASSIGN_OR_RETURN(auto edits, PrepareSpriteEdits());
  std::vector<rom::NativeOverworldSpriteEdit> result;
  for (uint16_t phase = 0; phase < 3; ++phase)
    for (uint16_t map = 0; map < edits[phase].size(); ++map)
      if (edits[phase][map])
        result.push_back({phase, map, std::move(*edits[phase][map])});
  return result;
}

absl::StatusOr<OverworldSpriteSavePlan> Overworld::PrepareSpriteSave() const {
  ASSIGN_OR_RETURN(auto edits, PrepareSpriteEdits());
  if (std::none_of(edits.begin(), edits.end(), [](const auto& phase) {
        return std::any_of(phase.begin(), phase.end(),
                           [](const auto& entry) { return entry.has_value(); });
      }))
    return OverworldSpriteSavePlan{};
  return PlanOverworldSpriteSave(*rom_, edits);
}

}  // namespace yaze::zelda3

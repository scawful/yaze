#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <limits>
#include <set>

#include "rom/native_overworld_authoring_metadata.h"
#include "util/macro.h"

namespace yaze::zelda3 {

absl::Status Overworld::ReserveNativeAuthoringSpritePlacement(int phase,
                                                              int map_id,
                                                              Sprite* sprite) {
  if (!descriptor_layout_.has_authoring_metadata)
    return absl::OkStatus();
  if (!sprite || !rom_ || phase < 0 || phase >= 3 || map_id < 0 ||
      map_id >= materialized_map_count() || sprite->map_id() != map_id ||
      overworld_maps_[map_id].parent() != map_id ||
      !sprite_maps_loaded_[phase][map_id] || sprite->placement_id() ||
      sprite->deleted())
    return absl::FailedPreconditionError(
        "Load the owning sprite list before inserting a placement");
  RETURN_IF_ERROR(rom_->CheckLayoutSaveable());
  ASSIGN_OR_RETURN(auto draft, ProposeNativeAuthoringMetadataForSave());
  if (draft.placements.size() >= rom::kNativeAuthoringMetadataSlots)
    return absl::ResourceExhaustedError(
        "Native authoring metadata has no free slots");
  if (draft.next_placement_id == std::numeric_limits<uint32_t>::max())
    return absl::ResourceExhaustedError(
        "Native placement identity allocation is exhausted");
  const auto size = overworld_maps_[map_id].area_size();
  const int width =
      (size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::WideArea) ? 1024
                                                                          : 512;
  const int height =
      (size == AreaSizeEnum::LargeArea || size == AreaSizeEnum::TallArea) ? 1024
                                                                          : 512;
  const int x = sprite->x() - (map_id % 8) * 512;
  const int y = sprite->y() - ((map_id % 64) / 8) * 512;
  if (x < 0 || y < 0 || x >= width || y >= height || x % 16 || y % 16)
    return absl::InvalidArgumentError(
        "Native placement must fit its owning area's16-pixel grid");
  const size_t row =
      std::count_if(all_sprites_[phase].begin(), all_sprites_[phase].end(),
                    [map_id](const auto& value) {
                      return value.map_id() == map_id && !value.deleted();
                    });
  if (row >= 128)
    return absl::ResourceExhaustedError("Native sprite list is full");
  const uint32_t id = draft.next_placement_id++;
  draft.placements.push_back(
      {id, rom::NativePlacementState::kBound,
       rom::NativeAuthoringSpritePlacement{
           static_cast<uint8_t>(phase),
           static_cast<uint8_t>(map_id),
           static_cast<uint8_t>(row),
           {static_cast<uint8_t>((y / 16) | sprite->overworld_y_flags()),
            static_cast<uint8_t>((x / 16) | sprite->overworld_x_flags()),
            sprite->id()}}});
  const auto encoded = rom::EncodeNativeOverworldAuthoringMetadata(
      draft, materialized_map_count());
  if (!encoded.ok())
    return encoded.status();
  native_authoring_metadata_draft_ = std::move(draft);
  sprite->set_placement_id(id);
  return absl::OkStatus();
}

absl::StatusOr<rom::NativeOverworldAuthoringMetadata>
Overworld::ProposeNativeAuthoringMetadataForSave() const {
  if (!descriptor_layout_.has_authoring_metadata ||
      !native_authoring_metadata_draft_ || !native_authoring_metadata_baseline_)
    return absl::FailedPreconditionError(
        "Native authoring metadata is not loaded");
  ASSIGN_OR_RETURN(auto streams, PrepareSpriteEdits());
  auto out = *native_authoring_metadata_draft_;
  std::set<uint32_t> model_ids;
  for (int phase = 0; phase < 3; ++phase) {
    std::array<size_t, kExpandedMapCount> ordinal{};
    for (const auto& sprite : all_sprites_[phase]) {
      const int map = sprite.map_id();
      if (map < 0 || map >= materialized_map_count())
        return absl::FailedPreconditionError(
            "Sprite references an invalid owner");
      // Every emitted row contributes to the ordinal, including untracked
      // legacy actors. Read the packed witness from that same stream plan.
      const size_t row = ordinal[map];
      if (!sprite.deleted())
        ++ordinal[map];
      if (!sprite.placement_id())
        continue;
      if (!model_ids.insert(*sprite.placement_id()).second)
        return absl::FailedPreconditionError(
            "Duplicate native placement identity in sprite model");
      auto found = std::find_if(
          out.placements.begin(), out.placements.end(),
          [&](const auto& p) { return p.id == *sprite.placement_id(); });
      if (found == out.placements.end() ||
          !std::holds_alternative<rom::NativeAuthoringSpritePlacement>(
              found->value))
        return absl::FailedPreconditionError(
            "Sprite placement identity is missing or has another kind");
      auto& value = std::get<rom::NativeAuthoringSpritePlacement>(found->value);
      if (value.phase != phase)
        return absl::FailedPreconditionError(
            "Native placement cannot change its game phase");
      if (sprite.deleted()) {
        found->state = rom::NativePlacementState::kDeleted;
        const auto saved = std::find_if(
            native_authoring_metadata_baseline_->placements.begin(),
            native_authoring_metadata_baseline_->placements.end(),
            [&](const auto& p) { return p.id == found->id; });
        // Retain the last saved typed locator/witness. A newly allocated draft
        // with no saved row keeps its original reservation witness.
        if (saved != native_authoring_metadata_baseline_->placements.end())
          found->value = saved->value;
        continue;
      }
      if (!streams[phase][map] || row >= 128 ||
          row * 3 + 3 >= streams[phase][map]->size())
        return absl::FailedPreconditionError(
            "Native placement has no matching proposed sprite row");
      const auto& bytes = *streams[phase][map];
      found->state = rom::NativePlacementState::kBound;
      value = {static_cast<uint8_t>(phase),
               static_cast<uint8_t>(map),
               static_cast<uint8_t>(row),
               {bytes[row * 3], bytes[row * 3 + 1], bytes[row * 3 + 2]}};
    }
  }
  for (const auto& placement : out.placements) {
    const auto* sprite =
        std::get_if<rom::NativeAuthoringSpritePlacement>(&placement.value);
    if (sprite && placement.state == rom::NativePlacementState::kBound &&
        sprite_maps_loaded_[sprite->phase][sprite->owner_map] &&
        !model_ids.contains(placement.id))
      return absl::FailedPreconditionError(
          "Bound placement has no matching loaded model identity");
  }
  RETURN_IF_ERROR(rom::ValidateNativeAuthoringSpriteTransition(
      *native_authoring_metadata_baseline_, out, materialized_map_count()));
  return out;
}

}  // namespace yaze::zelda3

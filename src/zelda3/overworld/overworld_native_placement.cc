#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>

#include "rom/native_overworld_authoring_metadata.h"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {

uint16_t PlacementSchema(const rom::NativeOverworldAuthoringMetadata& metadata,
                         uint16_t saved_schema) {
  const bool bound_endpoint = std::any_of(
      metadata.placements.begin(), metadata.placements.end(),
      [](const auto& p) {
        return p.state == rom::NativePlacementState::kBound &&
               !std::holds_alternative<rom::NativeAuthoringSpritePlacement>(
                   p.value);
      });
  return std::max<uint16_t>(saved_schema, bound_endpoint ? 2 : 1);
}

template <typename Record>
absl::StatusOr<rom::NativeAuthoringPlacement> EndpointModelPlacement(
    const Overworld& world, const rom::NativeAuthoringPlacement& identity,
    const Record& record, size_t row) {
  const int map = record.map_id_;
  const auto* area = world.overworld_map(map);
  if (!area || area->parent() != map || map >= world.materialized_map_count())
    return absl::FailedPreconditionError(
        "Endpoint must belong to a loaded parent area");
  const auto shape = area->area_size();
  const int width =
      (shape == AreaSizeEnum::LargeArea || shape == AreaSizeEnum::WideArea)
          ? 1024
          : 512;
  const int height =
      (shape == AreaSizeEnum::LargeArea || shape == AreaSizeEnum::TallArea)
          ? 1024
          : 512;
  const int x = record.x_ - (map % 8) * 512;
  const int y = record.y_ - ((map % 64) / 8) * 512;
  if (x < 0 || y < 0 || x >= width || y >= height || record.game_x_ != x / 16 ||
      record.game_y_ != y / 16)
    return absl::InvalidArgumentError(
        "Endpoint position is outside its owning area");
  const int page = (map / 64) * 64 + record.x_ / 512 + (record.y_ / 512) * 8;
  const auto* physical = world.overworld_map(page);
  if (!physical || physical->parent() != map)
    return absl::FailedPreconditionError(
        "Endpoint physical page does not belong to its parent");
  auto result = identity;
  result.state = rom::NativePlacementState::kBound;
  const auto px = static_cast<uint8_t>((record.x_ % 512) / 16);
  const auto py = static_cast<uint8_t>((record.y_ % 512) / 16);
  if constexpr (std::is_same_v<Record, OverworldEntrance>) {
    if (record.deleted || record.is_hole_ || x % 16 || y % 16 ||
        record.map_pos_ != ((y / 16) << 7 | (x / 16) << 1))
      return absl::FailedPreconditionError(
          "Select a live encoded door row before binding");
    result.value = rom::NativeAuthoringDoorPlacement{
        static_cast<uint8_t>(page), px, py, record.entrance_id_,
        static_cast<uint16_t>(row)};
  } else {
    if (record.deleted_ || record.x_player_ != record.x_ ||
        record.y_player_ != record.y_)
      return absl::FailedPreconditionError(
          "Select a live encoded return row before binding");
    result.value = rom::NativeAuthoringReturnPlacement{
        static_cast<uint8_t>(page), px, py, record.room_id_,
        static_cast<uint16_t>(row)};
  }
  return result;
}

}  // namespace

absl::Status Overworld::BindNativeEndpointPlacements() {
  if (!native_authoring_metadata_draft_)
    return absl::OkStatus();
  const auto& metadata = *native_authoring_metadata_draft_;
  RETURN_IF_ERROR(
      rom::ValidateNativeEndpointPlacementWitness(rom_->vector(), metadata));
  // Verify the complete model set before assigning any identity.
  for (const auto& p : metadata.placements) {
    if (p.state != rom::NativePlacementState::kBound)
      continue;
    if (const auto* door =
            std::get_if<rom::NativeAuthoringDoorPlacement>(&p.value)) {
      if (*door->trigger_row >= all_entrances_.size())
        return absl::FailedPreconditionError("Bound door row is not loaded");
      ASSIGN_OR_RETURN(
          auto model,
          EndpointModelPlacement(*this, p, all_entrances_[*door->trigger_row],
                                 *door->trigger_row));
      if (model != p)
        return absl::FailedPreconditionError(
            "Bound door model witness differs");
    } else if (const auto* back =
                   std::get_if<rom::NativeAuthoringReturnPlacement>(&p.value)) {
      if (*back->return_row >= all_exits_.size())
        return absl::FailedPreconditionError("Bound return row is not loaded");
      ASSIGN_OR_RETURN(auto model, EndpointModelPlacement(
                                       *this, p, all_exits_[*back->return_row],
                                       *back->return_row));
      if (model != p)
        return absl::FailedPreconditionError(
            "Bound return model witness differs");
    }
  }
  for (const auto& p : metadata.placements) {
    if (p.state != rom::NativePlacementState::kBound)
      continue;
    if (const auto* door =
            std::get_if<rom::NativeAuthoringDoorPlacement>(&p.value))
      all_entrances_[*door->trigger_row].authoring_placement_id = p.id;
    else if (const auto* back =
                 std::get_if<rom::NativeAuthoringReturnPlacement>(&p.value))
      all_exits_[*back->return_row].authoring_placement_id = p.id;
  }
  return absl::OkStatus();
}

absl::Status Overworld::RestoreNativeEndpointPlacement(
    const rom::NativeAuthoringPlacement& placement) {
  if (!rom_ || !native_authoring_metadata_draft_ ||
      !native_authoring_metadata_baseline_)
    return absl::FailedPreconditionError("Native placements are not loaded");
  RETURN_IF_ERROR(rom_->CheckLayoutSaveable());
  auto candidate = *native_authoring_metadata_draft_;
  auto found =
      std::find_if(candidate.placements.begin(), candidate.placements.end(),
                   [&](const auto& p) { return p.id == placement.id; });
  if (found == candidate.placements.end() ||
      found->value.index() != placement.value.index() ||
      std::holds_alternative<rom::NativeAuthoringSpritePlacement>(
          placement.value))
    return absl::InvalidArgumentError("Select an existing endpoint identity");
  *found = placement;
  candidate.schema_version = PlacementSchema(
      candidate, native_authoring_metadata_baseline_->schema_version);
  const auto encoded = rom::EncodeNativeOverworldAuthoringMetadata(
      candidate, materialized_map_count());
  if (!encoded.ok())
    return encoded.status();
  auto check = [&](const auto& records, auto row) -> absl::Status {
    if (!row || *row >= records.size() ||
        (records[*row].authoring_placement_id &&
         records[*row].authoring_placement_id != placement.id))
      return absl::FailedPreconditionError(
          "Endpoint row is unavailable or bound");
    ASSIGN_OR_RETURN(auto model, EndpointModelPlacement(*this, placement,
                                                        records[*row], *row));
    if (model != placement)
      return absl::FailedPreconditionError(
          "Endpoint binding differs from its row");
    return absl::OkStatus();
  };
  if (placement.state == rom::NativePlacementState::kBound) {
    if (const auto* door =
            std::get_if<rom::NativeAuthoringDoorPlacement>(&placement.value)) {
      RETURN_IF_ERROR(check(all_entrances_, door->trigger_row));
    } else {
      const auto& back =
          std::get<rom::NativeAuthoringReturnPlacement>(placement.value);
      RETURN_IF_ERROR(check(all_exits_, back.return_row));
    }
  }
  for (auto& e : all_entrances_)
    if (e.authoring_placement_id == placement.id)
      e.authoring_placement_id.reset();
  for (auto& e : all_exits_)
    if (e.authoring_placement_id == placement.id)
      e.authoring_placement_id.reset();
  if (placement.state == rom::NativePlacementState::kBound) {
    if (const auto* door =
            std::get_if<rom::NativeAuthoringDoorPlacement>(&placement.value))
      all_entrances_[*door->trigger_row].authoring_placement_id = placement.id;
    else
      all_exits_[*std::get<rom::NativeAuthoringReturnPlacement>(placement.value)
                      .return_row]
          .authoring_placement_id = placement.id;
  }
  native_authoring_metadata_draft_ = std::move(candidate);
  return absl::OkStatus();
}

absl::Status Overworld::BindNativeEndpointPlacement(uint32_t id, uint16_t row) {
  if (!native_authoring_metadata_draft_)
    return absl::FailedPreconditionError("Native placements are not loaded");
  const auto& records = native_authoring_metadata_draft_->placements;
  const auto found = std::find_if(records.begin(), records.end(),
                                  [id](const auto& p) { return p.id == id; });
  if (found == records.end() ||
      found->state != rom::NativePlacementState::kPending)
    return absl::FailedPreconditionError("Select a pending endpoint placement");
  auto check_owner = [&](uint8_t page, int parent) -> absl::Status {
    const auto* planned = overworld_map(page);
    if (!planned || planned->parent() != parent)
      return absl::FailedPreconditionError(
          "Move the selected row into the planned area before binding");
    return absl::OkStatus();
  };
  if (const auto* door =
          std::get_if<rom::NativeAuthoringDoorPlacement>(&found->value)) {
    if (row >= all_entrances_.size())
      return absl::OutOfRangeError("Door row is outside the current table");
    const auto& e = all_entrances_[row];
    if (door->entrance_id && *door->entrance_id != e.entrance_id_)
      return absl::FailedPreconditionError(
          "Door target does not match the plan");
    RETURN_IF_ERROR(check_owner(door->map, e.map_id_));
    ASSIGN_OR_RETURN(auto bound, EndpointModelPlacement(*this, *found, e, row));
    return RestoreNativeEndpointPlacement(bound);
  }
  if (const auto* back =
          std::get_if<rom::NativeAuthoringReturnPlacement>(&found->value)) {
    if (row >= all_exits_.size())
      return absl::OutOfRangeError("Return row is outside the current table");
    const auto& e = all_exits_[row];
    if (back->room_id && *back->room_id != e.room_id_)
      return absl::FailedPreconditionError(
          "Return room does not match the plan");
    RETURN_IF_ERROR(check_owner(back->map, e.map_id_));
    ASSIGN_OR_RETURN(auto bound, EndpointModelPlacement(*this, *found, e, row));
    return RestoreNativeEndpointPlacement(bound);
  }
  return absl::InvalidArgumentError("Selected placement is not an endpoint");
}

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
  ASSIGN_OR_RETURN(auto draft,
                   ProposeNativeAuthoringMetadataForSave(true, false, false));
  // Reserving a sprite must retain endpoint drafts without trying to publish
  // or validate unrelated endpoint model edits.
  for (auto& p : draft.placements) {
    if (std::holds_alternative<rom::NativeAuthoringSpritePlacement>(p.value))
      continue;
    const auto old =
        std::find_if(native_authoring_metadata_draft_->placements.begin(),
                     native_authoring_metadata_draft_->placements.end(),
                     [&](const auto& value) { return value.id == p.id; });
    p = *old;
  }
  draft.schema_version = PlacementSchema(
      draft, native_authoring_metadata_baseline_->schema_version);
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
Overworld::ProposeNativeAuthoringMetadataForSave(bool save_sprites,
                                                 bool save_entrances,
                                                 bool save_returns) const {
  if (!descriptor_layout_.has_authoring_metadata ||
      !native_authoring_metadata_draft_ || !native_authoring_metadata_baseline_)
    return absl::FailedPreconditionError(
        "Native authoring metadata is not loaded");
  auto out = save_sprites ? *native_authoring_metadata_draft_
                          : *native_authoring_metadata_baseline_;
  for (auto& p : out.placements) {
    if (std::holds_alternative<rom::NativeAuthoringSpritePlacement>(p.value))
      continue;
    const bool enabled = p.value.index() == 1 ? save_entrances : save_returns;
    const auto& source = enabled ? *native_authoring_metadata_draft_
                                 : *native_authoring_metadata_baseline_;
    const auto found =
        std::find_if(source.placements.begin(), source.placements.end(),
                     [&](const auto& value) { return value.id == p.id; });
    if (found == source.placements.end())
      return absl::FailedPreconditionError(
          "Endpoint placement identity is gone");
    p = *found;
  }
  if (save_sprites) {
    ASSIGN_OR_RETURN(auto streams, PrepareSpriteEdits());
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
        auto& value =
            std::get<rom::NativeAuthoringSpritePlacement>(found->value);
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
  }
  RETURN_IF_ERROR(
      ProposeNativeEndpointPlacements(&out, save_entrances, save_returns));
  out.schema_version =
      PlacementSchema(out, native_authoring_metadata_baseline_->schema_version);
  ASSIGN_OR_RETURN(auto endpoints,
                   PrepareNativeEndpointSave(save_entrances, save_returns));
  RETURN_IF_ERROR(rom::ValidateNativeAuthoringEndpointTransition(
      *native_authoring_metadata_baseline_, out, materialized_map_count(),
      endpoints));
  return out;
}

absl::Status Overworld::ProposeNativeEndpointPlacements(
    rom::NativeOverworldAuthoringMetadata* metadata, bool save_entrances,
    bool save_returns) const {
  auto update = [&](auto& records, size_t kind) -> absl::Status {
    std::set<uint32_t> identities;
    for (const auto& e : records) {
      if (!e.authoring_placement_id)
        continue;
      const auto id = *e.authoring_placement_id;
      const auto p =
          std::find_if(metadata->placements.begin(), metadata->placements.end(),
                       [id](const auto& p) { return p.id == id; });
      if (!identities.insert(id).second || p == metadata->placements.end() ||
          p->value.index() + 1 != kind)
        return absl::FailedPreconditionError(
            "Endpoint model has a duplicate, missing or wrong-kind identity");
    }
    for (auto& p : metadata->placements) {
      if (p.value.index() + 1 != kind)
        continue;
      const auto saved =
          std::find_if(native_authoring_metadata_baseline_->placements.begin(),
                       native_authoring_metadata_baseline_->placements.end(),
                       [&](const auto& value) { return value.id == p.id; });
      const auto row = std::find_if(
          records.begin(), records.end(),
          [&](const auto& e) { return e.authoring_placement_id == p.id; });
      bool deleted = row == records.end();
      if (row != records.end()) {
        if constexpr (std::is_same_v<
                          typename std::decay_t<decltype(records)>::value_type,
                          OverworldEntrance>)
          deleted = row->deleted;
        else
          deleted = row->deleted_;
      }
      if (deleted) {
        if (p.state == rom::NativePlacementState::kPending &&
            row == records.end())
          continue;  // An explicit binding Undo restored the authored plan.
        if (saved == native_authoring_metadata_baseline_->placements.end())
          return absl::FailedPreconditionError(
              "Endpoint has no saved identity");
        p = *saved;
        // A binding deleted before its first Save never became a live saved
        // identity. Keep the original pending plan rather than publish it.
        if (saved->state != rom::NativePlacementState::kPending)
          p.state = rom::NativePlacementState::kDeleted;
        continue;
      }
      ASSIGN_OR_RETURN(
          p, EndpointModelPlacement(
                 *this, p, *row, static_cast<size_t>(row - records.begin())));
    }
    return absl::OkStatus();
  };
  if (save_entrances)
    RETURN_IF_ERROR(update(all_entrances_, 2));
  if (save_returns)
    RETURN_IF_ERROR(update(all_exits_, 3));
  return absl::OkStatus();
}

bool Overworld::HasPendingNativeAuthoringMetadataChanges(size_t kind) const {
  if (!native_authoring_metadata_draft_ || !native_authoring_metadata_baseline_)
    return native_authoring_metadata_draft_.has_value() !=
           native_authoring_metadata_baseline_.has_value();
  const auto& draft = *native_authoring_metadata_draft_;
  const auto& saved = *native_authoring_metadata_baseline_;
  if (kind == 1 && draft.next_placement_id != saved.next_placement_id)
    return true;
  auto a = draft.placements.begin();
  auto b = saved.placements.begin();
  auto matches = [kind](const auto& p) {
    return p.value.index() + 1 == kind;
  };
  for (;;) {
    a = std::find_if(a, draft.placements.end(), matches);
    b = std::find_if(b, saved.placements.end(), matches);
    if (a == draft.placements.end() || b == saved.placements.end())
      return (a == draft.placements.end()) != (b == saved.placements.end());
    if (*a != *b)
      return true;
    ++a;
    ++b;
  }
}

}  // namespace yaze::zelda3

#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <span>

#include "util/macro.h"

namespace yaze::zelda3 {
namespace {
constexpr size_t kEntranceSnapshotFields = 10;
constexpr size_t kReturnSnapshotFields = 21;

rom::NativeOverworldEntranceRow EntranceRow(std::span<const int64_t> row,
                                            bool expanded) {
  const bool deleted = row[4];
  const uint16_t map =
      expanded && !deleted ? (row[0] & 0x7FFF) | (row[3] ? 0x8000 : 0) : row[0];
  return {map, static_cast<uint16_t>(deleted ? 0xFFFF : row[1]),
          static_cast<uint8_t>(row[2])};
}

rom::NativeOverworldReturnRow ReturnRow(std::span<const int64_t> row) {
  const bool deleted = row[14];
  return {static_cast<uint16_t>(row[1]),
          static_cast<uint8_t>(row[0]),
          static_cast<uint16_t>(row[2]),
          static_cast<uint16_t>(row[3]),
          static_cast<uint16_t>(row[4]),
          static_cast<uint16_t>(deleted ? 0xFFFF : row[5]),
          static_cast<uint16_t>(deleted ? 0xFFFF : row[6]),
          static_cast<uint16_t>(row[7]),
          static_cast<uint16_t>(row[8]),
          static_cast<uint8_t>(row[9]),
          static_cast<uint8_t>(row[10]),
          static_cast<uint16_t>(row[11]),
          static_cast<uint16_t>(row[12])};
}
}  // namespace

Overworld::NativeEndpointState Overworld::CaptureNativeEndpointState() const {
  NativeEndpointState state;
  auto add = [&](int group, std::initializer_list<int64_t> values) {
    state[group].insert(state[group].end(), values.begin(), values.end());
  };
  for (const auto* list : {&all_entrances_, &all_holes_}) {
    add(0, {static_cast<int64_t>(list->size())});
    for (const auto& e : *list)
      add(0,
          {e.map_id_, e.map_pos_, e.entrance_id_, e.is_hole_, e.deleted, e.x_,
           e.y_, e.game_x_, e.game_y_, e.authoring_placement_id.value_or(0)});
  }
  add(1, {static_cast<int64_t>(all_exits_.size())});
  for (const auto& e : all_exits_)
    add(1, {e.map_id_,
            e.room_id_,
            e.map_pos_,
            e.y_scroll_,
            e.x_scroll_,
            e.y_player_,
            e.x_player_,
            e.y_camera_,
            e.x_camera_,
            e.scroll_mod_y_,
            e.scroll_mod_x_,
            e.door_type_1_,
            e.door_type_2_,
            e.is_hole_,
            e.deleted_,
            e.is_automatic_,
            e.x_,
            e.y_,
            e.game_x_,
            e.game_y_,
            e.authoring_placement_id.value_or(0)});
  return state;
}

bool Overworld::HasPendingNativeEndpointChanges() const {
  if (!loaded_map_tiles_valid_ ||
      !rom::IsNativeSpecialOverworldLayout(descriptor_layout_))
    return false;
  const auto state = CaptureNativeEndpointState();
  return state[0] != native_nonterrain_baseline_[1] ||
         state[1] != native_nonterrain_baseline_[2] ||
         HasPendingNativeAuthoringMetadataChanges(2) ||
         HasPendingNativeAuthoringMetadataChanges(3);
}

absl::StatusOr<rom::NativeOverworldEndpointEdits>
Overworld::PrepareNativeEndpointSave(bool save_entrances,
                                     bool save_returns) const {
  rom::NativeOverworldEndpointEdits edits;
  const auto current = CaptureNativeEndpointState();
  auto has_bound = [&](size_t kind) {
    for (const auto* metadata : {&native_authoring_metadata_baseline_,
                                 &native_authoring_metadata_draft_})
      if (*metadata &&
          std::any_of((*metadata)->placements.begin(),
                      (*metadata)->placements.end(), [kind](const auto& p) {
                        return p.value.index() + 1 == kind &&
                               p.state == rom::NativePlacementState::kBound;
                      }))
        return true;
    return false;
  };
  if (save_entrances &&
      (current[0] != native_nonterrain_baseline_[1] || has_bound(2))) {
    const auto& saved = native_nonterrain_baseline_[1];
    const size_t count = all_entrances_.size();
    const size_t end = 1 + count * kEntranceSnapshotFields;
    if (saved.size() < end + 1 || saved[0] != static_cast<int64_t>(count) ||
        current[0].size() != saved.size())
      return absl::FailedPreconditionError(
          "Native entrance row identity changed after load");
    if (!std::equal(saved.begin() + end, saved.end(), current[0].begin() + end))
      return absl::FailedPreconditionError(
          "Native Save does not publish edits to the legacy hole table");
    const auto& options = layout_inspect_options_;
    if (options.entrance_expanded_flag_pc.value_or(
            options.Address(core::RomAddressKey::kOverworldEntranceFlagExpanded,
                            kOverworldEntranceExpandedFlagPos)) !=
        kOverworldEntranceExpandedFlagPos)
      return absl::FailedPreconditionError(
          "Native endpoint Save does not support a relocated entrance flag");
    const bool expanded = resolved_layout_.has_expanded_entrances;
    if (rom_->size() <= kOverworldEntranceExpandedFlagPos ||
        (rom_->vector()[kOverworldEntranceExpandedFlagPos] != 0xB8) != expanded)
      return absl::FailedPreconditionError(
          "Native entrance storage changed after load");
    if (expanded &&
        (options.Address(core::RomAddressKey::kOverworldEntranceMapExpanded,
                         kOverworldEntranceMapExpanded) !=
             kOverworldEntranceMapExpanded ||
         options.Address(core::RomAddressKey::kOverworldEntrancePosExpanded,
                         kOverworldEntrancePosExpanded) !=
             kOverworldEntrancePosExpanded ||
         options.Address(core::RomAddressKey::kOverworldEntranceIdExpanded,
                         kOverworldEntranceEntranceIdExpanded) !=
             kOverworldEntranceEntranceIdExpanded))
      return absl::FailedPreconditionError(
          "Native endpoint Save does not support relocated entrance tables");
    if (count != (expanded ? 256 : 129))
      return absl::FailedPreconditionError("Native entrance capacity changed");
    for (size_t row = 0; row < count; ++row) {
      const size_t at = 1 + row * kEntranceSnapshotFields;
      if (std::equal(saved.begin() + at,
                     saved.begin() + at + kEntranceSnapshotFields,
                     current[0].begin() + at))
        continue;
      if (!current[0][at + 4] &&
          (current[0][at] < 0 || current[0][at] >= materialized_map_count()))
        return absl::InvalidArgumentError(
            "Native entrance model map is outside the materialized domain");
      const auto expected = EntranceRow(
          std::span<const int64_t>(saved).subspan(at, kEntranceSnapshotFields),
          expanded);
      const auto value = EntranceRow(std::span<const int64_t>(current[0])
                                         .subspan(at, kEntranceSnapshotFields),
                                     expanded);
      if (expected != value) {
        if (value.position != 0xFFFF) {
          const int x = (value.position >> 1) & 63;
          const int y = value.position >> 7;
          const int map = expanded ? value.map_word & 0x7FFF : value.map_word;
          if (current[0][at + 5] != (map % 8) * 512 + x * 16 ||
              current[0][at + 6] != ((map % 64) / 8) * 512 + y * 16 ||
              current[0][at + 7] != x || current[0][at + 8] != y)
            return absl::FailedPreconditionError(
                "Native entrance position differs from its encoded row");
        }
        edits.entrances.push_back(
            {static_cast<uint16_t>(row), expected, value});
      } else if (!std::equal(saved.begin() + at,
                             saved.begin() + at + kEntranceSnapshotFields - 1,
                             current[0].begin() + at)) {
        return absl::FailedPreconditionError(
            "Native entrance edit has no serialized position change");
      }
    }
  }
  if (save_returns &&
      (current[1] != native_nonterrain_baseline_[2] || has_bound(3))) {
    const auto& saved = native_nonterrain_baseline_[2];
    const size_t count = all_exits_.size();
    if (count != kNumOverworldExits ||
        saved.size() != 1 + count * kReturnSnapshotFields ||
        saved[0] != static_cast<int64_t>(count) ||
        current[1].size() != saved.size())
      return absl::FailedPreconditionError(
          "Native return row identity changed after load");
    for (size_t row = 0; row < count; ++row) {
      const size_t at = 1 + row * kReturnSnapshotFields;
      if (std::equal(saved.begin() + at,
                     saved.begin() + at + kReturnSnapshotFields,
                     current[1].begin() + at))
        continue;
      const auto map = current[1][at];
      const bool deleted = current[1][at + 14];
      if (map < 0 || map > 0xFF ||
          (!deleted && map >= materialized_map_count()))
        return absl::InvalidArgumentError(
            "Native return model map cannot be represented in its runtime row");
      if (saved[at + 13] != current[1][at + 13])
        return absl::FailedPreconditionError(
            "Native return edit cannot change its record kind");
      const auto expected = ReturnRow(
          std::span<const int64_t>(saved).subspan(at, kReturnSnapshotFields));
      const auto value = ReturnRow(std::span<const int64_t>(current[1])
                                       .subspan(at, kReturnSnapshotFields));
      if (!deleted) {
        const int x = value.player_x - (map % 8) * 512;
        const int y = value.player_y - ((map % 64) / 8) * 512;
        if (x < 0 || y < 0 || current[1][at + 16] != value.player_x ||
            current[1][at + 17] != value.player_y ||
            current[1][at + 18] != x / 16 || current[1][at + 19] != y / 16)
          return absl::FailedPreconditionError(
              "Native return position differs from its encoded row");
      }
      if (expected != value) {
        edits.returns.push_back({static_cast<uint16_t>(row), expected, value});
      } else {
        for (size_t field = 0; field < kReturnSnapshotFields - 1; ++field)
          if (field != 15 && saved[at + field] != current[1][at + field])
            return absl::FailedPreconditionError(
                "Native return edit has no serialized position change");
      }
    }
  }
  // A metadata-only bind/unbind still selects the complete runtime row. Use
  // the saved model snapshot for its preimage, never a caller-supplied address.
  for (const auto* metadata : {&native_authoring_metadata_baseline_,
                               &native_authoring_metadata_draft_}) {
    if (!*metadata)
      continue;
    for (const auto& p : (*metadata)->placements) {
      if (p.state != rom::NativePlacementState::kBound)
        continue;
      if (const auto* door =
              std::get_if<rom::NativeAuthoringDoorPlacement>(&p.value);
          door && save_entrances) {
        const auto row = *door->trigger_row;
        if (std::any_of(edits.entrances.begin(), edits.entrances.end(),
                        [row](const auto& e) { return e.row == row; }))
          continue;
        const size_t at = 1 + row * kEntranceSnapshotFields;
        if (row >= all_entrances_.size() ||
            native_nonterrain_baseline_[1].size() <
                at + kEntranceSnapshotFields)
          return absl::FailedPreconditionError("Bound door row is not loaded");
        const bool expanded = resolved_layout_.has_expanded_entrances;
        edits.entrances.push_back(
            {row,
             EntranceRow(
                 std::span<const int64_t>(native_nonterrain_baseline_[1])
                     .subspan(at, kEntranceSnapshotFields),
                 expanded),
             EntranceRow(std::span<const int64_t>(current[0])
                             .subspan(at, kEntranceSnapshotFields),
                         expanded)});
      } else if (const auto* back =
                     std::get_if<rom::NativeAuthoringReturnPlacement>(&p.value);
                 back && save_returns) {
        const auto row = *back->return_row;
        if (std::any_of(edits.returns.begin(), edits.returns.end(),
                        [row](const auto& e) { return e.row == row; }))
          continue;
        const size_t at = 1 + row * kReturnSnapshotFields;
        if (row >= all_exits_.size() ||
            native_nonterrain_baseline_[2].size() < at + kReturnSnapshotFields)
          return absl::FailedPreconditionError(
              "Bound return row is not loaded");
        edits.returns.push_back(
            {row,
             ReturnRow(std::span<const int64_t>(native_nonterrain_baseline_[2])
                           .subspan(at, kReturnSnapshotFields)),
             ReturnRow(std::span<const int64_t>(current[1])
                           .subspan(at, kReturnSnapshotFields))});
      }
    }
  }
  return edits;
}

}  // namespace yaze::zelda3

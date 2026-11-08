#include "rom/native_overworld_endpoint_io.h"

#include <algorithm>
#include <array>
#include <set>
#include <span>
#include <utility>

#include "rom/lyra_layout_descriptor.h"
#include "util/macro.h"

namespace yaze::rom {
namespace {
// Same runtime tables as OverworldEntrance and OverworldExit. These are not
// directory allocations and must never alias a relocated directory payload.
constexpr uint32_t kEntranceFlag = 0xDB895;
constexpr std::array<uint32_t, 3> kEntrances = {0xDB96F, 0xDBA71, 0xDBB73};
constexpr std::array<uint32_t, 3> kExpandedEntrances = {0xDB55F, 0xDB35F,
                                                        0xDB75F};
constexpr std::array<uint32_t, 13> kReturns = {
    0x15D8A, 0x15E28, 0x15E77, 0x15F15, 0x15FB3, 0x16051, 0x160EF,
    0x1618D, 0x1622B, 0x162C9, 0x16318, 0x16367, 0x16405};
constexpr std::array<uint8_t, 13> kReturnWidths = {2, 1, 2, 2, 2, 2, 2,
                                                   2, 2, 1, 1, 2, 2};
constexpr uint32_t kSpecialReturnMaps = 0x15724;
constexpr uint16_t kReturnCount = 0x4F;

std::array<uint16_t, 13> Fields(const NativeOverworldReturnRow& row) {
  return {row.room,       row.map,          row.vram,         row.scroll_y,
          row.scroll_x,   row.player_y,     row.player_x,     row.camera_y,
          row.camera_x,   row.scroll_mod_y, row.scroll_mod_x, row.door_type_1,
          row.door_type_2};
}
}  // namespace

absl::StatusOr<std::vector<NativeOverworldEndpointWrite>>
PlanNativeOverworldEndpointSave(const std::vector<uint8_t>& baseline,
                                const NativeOverworldEndpointEdits& edits,
                                std::span<const uint8_t> final_image) {
  const auto layout = InspectLyraLayout(baseline.data(), baseline.size());
  if (layout.presence != LyraLayoutPresence::kValid ||
      !IsNativeSpecialOverworldLayout(layout)) {
    return absl::FailedPreconditionError(
        "Native endpoint planning requires a valid native layout");
  }
  if (final_image.empty())
    final_image = baseline;
  if (final_image.size() != baseline.size())
    return absl::InvalidArgumentError(
        "Native endpoint geometry image extent differs from baseline");
  std::vector<NativeOverworldEndpointWrite> writes;
  std::set<uint32_t> occupied;
  auto check_span = [&](uint32_t pc, size_t length) -> absl::Status {
    if (pc > baseline.size() || length > baseline.size() - pc)
      return absl::OutOfRangeError("Native endpoint table is truncated");
    for (const auto& entry : layout.entries) {
      const size_t end = entry.pc_address + entry.count * entry.stride;
      if (pc < end && entry.pc_address < pc + length)
        return absl::PermissionDeniedError(
            "Native endpoint table aliases a directory allocation");
    }
    return absl::OkStatus();
  };
  auto stage = [&](uint32_t pc, uint16_t expected, uint16_t value,
                   uint8_t width) -> absl::Status {
    RETURN_IF_ERROR(check_span(pc, width));
    NativeOverworldEndpointWrite write{pc, {}};
    for (uint8_t i = 0; i < width; ++i) {
      if (baseline[pc + i] != static_cast<uint8_t>(expected >> (8 * i)))
        return absl::FailedPreconditionError(
            "Native endpoint row preimage changed");
      if (!occupied.insert(pc + i).second)
        return absl::InvalidArgumentError(
            "Duplicate or overlapping native endpoint edits");
      write.bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
    writes.push_back(std::move(write));
    return absl::OkStatus();
  };
  auto geometry = [&](uint16_t map) -> absl::StatusOr<std::array<uint16_t, 4>> {
    if (map >= layout.active_count)
      return absl::InvalidArgumentError(
          "Native endpoint map is outside the materialized domain");
    const auto parent = LyraLayoutRecord(layout, final_image.data(),
                                         final_image.size(), 7, map);
    if (!parent || (*parent)[0] != map)
      return absl::InvalidArgumentError(
          "Native endpoint must reference a parent area");
    const auto size = LyraLayoutRecord(layout, final_image.data(),
                                       final_image.size(), 30, map);
    if (!size || (*size)[0] > 3)
      return absl::InvalidArgumentError("Native endpoint area size is invalid");
    const uint8_t shape = (*size)[0];
    return std::array<uint16_t, 4>{
        static_cast<uint16_t>((map % 8) * 512),
        static_cast<uint16_t>(((map % 64) / 8) * 512),
        static_cast<uint16_t>((shape == 1 || shape == 2) ? 64 : 32),
        static_cast<uint16_t>((shape == 1 || shape == 3) ? 64 : 32)};
  };
  if (!edits.entrances.empty()) {
    RETURN_IF_ERROR(check_span(kEntranceFlag, 1));
    const bool expanded = baseline[kEntranceFlag] != 0xB8;
    const auto& tables = expanded ? kExpandedEntrances : kEntrances;
    const uint16_t count = expanded ? 256 : 129;
    for (size_t field = 0; field < tables.size(); ++field)
      RETURN_IF_ERROR(check_span(tables[field], count * (field == 2 ? 1 : 2)));
    for (const auto& edit : edits.entrances) {
      if (edit.row >= count)
        return absl::OutOfRangeError(
            "Native entrance row exceeds table capacity");
      if (edit.value.position != 0xFFFF) {
        ASSIGN_OR_RETURN(auto area,
                         geometry(expanded ? edit.value.map_word & 0x7FFF
                                           : edit.value.map_word));
        if ((edit.value.position & 1) || edit.value.position >= 0x2000)
          return absl::InvalidArgumentError(
              "Native entrance position is not a packed area tile");
        if (((edit.value.position >> 1) & 63) >= area[2] ||
            (edit.value.position >> 7) >= area[3])
          return absl::InvalidArgumentError(
              "Native entrance position is outside its area dimensions");
      }
      RETURN_IF_ERROR(stage(tables[0] + edit.row * 2, edit.expected.map_word,
                            edit.value.map_word, 2));
      RETURN_IF_ERROR(stage(tables[1] + edit.row * 2, edit.expected.position,
                            edit.value.position, 2));
      RETURN_IF_ERROR(stage(tables[2] + edit.row, edit.expected.entrance_id,
                            edit.value.entrance_id, 1));
    }
  }
  if (!edits.returns.empty()) {
    for (size_t field = 0; field < kReturns.size(); ++field)
      RETURN_IF_ERROR(
          check_span(kReturns[field], kReturnCount * kReturnWidths[field]));
    for (const auto& edit : edits.returns) {
      if (edit.row >= kReturnCount)
        return absl::OutOfRangeError(
            "Native return row exceeds table capacity");
      const bool deleted =
          edit.value.player_x == 0xFFFF && edit.value.player_y == 0xFFFF;
      if (!deleted) {
        ASSIGN_OR_RETURN(auto area, geometry(edit.value.map));
        if (edit.value.player_x == 0xFFFF || edit.value.player_y == 0xFFFF)
          return absl::InvalidArgumentError(
              "Native return deletion requires both player coordinates");
        if (edit.value.player_x < area[0] || edit.value.player_y < area[1] ||
            edit.value.player_x >= area[0] + area[2] * 16 ||
            edit.value.player_y >= area[1] + area[3] * 16)
          return absl::InvalidArgumentError(
              "Native return player position is outside its area");
      }
      const auto expected = Fields(edit.expected);
      const auto value = Fields(edit.value);
      for (size_t field = 0; field < kReturns.size(); ++field)
        RETURN_IF_ERROR(stage(kReturns[field] + edit.row * kReturnWidths[field],
                              expected[field], value[field],
                              kReturnWidths[field]));
      // Match the ordinary writer: preserve auxiliary bytes unless an authored
      // destination change selects one of the three special return rooms.
      if ((edit.expected.room != edit.value.room ||
           edit.expected.map != edit.value.map) &&
          edit.value.room >= 0x180 && edit.value.room <= 0x182) {
        const uint32_t pc = kSpecialReturnMaps + (edit.value.room - 0x180) * 2;
        RETURN_IF_ERROR(check_span(pc, 1));
        RETURN_IF_ERROR(stage(pc, baseline[pc], edit.value.map, 1));
      }
    }
  }
  return writes;
}

absl::Status ValidateNativeAuthoringEndpointTransition(
    const NativeOverworldAuthoringMetadata& expected,
    const NativeOverworldAuthoringMetadata& proposed,
    uint16_t materialized_count, const NativeOverworldEndpointEdits& edits) {
  const auto encoded =
      EncodeNativeOverworldAuthoringMetadata(proposed, materialized_count);
  if (!encoded.ok())
    return encoded.status();
  if (proposed.schema_version < expected.schema_version)
    return absl::FailedPreconditionError(
        "Native authoring metadata cannot downgrade its saved schema");
  auto sprite_only = proposed;
  sprite_only.schema_version = expected.schema_version;
  bool endpoint_changed = false;
  for (size_t i = 0;
       i < std::min(expected.placements.size(), proposed.placements.size());
       ++i) {
    const auto& old = expected.placements[i];
    const auto& value = proposed.placements[i];
    if (std::holds_alternative<NativeAuthoringSpritePlacement>(old.value))
      continue;
    if (old.id != value.id || old.value.index() != value.value.index())
      return absl::FailedPreconditionError(
          "Native endpoint identity, kind and order are immutable");
    if (old != value) {
      endpoint_changed = true;
      bool selected = false;
      auto check = [&](const NativeAuthoringPlacement& placement) {
        if (placement.state != NativePlacementState::kBound)
          return true;
        selected = true;
        if (const auto* door =
                std::get_if<NativeAuthoringDoorPlacement>(&placement.value))
          return door->trigger_row &&
                 std::any_of(edits.entrances.begin(), edits.entrances.end(),
                             [&](const auto& e) {
                               return e.row == *door->trigger_row;
                             });
        const auto& back =
            std::get<NativeAuthoringReturnPlacement>(placement.value);
        return back.return_row &&
               std::any_of(
                   edits.returns.begin(), edits.returns.end(),
                   [&](const auto& e) { return e.row == *back.return_row; });
      };
      if (!check(old) || !check(value) || !selected)
        return absl::FailedPreconditionError(
            "Native endpoint metadata change requires selected runtime rows");
      if (value.state == NativePlacementState::kDeleted &&
          value.value != old.value)
        return absl::FailedPreconditionError(
            "Native endpoint tombstone must retain its saved witness");
    }
    sprite_only.placements[i] = old;
  }
  if (proposed.schema_version != expected.schema_version && !endpoint_changed)
    return absl::FailedPreconditionError(
        "Native authoring schema upgrade requires an endpoint binding");
  // Keep the original allocator/order/sprite checks rather than introduce a
  // parallel identity policy. Only the explicitly authorized endpoints differ.
  return ValidateNativeAuthoringSpriteTransition(expected, sprite_only,
                                                 materialized_count);
}

absl::Status ValidateNativeEndpointPlacementWitness(
    const std::vector<uint8_t>& image,
    const NativeOverworldAuthoringMetadata& metadata) {
  const auto layout = InspectLyraLayout(image.data(), image.size());
  if (!IsNativeSpecialOverworldLayout(layout) || !layout.has_authoring_metadata)
    return absl::FailedPreconditionError(
        "Native endpoint witness requires a valid metadata layout");
  const auto encoded =
      EncodeNativeOverworldAuthoringMetadata(metadata, layout.capacity);
  if (!encoded.ok())
    return encoded.status();
  auto word = [&](size_t pc) -> uint16_t {
    return image[pc] | (static_cast<uint16_t>(image[pc + 1]) << 8);
  };
  NativeOverworldEndpointEdits rows;
  for (const auto& placement : metadata.placements) {
    if (placement.state != NativePlacementState::kBound ||
        std::holds_alternative<NativeAuthoringSpritePlacement>(placement.value))
      continue;
    auto page_position =
        [&](const auto& value) -> absl::StatusOr<std::array<uint16_t, 3>> {
      const auto owner =
          LyraLayoutRecord(layout, image.data(), image.size(), 7, value.map);
      if (!owner)
        return absl::FailedPreconditionError(
            "Native endpoint witness has no physical-page owner");
      const uint16_t parent = (*owner)[0];
      const int dx = (value.map % 8) - (parent % 8);
      const int dy = ((value.map % 64) / 8) - ((parent % 64) / 8);
      const auto shape =
          LyraLayoutRecord(layout, image.data(), image.size(), 30, parent);
      if (value.map >= layout.active_count || value.map / 64 != parent / 64 ||
          dx < 0 || dy < 0 || !shape || (*shape)[0] > 3 ||
          dx >= (((*shape)[0] == 1 || (*shape)[0] == 2) ? 2 : 1) ||
          dy >= (((*shape)[0] == 1 || (*shape)[0] == 3) ? 2 : 1))
        return absl::FailedPreconditionError(
            "Native endpoint physical page is outside its owning area");
      return std::array<uint16_t, 3>{parent,
                                     static_cast<uint16_t>(dx * 32 + value.x),
                                     static_cast<uint16_t>(dy * 32 + value.y)};
    };
    if (const auto* door =
            std::get_if<NativeAuthoringDoorPlacement>(&placement.value)) {
      ASSIGN_OR_RETURN(auto position, page_position(*door));
      const bool expanded = image[kEntranceFlag] != 0xB8;
      const auto& tables = expanded ? kExpandedEntrances : kEntrances;
      if (*door->trigger_row >= (expanded ? 256 : 129))
        return absl::FailedPreconditionError(
            "Native door binding exceeds runtime entrance capacity");
      const size_t row = *door->trigger_row;
      const NativeOverworldEntranceRow saved = {word(tables[0] + row * 2),
                                                word(tables[1] + row * 2),
                                                image[tables[2] + row]};
      if (saved.map_word != position[0] ||
          saved.position != ((position[2] << 7) | (position[1] << 1)) ||
          saved.entrance_id != *door->entrance_id)
        return absl::FailedPreconditionError(
            "Native door placement witness differs from its runtime row");
      rows.entrances.push_back({static_cast<uint16_t>(row), saved, saved});
    } else {
      const auto& back =
          std::get<NativeAuthoringReturnPlacement>(placement.value);
      ASSIGN_OR_RETURN(auto position, page_position(back));
      const size_t row = *back.return_row;
      std::array<uint16_t, 13> fields{};
      for (size_t field = 0; field < fields.size(); ++field) {
        const size_t pc = kReturns[field] + row * kReturnWidths[field];
        fields[field] = kReturnWidths[field] == 1 ? image[pc] : word(pc);
      }
      const NativeOverworldReturnRow saved = {fields[0],
                                              static_cast<uint8_t>(fields[1]),
                                              fields[2],
                                              fields[3],
                                              fields[4],
                                              fields[5],
                                              fields[6],
                                              fields[7],
                                              fields[8],
                                              static_cast<uint8_t>(fields[9]),
                                              static_cast<uint8_t>(fields[10]),
                                              fields[11],
                                              fields[12]};
      if (saved.room != *back.room_id || saved.map != position[0] ||
          saved.player_x / 16 != (position[0] % 8) * 32 + position[1] ||
          saved.player_y / 16 != ((position[0] % 64) / 8) * 32 + position[2])
        return absl::FailedPreconditionError(
            "Native return placement witness differs from its runtime row");
      rows.returns.push_back({static_cast<uint16_t>(row), saved, saved});
    }
  }
  // The existing planner owns the complete storage, alias and area bounds
  // checks, even when these selected rows contain no byte changes.
  const auto checked = PlanNativeOverworldEndpointSave(image, rows);
  return checked.ok() ? absl::OkStatus() : checked.status();
}

}  // namespace yaze::rom

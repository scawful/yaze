#include "zelda3/dungeon/chest_edit.h"

#include <cstdint>

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze::zelda3 {
namespace {

bool IsChest(const RoomObject& object) {
  return UsesRoomObjectStream(object) && IsStatefulChestObjectId(object.id_);
}

std::vector<size_t> StreamObjectIndices(const std::vector<RoomObject>& objects,
                                        bool include_locks = false) {
  std::vector<size_t> indices;
  for (uint8_t list = 0; list < 3; ++list) {
    for (size_t index = 0; index < objects.size(); ++index) {
      const auto& object = objects[index];
      if (object.GetLayerValue() == list && UsesRoomObjectStream(object) &&
          (IsStatefulChestObjectId(object.id_) ||
           (include_locks && object.id_ == 0xF98))) {
        indices.push_back(index);
      }
    }
  }
  return indices;
}

absl::Status ValidateRoomEventSlots(const std::vector<RoomObject>& objects) {
  size_t slots = 0;
  bool saw_lock = false;
  for (uint8_t list = 0; list < 3; ++list) {
    for (const auto& object : objects) {
      if (!UsesRoomObjectStream(object) || object.GetLayerValue() != list) {
        continue;
      }
      if (object.id_ == 0xF98) {
        saw_lock = true;
        ++slots;
      } else if (IsStatefulChestObjectId(object.id_)) {
        if (saw_lock) {
          return absl::FailedPreconditionError(
              "Place stateful chests before big-key locks in object-list "
              "order to avoid shared room-state slots");
        }
        ++slots;
      }
    }
  }
  if (slots > kMaxChests) {
    return absl::ResourceExhaustedError(
        "A room supports at most six stateful chests and big-key locks "
        "combined");
  }
  return absl::OkStatus();
}

}  // namespace

std::optional<size_t> ChestIndexForObject(
    const std::vector<RoomObject>& objects, size_t object_index) {
  const auto indices = StreamObjectIndices(objects);
  for (size_t ordinal = 0; ordinal < indices.size(); ++ordinal) {
    if (indices[ordinal] == object_index) {
      return ordinal;
    }
  }
  return std::nullopt;
}

absl::Status ValidateChestObjectMapping(const std::vector<RoomObject>& objects,
                                        const std::vector<chest_data>& chests) {
  const auto indices = StreamObjectIndices(objects);
  if (chests.size() != indices.size()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room has %zu stateful chest objects but %zu contents records; "
        "resolve the mapping before adding, deleting, or reordering chests",
        indices.size(), chests.size()));
  }
  for (size_t ordinal = 0; ordinal < indices.size(); ++ordinal) {
    if (chests[ordinal].size != (objects[indices[ordinal]].id_ == 0xFB1)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Chest %zu contents type does not match its small/big object; "
          "resolve the mapping before changing chest objects",
          ordinal + 1));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<chest_data>> PlanChestObjectEdit(
    const std::vector<RoomObject>& before_objects,
    const std::vector<chest_data>& before_chests,
    const std::vector<RoomObject>& after_objects,
    const std::vector<std::optional<size_t>>& source_indices,
    const std::vector<std::optional<chest_data>>& chest_overrides) {
  if (source_indices.size() != after_objects.size()) {
    return absl::InvalidArgumentError(
        "Chest edit source indices must match the candidate objects");
  }
  if (!chest_overrides.empty() &&
      chest_overrides.size() != after_objects.size()) {
    return absl::InvalidArgumentError(
        "Chest overrides must match the candidate objects");
  }
  for (size_t index = 0; index < after_objects.size(); ++index) {
    if (source_indices[index] &&
        *source_indices[index] >= before_objects.size()) {
      return absl::InvalidArgumentError("Chest edit source index is invalid");
    }
    const auto& object = after_objects[index];
    if (UsesRoomObjectStream(object) &&
        (IsChest(object) || object.id_ == 0xF98) &&
        object.GetLayerValue() > 2) {
      return absl::InvalidArgumentError(
          "Chest or big-key lock object-list index must be in range 0..2");
    }
    if (!chest_overrides.empty() && chest_overrides[index]) {
      if (!IsChest(object)) {
        return absl::InvalidArgumentError(
            "Only stateful chest objects can have contents overrides");
      }
      if (chest_overrides[index]->size != (object.id_ == 0xFB1)) {
        return absl::InvalidArgumentError(
            "Chest contents override must match the small/big object type");
      }
    }
  }

  const auto before_indices = StreamObjectIndices(before_objects);
  const auto after_indices = StreamObjectIndices(after_objects);
  const auto before_events = StreamObjectIndices(before_objects, true);
  const auto after_events = StreamObjectIndices(after_objects, true);
  bool unchanged = before_events.size() == after_events.size();
  for (size_t ordinal = 0; unchanged && ordinal < after_events.size();
       ++ordinal) {
    const size_t index = after_events[ordinal];
    unchanged =
        source_indices[index] == before_events[ordinal] &&
        after_objects[index].id_ == before_objects[before_events[ordinal]].id_;
  }
  for (size_t ordinal = 0; unchanged && ordinal < after_indices.size();
       ++ordinal) {
    const size_t index = after_indices[ordinal];
    if (!chest_overrides.empty() && chest_overrides[index]) {
      const auto& replacement = *chest_overrides[index];
      unchanged = unchanged && ordinal < before_chests.size() &&
                  replacement.id == before_chests[ordinal].id &&
                  replacement.size == before_chests[ordinal].size;
    }
  }
  if (unchanged) {
    return before_chests;
  }

  const absl::Status mapping_status =
      ValidateChestObjectMapping(before_objects, before_chests);
  if (!mapping_status.ok()) {
    return mapping_status;
  }
  std::vector<std::optional<size_t>> before_ordinals(before_objects.size());
  for (size_t ordinal = 0; ordinal < before_indices.size(); ++ordinal) {
    before_ordinals[before_indices[ordinal]] = ordinal;
  }
  const absl::Status slots_status = ValidateRoomEventSlots(after_objects);
  if (!slots_status.ok()) {
    return slots_status;
  }

  std::vector<chest_data> after_chests;
  after_chests.reserve(after_indices.size());
  for (size_t index : after_indices) {
    chest_data chest{0x34, after_objects[index].id_ == 0xFB1};
    if (source_indices[index]) {
      if (const auto ordinal = before_ordinals[*source_indices[index]]) {
        chest.id = before_chests[*ordinal].id;
      }
    }
    if (!chest_overrides.empty() && chest_overrides[index]) {
      chest = *chest_overrides[index];
    }
    after_chests.push_back(chest);
  }
  return after_chests;
}

}  // namespace yaze::zelda3

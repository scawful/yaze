#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_STREAM_ORDERING_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_STREAM_ORDERING_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

struct ObjectStorageMutationResult {
  bool changed = false;
  std::vector<size_t> old_to_new_index;
  std::vector<size_t> changed_old_indices;
  std::vector<size_t> selected_indices;
};

// Reassigns the selected objects' stored placement value. Room-stream objects
// are removed from their old stream and appended to the target stream, matching
// the editor's z-order operation. Torches and pushable blocks stay in place and
// interpret values 0/1 as their special-table draw-layer selector. A value of
// 2 is rejected atomically when any selected object uses a special table.
inline absl::StatusOr<ObjectStorageMutationResult> ReassignObjectStorage(
    std::vector<RoomObject>& objects, const std::vector<size_t>& indices,
    int target_value) {
  if (target_value < 0 || target_value > 2) {
    return absl::InvalidArgumentError("Invalid object stream target");
  }

  ObjectStorageMutationResult result;
  result.old_to_new_index.resize(objects.size());
  std::iota(result.old_to_new_index.begin(), result.old_to_new_index.end(), 0);

  std::vector<bool> selected(objects.size(), false);
  for (size_t index : indices) {
    if (index < objects.size()) {
      selected[index] = true;
    }
  }

  bool room_stream_changed = false;
  for (size_t index = 0; index < objects.size(); ++index) {
    if (selected[index] && UsesSpecialLayerSelector(objects[index]) &&
        target_value == 2) {
      return absl::InvalidArgumentError(
          "Torches and pushable blocks only support upper/lower draw-layer "
          "selector values 0/1");
    }
    if (selected[index] && objects[index].GetLayerValue() != target_value) {
      result.changed_old_indices.push_back(index);
      room_stream_changed |= UsesRoomObjectStream(objects[index]);
    }
  }
  if (result.changed_old_indices.empty()) {
    return result;
  }
  result.changed = true;

  struct Entry {
    RoomObject object;
    size_t old_index = 0;
  };

  std::vector<RoomObject> reordered = objects;
  for (size_t index = 0; index < objects.size(); ++index) {
    if (selected[index] && UsesSpecialLayerSelector(objects[index])) {
      reordered[index].layer_ =
          static_cast<RoomObject::LayerType>(target_value);
    }
  }

  std::vector<size_t> stream_slots;
  std::array<std::vector<Entry>, 3> stream_buckets;
  std::vector<Entry> moved_to_target;

  if (room_stream_changed) {
    for (size_t index = 0; index < objects.size(); ++index) {
      RoomObject object = objects[index];
      if (UsesSpecialLayerSelector(object)) {
        continue;
      }

      stream_slots.push_back(index);
      if (selected[index] && object.GetLayerValue() != target_value) {
        object.layer_ = static_cast<RoomObject::LayerType>(target_value);
        moved_to_target.push_back(Entry{std::move(object), index});
        continue;
      }

      const int stream =
          std::clamp(static_cast<int>(object.GetLayerValue()), 0, 2);
      stream_buckets[stream].push_back(Entry{std::move(object), index});
    }

    for (auto& entry : moved_to_target) {
      stream_buckets[target_value].push_back(std::move(entry));
    }

    size_t stream_slot_index = 0;
    for (auto& bucket : stream_buckets) {
      for (auto& entry : bucket) {
        const size_t new_index = stream_slots[stream_slot_index++];
        result.old_to_new_index[entry.old_index] = new_index;
        reordered[new_index] = std::move(entry.object);
      }
    }
  }

  for (size_t old_index = 0; old_index < selected.size(); ++old_index) {
    if (selected[old_index]) {
      result.selected_indices.push_back(result.old_to_new_index[old_index]);
    }
  }
  std::sort(result.selected_indices.begin(), result.selected_indices.end());

  objects = std::move(reordered);
  return result;
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_STREAM_ORDERING_H_

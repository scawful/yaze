#include "dungeon_validator.h"

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze {
namespace zelda3 {
namespace {

constexpr int16_t kBigKeyLockObjectId = 0xF98;
constexpr size_t kMaxStatefulRoomEventSlots = 6;

}  // namespace

ValidationResult DungeonValidator::ValidateRoom(const Room& room) {
  ValidationResult result;

  // Check object count
  size_t object_count = room.GetTileObjects().size();
  if (object_count > kMaxTileObjects) {
    result.warnings.push_back(absl::StrFormat(
        "High object count (%zu > %d). May cause lag or memory issues.",
        object_count, kMaxTileObjects));
  }

  // Check sprite count
  size_t sprite_count = room.GetSprites().size();
  if (sprite_count > kMaxTotalSprites) {
    result.warnings.push_back(
        absl::StrFormat("Too many sprites (%zu > %d). Game limit is strict.",
                        sprite_count, kMaxTotalSprites));
  }

  // Stateful chests and big-key locks share a six-entry room-event table in
  // the game engine. Chests use a chest-only index and then synchronize the
  // shared index, so a chest after a lock reuses an earlier event slot.
  // Validate the encoded stream order (primary, BG2 overlay, BG1 overlay), not
  // the editor vector order, which may interleave objects from those lists.
  size_t chest_slot_index = 0;
  size_t shared_event_slot_index = 0;
  bool saw_big_key_lock = false;
  int first_chest_after_lock = -1;
  int first_out_of_range_object = -1;
  size_t first_out_of_range_slot = 0;
  for (uint8_t list_index = 0; list_index < 3; ++list_index) {
    for (const auto& obj : room.GetTileObjects()) {
      if (!UsesRoomObjectStream(obj) || obj.GetLayerValue() != list_index) {
        continue;
      }

      if (obj.id_ == kBigKeyLockObjectId) {
        if (shared_event_slot_index >= kMaxStatefulRoomEventSlots &&
            first_out_of_range_object < 0) {
          first_out_of_range_object = obj.id_;
          first_out_of_range_slot = shared_event_slot_index;
        }
        saw_big_key_lock = true;
        ++shared_event_slot_index;
      } else if (IsStatefulChestObjectId(obj.id_)) {
        if (chest_slot_index >= kMaxStatefulRoomEventSlots &&
            first_out_of_range_object < 0) {
          first_out_of_range_object = obj.id_;
          first_out_of_range_slot = chest_slot_index;
        }
        if (saw_big_key_lock && first_chest_after_lock < 0) {
          first_chest_after_lock = obj.id_;
        }
        ++chest_slot_index;
        // RoomDraw_Chest and RoomDraw_BigChest advance the chest-only $0496
        // index, then copy its next value into shared index $0498.
        shared_event_slot_index = chest_slot_index;
      }
    }
  }

  if (first_chest_after_lock >= 0) {
    result.warnings.push_back(absl::StrFormat(
        "Stateful chest 0x%03X appears after big-key lock 0xF98 in "
        "room-object stream order; move stateful chests before locks to avoid "
        "room-state slot conflicts.",
        first_chest_after_lock));
  }
  if (first_out_of_range_object >= 0) {
    result.warnings.push_back(absl::StrFormat(
        "Stateful object 0x%03X accesses room-event slot %zu; RoomFlagMask "
        "only defines slots 0-%zu.",
        first_out_of_range_object, first_out_of_range_slot,
        kMaxStatefulRoomEventSlots - 1));
  }

  // Check bounds
  for (const auto& obj : room.GetTileObjects()) {
    if (UsesRoomObjectStream(obj)) {
      const absl::Status status = ValidateRoomObjectStreamEntryForSave(obj);
      if (!status.ok()) {
        result.errors.emplace_back(std::string(status.message()));
        result.is_valid = false;
      }
      continue;
    }

    const int layer = static_cast<int>(obj.GetLayerValue());
    if (layer > 1) {
      result.errors.push_back(absl::StrFormat(
          "Special-table object 0x%02X has invalid layer selector %d; "
          "expected upper/BG1 (0) or lower/BG2 (1)",
          obj.id_, layer));
      result.is_valid = false;
    }

    if (obj.x_ < 0 || obj.x_ >= 64 || obj.y_ < 0 || obj.y_ >= 64) {
      result.errors.push_back(absl::StrFormat(
          "Object 0x%02X out of bounds at (%d, %d)", obj.id_, obj.x_, obj.y_));
      result.is_valid = false;
    }
  }

  return result;
}

}  // namespace zelda3
}  // namespace yaze

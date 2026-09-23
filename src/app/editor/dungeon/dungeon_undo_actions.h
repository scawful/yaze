#ifndef YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/dungeon/dungeon_selection_edit.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonObjectsAction
 * @brief Undoable action for dungeon room object edits.
 *
 * Captures a full snapshot of a room's tile objects, chest contents and selection
 * before and after an editing operation. Undo restores the before-state,
 * Redo restores the after-state, using a caller-provided restore callback
 * that applies the snapshot back into the room.
 */
class DungeonObjectsAction : public UndoAction {
 public:
  using RestoreFn = std::function<absl::Status(
      int room_id, const std::vector<zelda3::RoomObject>&,
      const std::vector<size_t>& selected_indices,
      const std::vector<chest_data>& chests)>;

  DungeonObjectsAction(int room_id, std::vector<zelda3::RoomObject> before,
                       std::vector<size_t> before_selection,
                       std::vector<zelda3::RoomObject> after,
                       std::vector<size_t> after_selection,
                       std::vector<chest_data> before_chests,
                       std::vector<chest_data> after_chests, RestoreFn restore)
      : room_id_(room_id),
        before_(std::move(before)),
        before_selection_(std::move(before_selection)),
        after_(std::move(after)),
        after_selection_(std::move(after_selection)),
        before_chests_(std::move(before_chests)),
        after_chests_(std::move(after_chests)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError("DungeonObjectsAction: no restore callback");
    }
    return restore_(room_id_, before_, before_selection_, before_chests_);
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError("DungeonObjectsAction: no restore callback");
    }
    return restore_(room_id_, after_, after_selection_, after_chests_);
  }

  std::string Description() const override {
    return absl::StrFormat("Edit room %03X objects", room_id_);
  }

  size_t MemoryUsage() const override {
    // Rough estimate: each RoomObject is ~40-80 bytes
    return (before_.size() + after_.size()) * sizeof(zelda3::RoomObject) +
           (before_selection_.size() + after_selection_.size()) *
               sizeof(size_t) +
           (before_chests_.size() + after_chests_.size()) * sizeof(chest_data);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    // Object edits are already grouped per mutation (drag, delete, etc.)
    // so merging is not needed.
    return false;
  }

 private:
  int room_id_;
  std::vector<zelda3::RoomObject> before_;
  std::vector<size_t> before_selection_;
  std::vector<zelda3::RoomObject> after_;
  std::vector<size_t> after_selection_;
  std::vector<chest_data> before_chests_, after_chests_;
  RestoreFn restore_;
};

// Only the vector named by domain is populated. Keeping domains independent
// prevents a door undo from reverting a later sprite or item edit.
struct DungeonEntitySnapshot {
  std::vector<zelda3::Room::Door> doors;
  std::vector<DungeonSpriteSnapshot> sprites;
  std::vector<zelda3::PotItem> items;
  std::vector<SelectedEntity> entities;
  std::vector<size_t> objects;
};

class DungeonEntitiesAction : public UndoAction {
 public:
  using RestoreFn = std::function<absl::Status(int, MutationDomain,
                                               const DungeonEntitySnapshot&)>;
  DungeonEntitiesAction(int room_id, MutationDomain domain,
                        DungeonEntitySnapshot before,
                        DungeonEntitySnapshot after, RestoreFn restore)
      : room_id_(room_id),
        domain_(domain),
        before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}
  absl::Status Undo() override {
    return restore_ ? restore_(room_id_, domain_, before_)
                    : absl::InternalError(
                          "DungeonEntitiesAction: no restore callback");
  }
  absl::Status Redo() override {
    return restore_ ? restore_(room_id_, domain_, after_)
                    : absl::InternalError(
                          "DungeonEntitiesAction: no restore callback");
  }
  std::string Description() const override {
    const char* label = domain_ == MutationDomain::kDoors     ? "doors"
                        : domain_ == MutationDomain::kSprites ? "sprites"
                                                              : "pot items";
    return absl::StrFormat("Edit room %03X %s", room_id_, label);
  }
  size_t MemoryUsage() const override {
    auto bytes = [](const DungeonEntitySnapshot& state) {
      return state.doors.size() * sizeof(zelda3::Room::Door) +
             state.sprites.size() * sizeof(DungeonSpriteSnapshot) +
             state.items.size() * sizeof(zelda3::PotItem) +
             state.entities.size() * sizeof(SelectedEntity) +
             state.objects.size() * sizeof(size_t);
    };
    return bytes(before_) + bytes(after_);
  }
  bool CanMergeWith(const UndoAction&) const override { return false; }

 private:
  int room_id_;
  MutationDomain domain_;
  DungeonEntitySnapshot before_, after_;
  RestoreFn restore_;
};

struct WaterFillSnapshot {
  uint8_t sram_bit_mask = 0;      // Bit in $7EF411 (0x00 = Auto/unspecified)
  std::vector<uint16_t> offsets;  // Each offset = Y*64 + X (0..4095)
};

class DungeonCustomCollisionAction : public UndoAction {
 public:
  using RestoreFn =
      std::function<void(int room_id, const zelda3::CustomCollisionMap&)>;

  DungeonCustomCollisionAction(int room_id, zelda3::CustomCollisionMap before,
                               zelda3::CustomCollisionMap after,
                               RestoreFn restore)
      : room_id_(room_id),
        before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError(
          "DungeonCustomCollisionAction: no restore callback");
    }
    restore_(room_id_, before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError(
          "DungeonCustomCollisionAction: no restore callback");
    }
    restore_(room_id_, after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit room %03X custom collision", room_id_);
  }

  size_t MemoryUsage() const override {
    return sizeof(before_) + sizeof(after_);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  int room_id_;
  zelda3::CustomCollisionMap before_;
  zelda3::CustomCollisionMap after_;
  RestoreFn restore_;
};

struct DungeonCustomCollisionSnapshot {
  int room_id = -1;
  zelda3::CustomCollisionMap map;
};

class DungeonCustomCollisionBatchAction : public UndoAction {
 public:
  using RestoreFn = std::function<absl::Status(
      const std::vector<DungeonCustomCollisionSnapshot>&)>;

  DungeonCustomCollisionBatchAction(
      std::vector<DungeonCustomCollisionSnapshot> before,
      std::vector<DungeonCustomCollisionSnapshot> after, RestoreFn restore)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError(
          "DungeonCustomCollisionBatchAction: no restore callback");
    }
    return restore_(before_);
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError(
          "DungeonCustomCollisionBatchAction: no restore callback");
    }
    return restore_(after_);
  }

  std::string Description() const override {
    return absl::StrFormat("Generate minecart collision for %d rooms",
                           static_cast<int>(after_.size()));
  }

  size_t MemoryUsage() const override {
    return (before_.size() + after_.size()) *
           sizeof(DungeonCustomCollisionSnapshot);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  std::vector<DungeonCustomCollisionSnapshot> before_;
  std::vector<DungeonCustomCollisionSnapshot> after_;
  RestoreFn restore_;
};

class DungeonWaterFillAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(int room_id, const WaterFillSnapshot&)>;

  DungeonWaterFillAction(int room_id, WaterFillSnapshot before,
                         WaterFillSnapshot after, RestoreFn restore)
      : room_id_(room_id),
        before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError("DungeonWaterFillAction: no restore callback");
    }
    restore_(room_id_, before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError("DungeonWaterFillAction: no restore callback");
    }
    restore_(room_id_, after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit room %03X water fill", room_id_);
  }

  size_t MemoryUsage() const override {
    return before_.offsets.size() * sizeof(uint16_t) +
           after_.offsets.size() * sizeof(uint16_t) + sizeof(before_) +
           sizeof(after_);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  int room_id_;
  WaterFillSnapshot before_;
  WaterFillSnapshot after_;
  RestoreFn restore_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_

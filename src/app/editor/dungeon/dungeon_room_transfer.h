#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_TRANSFER_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_TRANSFER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_selection_edit.h"

namespace yaze::editor {

inline constexpr uint16_t kTransferObjects = 1;
inline constexpr uint16_t kTransferDoors = 2;
inline constexpr uint16_t kTransferSprites = 4;
inline constexpr uint16_t kTransferItems = 8;
inline constexpr uint16_t kTransferMetadata = 16;
inline constexpr uint16_t kTransferCollision = 32;
inline constexpr uint16_t kTransferWater = 64;
inline constexpr uint16_t kTransferCore = 31;
inline constexpr uint16_t kTransferAll = 127;
inline constexpr size_t kMaxDungeonRoomDocumentBytes = 1024 * 1024;

struct DungeonRoomTransferOptions {
  uint16_t domains = kTransferCore;
  bool copy_destinations = false;
  bool operator==(const DungeonRoomTransferOptions&) const = default;
};

// Authored room fields, not a ROM image. Graphics/layout/message references
// retain numeric IDs; their project-owned assets and incoming links are not
// included. Selection, caches, raw reserved header bits and sprite sort mode
// are not interchange data.
struct DungeonRoomDocument {
  int source_room_id = -1;
  DungeonSelectionEditState contents;
  zelda3::Room::MetadataSnapshot metadata;
  zelda3::CustomCollisionMap collision;
  zelda3::WaterFillZoneMap water;
};

struct DungeonRoomTransferPlan {
  const Rom* rom = nullptr;
  int target_room_id = -1;
  int clone_source_room_id = -1;
  std::optional<DungeonRoomDocument> clone_source_before;
  DungeonRoomDocument before;
  DungeonRoomDocument after;
  DungeonRoomTransferOptions options;
  bool changed() const;
};

DungeonRoomDocument CaptureDungeonRoomDocument(const zelda3::Room& room);
// Compares authored fields only, ignoring provenance room ID and selection.
bool SameDungeonRoomDocument(const DungeonRoomDocument& a,
                             const DungeonRoomDocument& b);
absl::Status ValidateDungeonRoomDocument(const DungeonRoomDocument& document);
absl::StatusOr<std::string> SerializeDungeonRoomDocument(
    const DungeonRoomDocument& document);
absl::StatusOr<DungeonRoomDocument> ParseDungeonRoomDocument(
    const std::string& json);

// Pure planning. Allocation, global table capacity and project write policy
// require the editor's detached persistence preflight before publication.
absl::StatusOr<DungeonRoomTransferPlan> PlanDungeonRoomTransfer(
    const zelda3::Room& target, const DungeonRoomDocument& source,
    DungeonRoomTransferOptions options = {});
// History restore deliberately accepts prior legacy fields. Publishes changed
// domains only; callers own preflight, history and view invalidation.
void ApplyDungeonRoomDocument(zelda3::Room& room,
                              const DungeonRoomDocument& document);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_TRANSFER_H_

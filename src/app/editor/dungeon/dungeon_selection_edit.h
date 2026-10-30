#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_EDIT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_EDIT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/minecart_object_semantics.h"

namespace yaze::editor {

// Authored fields only: snapshots never retain borrowed sprite preview buffers.
struct DungeonSpriteSnapshot {
  uint8_t id;
  int x, y, subtype, layer, key_drop;
  bool deleted;
  bool operator==(const DungeonSpriteSnapshot&) const = default;
};

inline constexpr uint8_t kSelectionObjects = 1;
inline constexpr uint8_t kSelectionDoors = 2;
inline constexpr uint8_t kSelectionSprites = 4;
inline constexpr uint8_t kSelectionItems = 8;

struct DungeonSelectionEditState {
  std::vector<zelda3::RoomObject> objects;
  std::vector<chest_data> chests;
  std::vector<zelda3::Room::Door> doors;
  std::vector<DungeonSpriteSnapshot> sprites;
  std::vector<zelda3::PotItem> items;
  std::vector<size_t> selected_objects;
  std::vector<SelectedEntity> selected_entities;
};

// Transient clipboard dependency data. This never changes a project/file format.
struct DungeonMinecartClipboardData {
  const void* source_rom = nullptr;
  uint64_t session_generation = 0;
  int source_room = -1;
  zelda3::MinecartRailProfile profile = zelda3::MinecartRailProfile::kLegacy;
  std::map<size_t, uint8_t> collision;
  std::vector<MinecartTrack> starts;
};

struct DungeonSelectionClipboard {
  std::vector<zelda3::RoomObject> objects;
  std::vector<std::optional<chest_data>> object_chests;
  std::vector<zelda3::Room::Door> doors;
  std::vector<DungeonSpriteSnapshot> sprites;
  std::vector<zelda3::PotItem> items;
  int origin_pixel_x = 0;
  int origin_pixel_y = 0;
  std::optional<DungeonMinecartClipboardData> minecart;
  bool empty() const {
    return objects.empty() && doors.empty() && sprites.empty() && items.empty();
  }
};

enum class DungeonSelectionEditKind {
  kDelete,
  kDuplicate,
  kMove,
  kPaste,
  kDoorTrim
};

struct DungeonSelectionEditRequest {
  DungeonSelectionEditKind kind = DungeonSelectionEditKind::kDelete;
  std::vector<size_t> objects;
  std::vector<SelectedEntity> entities;
  const DungeonSelectionClipboard* clipboard = nullptr;
  // Rigid pixel displacement, including paste relative to clipboard positions.
  int delta_x_pixels = 0;
  int delta_y_pixels = 0;
};

struct DungeonSelectionEditPlan {
  int room_id = -1;
  DungeonSelectionEditState before;
  DungeonSelectionEditState after;
  uint8_t domains = 0;
  DungeonSelectionEditKind kind = DungeonSelectionEditKind::kDelete;
  bool changed() const { return domains != 0; }
};

DungeonSelectionEditState CaptureDungeonSelectionEditState(
    const zelda3::Room& room);

uint8_t ChangedDungeonSelectionDomains(const DungeonSelectionEditState& before,
                                       const DungeonSelectionEditState& after);

// Pure planning: failed validation never changes room data, selection or history.
// Stream allocation and global chest-table preflight belong to the editor's
// existing persistence boundary; this planner does not write ROM bytes.
absl::StatusOr<DungeonSelectionEditPlan> PlanDungeonSelectionEdit(
    const zelda3::Room& room, const DungeonSelectionEditRequest& request);

// Complete the currently supported normal-door stone surround. Reuses exact
// native trim records and returns the door + trim selection even for a no-op.
// Profile opt-in and persistence preflight belong to the caller.
absl::StatusOr<DungeonSelectionEditPlan> PlanDungeonDoorTrim(
    const zelda3::Room& room, size_t door_index);

absl::StatusOr<DungeonSelectionClipboard> CopyDungeonSelection(
    const zelda3::Room& room, const std::vector<size_t>& objects,
    const std::vector<SelectedEntity>& entities);

// Publish all requested data planes before the caller invalidates any renderer.
// Restoring history intentionally permits legacy values retained in snapshots.
void ApplyDungeonSelectionEditState(zelda3::Room& room,
                                    const DungeonSelectionEditState& state,
                                    uint8_t domains);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_EDIT_H_

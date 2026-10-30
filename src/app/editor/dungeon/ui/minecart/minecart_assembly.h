#ifndef YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_ASSEMBLY_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_ASSEMBLY_H_

#include <map>
#include <span>
#include <utility>
#include "app/editor/dungeon/dungeon_selection_edit.h"
#include "zelda3/dungeon/track_collision_generator.h"

namespace yaze::editor {

struct MinecartAssemblyMapping {
  std::map<int, int> carts;
  std::map<int, int> channels;
};

struct MinecartAssemblyPlan {
  DungeonSelectionEditPlan selection;
  zelda3::CustomCollisionMap collision_before, collision_after;
  zelda3::MinecartTrackCollisionOwnership ownership_before, ownership_after;
  std::vector<MinecartTrack> tracks_before, tracks_after;
  zelda3::MinecartRailProfile profile;
  uint64_t room_generation = 0, source_generation = 0, asset_generation = 0;
  const void* source_rom = nullptr;
  zelda3::Room::MetadataSnapshot room_metadata{};
  std::string description;
};

// Orthogonal waypoints use tile8 anchors on the 16px grid. No mutation or IDs.
absl::StatusOr<DungeonSelectionClipboard> BuildMinecartRailPath(
    std::span<const std::pair<int, int>> waypoints,
    zelda3::MinecartRailStyle style, zelda3::MinecartRailProfile profile);

absl::StatusOr<DungeonSelectionClipboard> CaptureMinecartAssembly(
    const zelda3::Room& room, const std::vector<size_t>& objects,
    const std::vector<SelectedEntity>& entities,
    std::span<const MinecartTrack> tracks, zelda3::MinecartRailProfile profile);

absl::StatusOr<MinecartAssemblyPlan> PlanMinecartAssembly(
    const zelda3::Room& room, const DungeonSelectionClipboard& clipboard,
    int anchor_x, int anchor_y, const MinecartAssemblyMapping& mapping,
    std::span<const MinecartTrack> tracks,
    const zelda3::MinecartTrackCollisionOwnership& ownership,
    zelda3::MinecartRailProfile profile);

}  // namespace yaze::editor
#endif

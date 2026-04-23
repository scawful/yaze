#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_

#include <cstddef>
#include <optional>
#include <string>

#include "app/editor/dungeon/dungeon_object_interaction.h"

namespace yaze::editor {

enum class DungeonSelectionKind : uint8_t {
  None = 0,
  ObjectSingle,
  ObjectMulti,
  Door,
  Sprite,
  Item,
};

struct DungeonSelectionSnapshot {
  DungeonSelectionKind kind = DungeonSelectionKind::None;
  size_t count = 0;
  int selection_layer = -1;
  std::optional<size_t> primary_object_index;
  SelectedEntity entity{};

  bool HasSelection() const { return kind != DungeonSelectionKind::None; }
  bool HasObjectSelection() const {
    return kind == DungeonSelectionKind::ObjectSingle ||
           kind == DungeonSelectionKind::ObjectMulti;
  }
  bool HasEntitySelection() const {
    return kind == DungeonSelectionKind::Door ||
           kind == DungeonSelectionKind::Sprite ||
           kind == DungeonSelectionKind::Item;
  }
};

inline DungeonSelectionSnapshot BuildDungeonSelectionSnapshot(
    const DungeonObjectInteraction& interaction, const DungeonRoomStore* rooms,
    int room_id) {
  DungeonSelectionSnapshot snapshot;

  const auto selected_objects = interaction.GetSelectedObjectIndices();
  if (!selected_objects.empty()) {
    snapshot.count = selected_objects.size();
    snapshot.primary_object_index = selected_objects.front();
    snapshot.kind = selected_objects.size() == 1
                        ? DungeonSelectionKind::ObjectSingle
                        : DungeonSelectionKind::ObjectMulti;

    if (rooms != nullptr && room_id >= 0 &&
        room_id < static_cast<int>(rooms->size())) {
      const auto& objects = (*rooms)[room_id].GetTileObjects();
      if (selected_objects.front() < objects.size()) {
        snapshot.selection_layer = objects[selected_objects.front()].layer_;
      }
    }
    return snapshot;
  }

  if (!interaction.HasEntitySelection()) {
    return snapshot;
  }

  snapshot.count = 1;
  snapshot.entity = interaction.GetSelectedEntity();
  switch (snapshot.entity.type) {
    case EntityType::Door:
      snapshot.kind = DungeonSelectionKind::Door;
      break;
    case EntityType::Sprite:
      snapshot.kind = DungeonSelectionKind::Sprite;
      break;
    case EntityType::Item:
      snapshot.kind = DungeonSelectionKind::Item;
      break;
    case EntityType::Object:
      snapshot.kind = DungeonSelectionKind::ObjectSingle;
      break;
    case EntityType::None:
    default:
      break;
  }

  return snapshot;
}

inline std::string GetDungeonSelectionSummaryText(
    const DungeonSelectionSnapshot& snapshot) {
  switch (snapshot.kind) {
    case DungeonSelectionKind::ObjectSingle:
      if (snapshot.selection_layer >= 0) {
        return "1 obj, L" + std::to_string(snapshot.selection_layer + 1);
      }
      return "1 obj";
    case DungeonSelectionKind::ObjectMulti:
      return std::to_string(snapshot.count) + " obj";
    case DungeonSelectionKind::Door:
      return "Door";
    case DungeonSelectionKind::Sprite:
      return "Sprite";
    case DungeonSelectionKind::Item:
      return "Item";
    case DungeonSelectionKind::None:
    default:
      return "No selection";
  }
}

inline const char* GetDungeonSelectionKindLabel(DungeonSelectionKind kind) {
  switch (kind) {
    case DungeonSelectionKind::ObjectSingle:
      return "object";
    case DungeonSelectionKind::ObjectMulti:
      return "objects";
    case DungeonSelectionKind::Door:
      return "door";
    case DungeonSelectionKind::Sprite:
      return "sprite";
    case DungeonSelectionKind::Item:
      return "item";
    case DungeonSelectionKind::None:
    default:
      return "selection";
  }
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_

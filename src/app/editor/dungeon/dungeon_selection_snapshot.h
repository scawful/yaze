#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_object_interaction.h"

namespace yaze::editor {

enum class DungeonSelectionKind : uint8_t {
  None = 0,
  ObjectSingle,
  ObjectMulti,
  Door,
  Sprite,
  Item,
  EntityMulti,
  Mixed,
};

struct DungeonSelectionSnapshot {
  DungeonSelectionKind kind = DungeonSelectionKind::None;
  size_t count = 0;
  size_t object_count = 0;
  size_t door_count = 0;
  size_t sprite_count = 0;
  size_t item_count = 0;
  int selection_layer = -1;
  std::optional<size_t> primary_object_index;
  SelectedEntity entity{};

  bool HasSelection() const { return count > 0; }
  bool HasObjectSelection() const {
    return kind == DungeonSelectionKind::ObjectSingle ||
           kind == DungeonSelectionKind::ObjectMulti;
  }
  bool HasEntitySelection() const {
    return door_count > 0 || sprite_count > 0 || item_count > 0;
  }
  bool HasMixedSelection() const {
    return kind == DungeonSelectionKind::Mixed ||
           kind == DungeonSelectionKind::EntityMulti;
  }
};

inline void AppendDungeonSelectionSummaryPart(std::vector<std::string>* parts,
                                              size_t count,
                                              const char* singular,
                                              const char* plural = nullptr) {
  if (count == 0 || parts == nullptr) {
    return;
  }
  parts->push_back(std::to_string(count) + " " +
                   (count == 1 || plural == nullptr ? singular : plural));
}

inline DungeonSelectionSnapshot BuildDungeonSelectionSnapshot(
    const DungeonObjectInteraction& interaction, const DungeonRoomStore* rooms,
    int room_id) {
  DungeonSelectionSnapshot snapshot;

  const auto selected_objects = interaction.GetSelectedObjectIndices();
  snapshot.object_count = selected_objects.size();
  if (!selected_objects.empty()) {
    snapshot.primary_object_index = selected_objects.front();

    if (rooms != nullptr && room_id >= 0 &&
        room_id < static_cast<int>(rooms->size())) {
      const auto& objects = (*rooms)[room_id].GetTileObjects();
      if (selected_objects.front() < objects.size()) {
        snapshot.selection_layer = objects[selected_objects.front()].layer_;
      }
    }
  }

  auto selected_entities =
      interaction.entity_coordinator().GetSelectedEntities();
  if (selected_entities.empty() && interaction.HasEntitySelection()) {
    const SelectedEntity selected = interaction.GetSelectedEntity();
    if (selected.type != EntityType::None) {
      selected_entities.push_back(selected);
    }
  }

  for (SelectedEntity selected : selected_entities) {
    if (snapshot.entity.type == EntityType::None &&
        selected.type != EntityType::None) {
      snapshot.entity = selected;
    }
    switch (selected.type) {
      case EntityType::Door:
        ++snapshot.door_count;
        break;
      case EntityType::Sprite:
        ++snapshot.sprite_count;
        break;
      case EntityType::Item:
        ++snapshot.item_count;
        break;
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
  }

  const size_t entity_count =
      snapshot.door_count + snapshot.sprite_count + snapshot.item_count;
  snapshot.count = snapshot.object_count + entity_count;
  if (snapshot.count == 0) {
    return snapshot;
  }
  if (snapshot.object_count > 0 && entity_count > 0) {
    snapshot.kind = DungeonSelectionKind::Mixed;
    return snapshot;
  }
  if (snapshot.object_count > 0) {
    snapshot.kind = snapshot.object_count == 1
                        ? DungeonSelectionKind::ObjectSingle
                        : DungeonSelectionKind::ObjectMulti;
    return snapshot;
  }
  if (entity_count > 1) {
    snapshot.kind = DungeonSelectionKind::EntityMulti;
    return snapshot;
  }

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
    case DungeonSelectionKind::EntityMulti:
    case DungeonSelectionKind::Mixed: {
      std::vector<std::string> parts;
      AppendDungeonSelectionSummaryPart(&parts, snapshot.object_count, "obj",
                                        "obj");
      AppendDungeonSelectionSummaryPart(&parts, snapshot.door_count, "door",
                                        "doors");
      AppendDungeonSelectionSummaryPart(&parts, snapshot.sprite_count, "sprite",
                                        "sprites");
      AppendDungeonSelectionSummaryPart(&parts, snapshot.item_count, "item",
                                        "items");
      std::string summary = std::to_string(snapshot.count) + " selected";
      if (!parts.empty()) {
        summary += ": ";
        for (size_t i = 0; i < parts.size(); ++i) {
          if (i > 0) {
            summary += ", ";
          }
          summary += parts[i];
        }
      }
      return summary;
    }
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
    case DungeonSelectionKind::EntityMulti:
      return "entities";
    case DungeonSelectionKind::Mixed:
      return "selection";
    case DungeonSelectionKind::None:
    default:
      return "selection";
  }
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_SELECTION_SNAPSHOT_H_

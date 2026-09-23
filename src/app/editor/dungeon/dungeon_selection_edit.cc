#include "app/editor/dungeon/dungeon_selection_edit.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "util/macro.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze::editor {
namespace {

template <typename T>
void AppendUnique(std::vector<T>& values, const T& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

absl::Status NormalizeSelection(
    const DungeonSelectionEditState& state, const std::vector<size_t>& objects,
    const std::vector<SelectedEntity>& entities,
    std::vector<size_t>& selected_objects,
    std::vector<SelectedEntity>& selected_entities) {
  for (size_t index : objects) {
    if (index >= state.objects.size()) {
      return absl::OutOfRangeError("Selected object is no longer available");
    }
    AppendUnique(selected_objects, index);
  }
  for (const auto& entity : entities) {
    size_t count = 0;
    switch (entity.type) {
      case EntityType::Object:
        if (entity.index >= state.objects.size()) {
          return absl::OutOfRangeError(
              "Selected object is no longer available");
        }
        AppendUnique(selected_objects, entity.index);
        continue;
      case EntityType::Door:
        count = state.doors.size();
        break;
      case EntityType::Sprite:
        count = state.sprites.size();
        break;
      case EntityType::Item:
        count = state.items.size();
        break;
      case EntityType::None:
        return absl::InvalidArgumentError(
            "Selection contains an invalid entity");
      default:
        return absl::InvalidArgumentError(
            "Selection contains an unknown entity");
    }
    if (entity.index >= count) {
      return absl::OutOfRangeError("Selected entity is no longer available");
    }
    AppendUnique(selected_entities, entity);
  }
  return absl::OkStatus();
}

template <typename T, typename Equal>
bool SameVector(const std::vector<T>& a, const std::vector<T>& b, Equal equal) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), equal);
}

bool SameChests(const std::vector<chest_data>& a,
                const std::vector<chest_data>& b) {
  return SameVector(a, b, [](const auto& x, const auto& y) {
    return x.id == y.id && x.size == y.size;
  });
}

bool SameObjects(const std::vector<zelda3::RoomObject>& a,
                 const std::vector<zelda3::RoomObject>& b) {
  return SameVector(a, b, [](const auto& x, const auto& y) {
    return x.id_ == y.id_ && x.x_ == y.x_ && x.y_ == y.y_ &&
           x.size_ == y.size_ && x.layer_ == y.layer_ &&
           x.options() == y.options() && x.all_bgs_ == y.all_bgs_ &&
           x.lit_ == y.lit_ && x.block_load_order() == y.block_load_order() &&
           x.block_behavior_layer() == y.block_behavior_layer() &&
           x.torch_reserved_bit() == y.torch_reserved_bit();
  });
}

absl::StatusOr<int> TranslateCoordinate(int64_t coordinate, int delta, int step,
                                        int maximum, const char* label) {
  const int64_t result = coordinate + delta;
  if (result < 0 || result > maximum || result % step != 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s cannot move to pixel %lld; expected 0..%d in %d-pixel steps", label,
        static_cast<long long>(result), maximum, step));
  }
  return static_cast<int>(result);
}

absl::Status TranslateObject(zelda3::RoomObject& object, int dx, int dy) {
  ASSIGN_OR_RETURN(const int x,
                   TranslateCoordinate(object.x_ * 8, dx, 8, 504, "Object X"));
  ASSIGN_OR_RETURN(const int y,
                   TranslateCoordinate(object.y_ * 8, dy, 8, 504, "Object Y"));
  object.x_ = x / 8;
  object.y_ = y / 8;
  if (zelda3::UsesRoomObjectStream(object)) {
    return zelda3::ValidateRoomObjectStreamEntryForSave(object);
  }
  if (object.GetLayerValue() > 1) {
    return absl::InvalidArgumentError("Special objects require layer 0 or 1");
  }
  if ((object.options() & zelda3::ObjectOption::Torch) !=
          zelda3::ObjectOption::Nothing &&
      (object.x_ > 62 || object.y_ > 62)) {
    return absl::InvalidArgumentError("Torch coordinates must be in 0..62");
  }
  return absl::OkStatus();
}

absl::Status TranslateDoor(zelda3::Room::Door& door, int dx, int dy) {
  const auto type = static_cast<unsigned>(door.type);
  if (type > 0x66 || (type & 1) != 0 ||
      static_cast<unsigned>(door.direction) > 3 ||
      !zelda3::DoorPositionManager::IsValidPosition(door.position,
                                                    door.direction)) {
    return absl::InvalidArgumentError(
        "Selected door has invalid ROM properties");
  }
  // Preserve the original bytes and exact slot for an unchanged anchor.
  if (dx == 0 && dy == 0)
    return absl::OkStatus();
  const auto [x, y] = door.GetPixelCoords();
  const int64_t target_x = static_cast<int64_t>(x) + dx;
  const int64_t target_y = static_cast<int64_t>(y) + dy;
  // Translating a selection does not rotate doors. Each direction's usable
  // anchors are drawn from the ROM tables, not the legacy 32-slot header hint.
  for (uint8_t position = 0; position < 16; ++position) {
    if (!zelda3::DoorPositionManager::IsValidPosition(position, door.direction))
      continue;
    const auto [next_x, next_y] =
        zelda3::DoorPositionManager::PositionToPixelCoords(position,
                                                           door.direction);
    if (next_x == target_x && next_y == target_y) {
      door.position = position;
      const auto [b1, b2] = door.EncodeBytes();
      door.byte1 = b1;
      door.byte2 = b2;
      return absl::OkStatus();
    }
  }
  return absl::InvalidArgumentError(
      "Door has no valid slot at the requested position; the selection was not "
      "moved");
}

absl::Status TranslateSprite(DungeonSpriteSnapshot& sprite, int dx, int dy) {
  ASSIGN_OR_RETURN(const int x,
                   TranslateCoordinate(static_cast<int64_t>(sprite.x) * 16, dx,
                                       16, 496, "Sprite X"));
  ASSIGN_OR_RETURN(const int y,
                   TranslateCoordinate(static_cast<int64_t>(sprite.y) * 16, dy,
                                       16, 496, "Sprite Y"));
  sprite.x = x / 16;
  sprite.y = y / 16;
  return SpriteInteractionHandler::ValidateSpriteProperties(
      sprite.id, sprite.x, sprite.y, sprite.subtype, sprite.layer,
      sprite.key_drop);
}

absl::Status TranslateItem(zelda3::PotItem& item, int dx, int dy) {
  ASSIGN_OR_RETURN(const int x, TranslateCoordinate(item.GetPixelX(), dx, 4,
                                                    508, "Pot item X"));
  ASSIGN_OR_RETURN(const int y, TranslateCoordinate(item.GetPixelY(), dy, 16,
                                                    496, "Pot item Y"));
  item.position = static_cast<uint16_t>(((y / 16) << 8) | (x / 4));
  return absl::OkStatus();
}

absl::Status CheckGrowth(size_t before, size_t after, size_t maximum,
                         const char* label) {
  if (after > maximum && after > before) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("%s limit reached (%zu maximum)", label, maximum));
  }
  return absl::OkStatus();
}

template <typename T>
void RemoveIndices(std::vector<T>& values, std::vector<size_t> indices) {
  std::sort(indices.begin(), indices.end(), std::greater<size_t>());
  for (size_t index : indices)
    values.erase(values.begin() + index);
}

std::vector<size_t> EntityIndices(const std::vector<SelectedEntity>& entities,
                                  EntityType type) {
  std::vector<size_t> indices;
  for (const auto& entity : entities) {
    if (entity.type == type)
      indices.push_back(entity.index);
  }
  return indices;
}

}  // namespace

DungeonSelectionEditState CaptureDungeonSelectionEditState(
    const zelda3::Room& room) {
  DungeonSelectionEditState state;
  state.objects = room.GetTileObjects();
  state.chests = room.GetChests();
  state.doors = room.GetDoors();
  state.items = room.GetPotItems();
  for (const auto& sprite : room.GetSprites()) {
    state.sprites.push_back({sprite.id(), sprite.x(), sprite.y(),
                             sprite.subtype(), sprite.layer(),
                             sprite.key_drop(), sprite.deleted()});
  }
  return state;
}

uint8_t ChangedDungeonSelectionDomains(const DungeonSelectionEditState& a,
                                       const DungeonSelectionEditState& b) {
  uint8_t domains = 0;
  if (!SameObjects(a.objects, b.objects) || !SameChests(a.chests, b.chests))
    domains |= kSelectionObjects;
  if (!SameVector(a.doors, b.doors, [](const auto& x, const auto& y) {
        return x.position == y.position && x.type == y.type &&
               x.direction == y.direction && x.byte1 == y.byte1 &&
               x.byte2 == y.byte2;
      }))
    domains |= kSelectionDoors;
  if (a.sprites != b.sprites)
    domains |= kSelectionSprites;
  if (!SameVector(a.items, b.items, [](const auto& x, const auto& y) {
        return x.position == y.position && x.item == y.item;
      }))
    domains |= kSelectionItems;
  return domains;
}

absl::StatusOr<DungeonSelectionClipboard> CopyDungeonSelection(
    const zelda3::Room& room, const std::vector<size_t>& objects,
    const std::vector<SelectedEntity>& entities) {
  const auto state = CaptureDungeonSelectionEditState(room);
  std::vector<size_t> selected_objects;
  std::vector<SelectedEntity> selected_entities;
  RETURN_IF_ERROR(NormalizeSelection(state, objects, entities, selected_objects,
                                     selected_entities));
  DungeonSelectionClipboard clipboard;
  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  const auto include_anchor = [&](int x, int y) {
    min_x = std::min(min_x, x);
    min_y = std::min(min_y, y);
  };
  bool checked_chests = false;
  for (size_t index : selected_objects) {
    const auto& object = state.objects[index];
    std::optional<chest_data> chest;
    if (zelda3::UsesRoomObjectStream(object) &&
        zelda3::IsStatefulChestObjectId(object.id_)) {
      if (!checked_chests) {
        RETURN_IF_ERROR(
            zelda3::ValidateChestObjectMapping(state.objects, state.chests));
        checked_chests = true;
      }
      const auto ordinal = zelda3::ChestIndexForObject(state.objects, index);
      if (!ordinal || *ordinal >= state.chests.size()) {
        return absl::FailedPreconditionError(
            "Selected chest has no contents record");
      }
      chest = state.chests[*ordinal];
    }
    clipboard.objects.push_back(object);
    clipboard.object_chests.push_back(chest);
    include_anchor(object.x_ * 8, object.y_ * 8);
  }
  for (const auto& entity : selected_entities) {
    switch (entity.type) {
      case EntityType::Door: {
        const auto& door = state.doors[entity.index];
        clipboard.doors.push_back(door);
        const auto [x, y] = door.GetPixelCoords();
        include_anchor(x, y);
        break;
      }
      case EntityType::Sprite: {
        const auto& sprite = state.sprites[entity.index];
        clipboard.sprites.push_back(sprite);
        include_anchor(sprite.x * 16, sprite.y * 16);
        break;
      }
      case EntityType::Item: {
        const auto& item = state.items[entity.index];
        clipboard.items.push_back(item);
        include_anchor(item.GetPixelX(), item.GetPixelY());
        break;
      }
      default:
        break;  // NormalizeSelection accepts only the domains above.
    }
  }
  if (!clipboard.empty()) {
    clipboard.origin_pixel_x = min_x;
    clipboard.origin_pixel_y = min_y;
  }
  return clipboard;
}

absl::StatusOr<DungeonSelectionEditPlan> PlanDungeonSelectionEdit(
    const zelda3::Room& room, const DungeonSelectionEditRequest& request) {
  DungeonSelectionEditPlan plan;
  plan.room_id = room.id();
  plan.kind = request.kind;
  plan.before = CaptureDungeonSelectionEditState(room);
  RETURN_IF_ERROR(NormalizeSelection(
      plan.before, request.objects, request.entities,
      plan.before.selected_objects, plan.before.selected_entities));
  plan.after = plan.before;
  auto& after = plan.after;
  const int dx = request.delta_x_pixels;
  const int dy = request.delta_y_pixels;
  std::vector<std::optional<size_t>> sources;
  for (size_t i = 0; i < after.objects.size(); ++i)
    sources.push_back(i);
  std::vector<std::optional<chest_data>> overrides(after.objects.size());
  switch (request.kind) {
    case DungeonSelectionEditKind::kDelete:
      RemoveIndices(after.objects, plan.before.selected_objects);
      RemoveIndices(sources, plan.before.selected_objects);
      overrides.resize(after.objects.size());
      RemoveIndices(after.doors, EntityIndices(plan.before.selected_entities,
                                               EntityType::Door));
      RemoveIndices(after.sprites, EntityIndices(plan.before.selected_entities,
                                                 EntityType::Sprite));
      RemoveIndices(after.items, EntityIndices(plan.before.selected_entities,
                                               EntityType::Item));
      after.selected_objects.clear();
      after.selected_entities.clear();
      break;
    case DungeonSelectionEditKind::kMove:
      if (dx == 0 && dy == 0)
        return plan;
      for (size_t index : after.selected_objects) {
        RETURN_IF_ERROR(TranslateObject(after.objects[index], dx, dy));
      }
      for (const auto& entity : after.selected_entities) {
        if (entity.type == EntityType::Door) {
          RETURN_IF_ERROR(TranslateDoor(after.doors[entity.index], dx, dy));
        } else if (entity.type == EntityType::Sprite) {
          RETURN_IF_ERROR(TranslateSprite(after.sprites[entity.index], dx, dy));
        } else {
          RETURN_IF_ERROR(TranslateItem(after.items[entity.index], dx, dy));
        }
      }
      break;
    case DungeonSelectionEditKind::kDuplicate:
    case DungeonSelectionEditKind::kPaste: {
      DungeonSelectionClipboard copied;
      const DungeonSelectionClipboard* clipboard = request.clipboard;
      if (request.kind == DungeonSelectionEditKind::kDuplicate) {
        ASSIGN_OR_RETURN(
            copied, CopyDungeonSelection(room, plan.before.selected_objects,
                                         plan.before.selected_entities));
        clipboard = &copied;
      }
      if (!clipboard)
        return absl::InvalidArgumentError("Clipboard is unavailable");
      if (clipboard->object_chests.size() != clipboard->objects.size()) {
        return absl::InvalidArgumentError(
            "Clipboard chest metadata is incomplete");
      }
      after.selected_objects.clear();
      after.selected_entities.clear();
      for (size_t i = 0; i < clipboard->objects.size(); ++i) {
        auto object = clipboard->objects[i].CopyForNewPlacement();
        const bool is_chest = zelda3::UsesRoomObjectStream(object) &&
                              zelda3::IsStatefulChestObjectId(object.id_);
        if (is_chest != clipboard->object_chests[i].has_value()) {
          return absl::InvalidArgumentError(
              "Clipboard chest contents do not match its objects");
        }
        if (is_chest)
          object.set_options(object.options() | zelda3::ObjectOption::Chest);
        RETURN_IF_ERROR(TranslateObject(object, dx, dy));
        object.SetRom(room.rom());
        object.tiles_loaded_ = false;
        after.selected_objects.push_back(after.objects.size());
        after.objects.push_back(std::move(object));
        sources.push_back(std::nullopt);
        overrides.push_back(clipboard->object_chests[i]);
      }
      for (auto door : clipboard->doors) {
        RETURN_IF_ERROR(TranslateDoor(door, dx, dy));
        after.selected_entities.push_back(
            {EntityType::Door, after.doors.size()});
        after.doors.push_back(door);
      }
      for (auto sprite : clipboard->sprites) {
        RETURN_IF_ERROR(TranslateSprite(sprite, dx, dy));
        after.selected_entities.push_back(
            {EntityType::Sprite, after.sprites.size()});
        after.sprites.push_back(sprite);
      }
      for (auto item : clipboard->items) {
        RETURN_IF_ERROR(TranslateItem(item, dx, dy));
        after.selected_entities.push_back(
            {EntityType::Item, after.items.size()});
        after.items.push_back(item);
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("Unknown selection edit operation");
  }
  RETURN_IF_ERROR(CheckGrowth(plan.before.objects.size(), after.objects.size(),
                              zelda3::kMaxTileObjects, "Room object"));
  RETURN_IF_ERROR(CheckGrowth(plan.before.doors.size(), after.doors.size(),
                              zelda3::kMaxDoors, "Door"));
  RETURN_IF_ERROR(CheckGrowth(plan.before.sprites.size(), after.sprites.size(),
                              zelda3::kMaxTotalSprites, "Sprite"));
  if (ChangedDungeonSelectionDomains(plan.before, after) & kSelectionObjects) {
    ASSIGN_OR_RETURN(after.chests, zelda3::PlanChestObjectEdit(
                                       plan.before.objects, plan.before.chests,
                                       after.objects, sources, overrides));
  }
  plan.domains = ChangedDungeonSelectionDomains(plan.before, after);
  return plan;
}

void ApplyDungeonSelectionEditState(zelda3::Room& room,
                                    const DungeonSelectionEditState& state,
                                    uint8_t domains) {
  const auto current = CaptureDungeonSelectionEditState(room);
  domains &= ChangedDungeonSelectionDomains(current, state);
  if (domains & kSelectionObjects) {
    if (!SameObjects(current.objects, state.objects)) {
      room.SetTileObjects(state.objects);
    }
    if (!SameChests(current.chests, state.chests)) {
      room.GetChests() = state.chests;
      room.MarkChestsDirty();
    }
  }
  if (domains & kSelectionDoors) {
    room.GetDoors() = state.doors;
    room.MarkObjectStreamDirty();
  }
  if (domains & kSelectionSprites) {
    std::vector<zelda3::Sprite> sprites;
    sprites.reserve(state.sprites.size());
    for (const auto& source : state.sprites) {
      zelda3::Sprite sprite(source.id, static_cast<uint8_t>(source.x),
                            static_cast<uint8_t>(source.y),
                            static_cast<uint8_t>(source.subtype),
                            static_cast<uint8_t>(source.layer));
      sprite.set_key_drop(source.key_drop);
      sprite.set_deleted(source.deleted);
      sprites.push_back(std::move(sprite));
    }
    room.GetSprites() = std::move(sprites);
    room.MarkSpritesDirty();
  }
  if (domains & kSelectionItems) {
    room.GetPotItems() = state.items;
    room.MarkPotItemsDirty();
  }
}

}  // namespace yaze::editor

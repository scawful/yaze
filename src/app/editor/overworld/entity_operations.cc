#include "entity_operations.h"

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace editor {

absl::StatusOr<zelda3::OverworldEntrance*> InsertEntrance(
    zelda3::Overworld* overworld, ImVec2 mouse_pos, int current_map,
    bool is_hole) {
  if (!overworld || !overworld->is_loaded()) {
    return absl::FailedPreconditionError("Overworld not loaded");
  }

  // Snap to 16x16 grid and clamp to bounds (ZScream: EntranceMode.cs:86-87)
  ImVec2 snapped_pos = ClampToOverworldBounds(SnapToEntityGrid(mouse_pos));

  // Get parent map ID (ZScream: EntranceMode.cs:78-82)
  auto* current_ow_map = overworld->overworld_map(current_map);
  uint8_t map_id = GetParentMapId(current_ow_map, current_map);

  if (is_hole) {
    // Search for first deleted hole slot (ZScream: EntranceMode.cs:74-100)
    auto& holes = overworld->holes();
    for (size_t i = 0; i < holes.size(); ++i) {
      if (holes[i].deleted) {
        // Reuse deleted slot
        holes[i].deleted = false;
        holes[i].map_id_ = map_id;
        holes[i].x_ = static_cast<int>(snapped_pos.x);
        holes[i].y_ = static_cast<int>(snapped_pos.y);
        holes[i].entrance_id_ = 0;  // Default, user configures in popup
        holes[i].is_hole_ = true;

        // Update map properties (ZScream: EntranceMode.cs:90)
        holes[i].UpdateMapProperties(map_id, overworld);

        LOG_DEBUG("EntityOps",
                  "Inserted hole at slot %zu: pos=(%d,%d) map=0x%02X", i,
                  holes[i].x_, holes[i].y_, map_id);

        return &holes[i];
      }
    }
    return absl::ResourceExhaustedError(
        "No space available for new hole. Delete one first.");

  } else {
    // Search for first deleted entrance slot (ZScream: EntranceMode.cs:104-130)
    auto* entrances = overworld->mutable_entrances();
    for (size_t i = 0; i < entrances->size(); ++i) {
      if (entrances->at(i).deleted) {
        // Reuse deleted slot
        entrances->at(i).deleted = false;
        entrances->at(i).map_id_ = map_id;
        entrances->at(i).x_ = static_cast<int>(snapped_pos.x);
        entrances->at(i).y_ = static_cast<int>(snapped_pos.y);
        entrances->at(i).entrance_id_ = 0;  // Default, user configures in popup
        entrances->at(i).is_hole_ = false;

        // Update map properties (ZScream: EntranceMode.cs:120)
        entrances->at(i).UpdateMapProperties(map_id, overworld);

        LOG_DEBUG("EntityOps",
                  "Inserted entrance at slot %zu: pos=(%d,%d) map=0x%02X", i,
                  entrances->at(i).x_, entrances->at(i).y_, map_id);

        return &entrances->at(i);
      }
    }
    return absl::ResourceExhaustedError(
        "No space available for new entrance. Delete one first.");
  }
}

absl::StatusOr<zelda3::OverworldExit*> InsertExit(zelda3::Overworld* overworld,
                                                  ImVec2 mouse_pos,
                                                  int current_map) {
  if (!overworld || !overworld->is_loaded()) {
    return absl::FailedPreconditionError("Overworld not loaded");
  }

  // Snap to 16x16 grid and clamp to bounds (ZScream: ExitMode.cs:71-72)
  ImVec2 snapped_pos = ClampToOverworldBounds(SnapToEntityGrid(mouse_pos));

  // Get parent map ID (ZScream: ExitMode.cs:63-67)
  auto* current_ow_map = overworld->overworld_map(current_map);
  uint8_t map_id = GetParentMapId(current_ow_map, current_map);

  // Search for first deleted exit slot (ZScream: ExitMode.cs:59-124)
  auto& exits = *overworld->mutable_exits();
  for (size_t i = 0; i < exits.size(); ++i) {
    if (exits[i].deleted_) {
      // Reuse deleted slot
      exits[i].deleted_ = false;
      exits[i].map_id_ = map_id;
      exits[i].x_ = static_cast<int>(snapped_pos.x);
      exits[i].y_ = static_cast<int>(snapped_pos.y);

      // Initialize with default values (ZScream: ExitMode.cs:95-112)
      // User will configure room_id, scroll, camera in popup
      exits[i].room_id_ = 0;
      exits[i].x_scroll_ = 0;
      exits[i].y_scroll_ = 0;
      exits[i].x_camera_ = 0;
      exits[i].y_camera_ = 0;
      exits[i].x_player_ = static_cast<uint16_t>(snapped_pos.x);
      exits[i].y_player_ = static_cast<uint16_t>(snapped_pos.y);
      exits[i].scroll_mod_x_ = 0;
      exits[i].scroll_mod_y_ = 0;
      exits[i].door_type_1_ = 0;
      exits[i].door_type_2_ = 0;

      // Update map properties with overworld context for area size detection
      exits[i].UpdateMapProperties(map_id, overworld);

      LOG_DEBUG("EntityOps",
                "Inserted exit at slot %zu: pos=(%d,%d) map=0x%02X", i,
                exits[i].x_, exits[i].y_, map_id);

      return &exits[i];
    }
  }

  return absl::ResourceExhaustedError(
      "No space available for new exit. Delete one first.");
}

absl::StatusOr<zelda3::Sprite*> InsertSprite(zelda3::Overworld* overworld,
                                             ImVec2 mouse_pos, int current_map,
                                             int game_state,
                                             uint8_t sprite_id) {
  if (!overworld || !overworld->is_loaded()) {
    return absl::FailedPreconditionError("Overworld not loaded");
  }

  if (game_state < 0 || game_state > 2) {
    return absl::InvalidArgumentError("Invalid game state (must be 0-2)");
  }

  // Snap to 16x16 grid and clamp to bounds (ZScream: SpriteMode.cs similar
  // logic)
  ImVec2 snapped_pos = ClampToOverworldBounds(SnapToEntityGrid(mouse_pos));

  // Get parent map ID (ZScream: SpriteMode.cs:90-95)
  auto* current_ow_map = overworld->overworld_map(current_map);
  uint8_t map_id = GetParentMapId(current_ow_map, current_map);

  // Calculate map position (ZScream uses mapHover for parent tracking)
  // For sprites, we need the actual map coordinates within the 512x512 map
  int map_local_x = static_cast<int>(snapped_pos.x) % 512;
  int map_local_y = static_cast<int>(snapped_pos.y) % 512;

  // Convert to game coordinates (0-63 for X/Y within map)
  uint8_t game_x = static_cast<uint8_t>(map_local_x / 16);
  uint8_t game_y = static_cast<uint8_t>(map_local_y / 16);

  // Add new sprite to the game state array (ZScream: SpriteMode.cs:34-35)
  auto& sprites = *overworld->mutable_sprites(game_state);

  // Create new sprite
  zelda3::Sprite new_sprite(
      current_ow_map->current_graphics(), static_cast<uint8_t>(map_id),
      sprite_id,  // Sprite ID (user will configure in popup)
      game_x,     // X position in map coordinates
      game_y,     // Y position in map coordinates
      static_cast<int>(snapped_pos.x),  // Real X (world coordinates)
      static_cast<int>(snapped_pos.y)   // Real Y (world coordinates)
  );

  sprites.push_back(new_sprite);

  // Return pointer to the newly added sprite
  zelda3::Sprite* inserted_sprite = &sprites.back();

  LOG_DEBUG(
      "EntityOps",
      "Inserted sprite at game_state=%d: pos=(%d,%d) map=0x%02X id=0x%02X",
      game_state, inserted_sprite->x_, inserted_sprite->y_, map_id, sprite_id);

  return inserted_sprite;
}

absl::StatusOr<zelda3::OverworldItem*> InsertItem(zelda3::Overworld* overworld,
                                                  ImVec2 mouse_pos,
                                                  int current_map,
                                                  uint8_t item_id) {
  if (!overworld || !overworld->is_loaded()) {
    return absl::FailedPreconditionError("Overworld not loaded");
  }

  // Snap to 16x16 grid and clamp to bounds (ZScream: ItemMode.cs similar logic)
  ImVec2 snapped_pos = ClampToOverworldBounds(SnapToEntityGrid(mouse_pos));

  // Get parent map ID (ZScream: ItemMode.cs:60-64)
  auto* current_ow_map = overworld->overworld_map(current_map);
  uint8_t map_id = GetParentMapId(current_ow_map, current_map);

  // Calculate game coordinates (0-63 for X/Y within map)
  // Following LoadItems logic in overworld.cc:840-854
  int fake_id = current_map % 0x40;
  int sy = fake_id / 8;
  int sx = fake_id - (sy * 8);

  // Calculate map-local coordinates
  int map_local_x = static_cast<int>(snapped_pos.x) % 512;
  int map_local_y = static_cast<int>(snapped_pos.y) % 512;

  // Game coordinates (0-63 range)
  uint8_t game_x = static_cast<uint8_t>(map_local_x / 16);
  uint8_t game_y = static_cast<uint8_t>(map_local_y / 16);

  // Add new item to the all_items array (ZScream: ItemMode.cs:92-108)
  auto& items = *overworld->mutable_all_items();

  // Create new item with calculated coordinates
  items.emplace_back(item_id,                          // Item ID
                     static_cast<uint16_t>(map_id),    // Room map ID
                     static_cast<int>(snapped_pos.x),  // X (world coordinates)
                     static_cast<int>(snapped_pos.y),  // Y (world coordinates)
                     false                             // Not deleted
  );

  // Set game coordinates
  zelda3::OverworldItem* inserted_item = &items.back();
  inserted_item->game_x_ = game_x;
  inserted_item->game_y_ = game_y;

  LOG_DEBUG("EntityOps",
            "Inserted item: pos=(%d,%d) game=(%d,%d) map=0x%02X id=0x%02X",
            inserted_item->x_, inserted_item->y_, game_x, game_y, map_id,
            item_id);

  return inserted_item;
}

}  // namespace editor
}  // namespace yaze

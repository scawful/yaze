#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_EDITING_TARGET_H_
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_EDITING_TARGET_H_

#include <optional>

#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

struct OverworldEntityEditingTarget {
  zelda3::GameEntity::EntityType type = zelda3::GameEntity::EntityType::kItem;
  std::optional<zelda3::OverworldItem> item_identity;
  uint16_t map_id = 0;
  int x = 0;
  int y = 0;
  uint8_t entrance_id = 0;
  uint16_t entrance_map_pos = 0;
  bool entrance_is_hole = false;
  uint16_t exit_room_id = 0;
  uint16_t exit_map_pos = 0;
  uint8_t sprite_id = 0;
  int sprite_subtype = 0;
  int sprite_layer = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_ENTITY_EDITING_TARGET_H_

#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_OPERATIONS_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_OPERATIONS_H

#include "absl/status/statusor.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

/**
 * @brief Flat helper functions for entity insertion/manipulation
 *
 * Following ZScream's entity management pattern (EntranceMode.cs, ExitMode.cs,
 * etc.) but implemented as free functions to minimize state management.
 *
 * Key concepts from ZScream:
 * - Find first deleted slot for insertion
 * - Calculate map position from mouse coordinates
 * - Use parent map ID for multi-area maps
 * - Call UpdateMapProperties to sync position data
 */

/**
 * @brief Insert a new entrance at the specified position
 *
 * Follows ZScream's EntranceMode.AddEntrance() logic (EntranceMode.cs:53-148):
 * - Finds first deleted entrance slot
 * - Snaps position to 16x16 grid
 * - Uses parent map ID for multi-area maps
 * - Calls UpdateMapProperties to calculate game coordinates
 *
 * @param overworld Overworld data containing entrance arrays
 * @param mouse_pos Mouse position in canvas coordinates (world space)
 * @param current_map Current map index being edited
 * @param is_hole True to insert a hole instead of entrance
 * @return Pointer to newly inserted entrance, or error if no slots available
 */
absl::StatusOr<zelda3::OverworldEntrance*> InsertEntrance(
    zelda3::Overworld* overworld, ImVec2 mouse_pos, int current_map,
    bool is_hole = false);

/**
 * @brief Insert a new exit at the specified position
 *
 * Follows ZScream's ExitMode.AddExit() logic (ExitMode.cs:59-124):
 * - Finds first deleted exit slot
 * - Snaps position to 16x16 grid
 * - Initializes exit with default scroll/camera values
 * - Sets room ID to 0 (needs to be configured by user)
 *
 * @param overworld Overworld data containing exit arrays
 * @param mouse_pos Mouse position in canvas coordinates
 * @param current_map Current map index being edited
 * @return Pointer to newly inserted exit, or error if no slots available
 */
absl::StatusOr<zelda3::OverworldExit*> InsertExit(zelda3::Overworld* overworld,
                                                  ImVec2 mouse_pos,
                                                  int current_map);

/**
 * @brief Insert a new sprite at the specified position
 *
 * Follows ZScream's SpriteMode sprite insertion (SpriteMode.cs:27-100):
 * - Adds new sprite to game state array
 * - Calculates map position and game coordinates
 * - Sets sprite ID (default 0, user configures in popup)
 *
 * @param overworld Overworld data containing sprite arrays
 * @param mouse_pos Mouse position in canvas coordinates
 * @param current_map Current map index being edited
 * @param game_state Current game state (0=beginning, 1=zelda, 2=agahnim)
 * @param sprite_id Sprite ID to insert (default 0)
 * @return Pointer to newly inserted sprite
 */
absl::StatusOr<zelda3::Sprite*> InsertSprite(zelda3::Overworld* overworld,
                                             ImVec2 mouse_pos, int current_map,
                                             int game_state,
                                             uint8_t sprite_id = 0);

/**
 * @brief Insert a new item at the specified position
 *
 * Follows ZScream's ItemMode item insertion (ItemMode.cs:54-113):
 * - Adds new item to all_items array
 * - Calculates map position and game coordinates
 * - Sets item ID (default 0, user configures in popup)
 *
 * @param overworld Overworld data containing item arrays
 * @param mouse_pos Mouse position in canvas coordinates
 * @param current_map Current map index being edited
 * @param item_id Item ID to insert (default 0x00 - Nothing)
 * @return Pointer to newly inserted item
 */
absl::StatusOr<zelda3::OverworldItem*> InsertItem(zelda3::Overworld* overworld,
                                                  ImVec2 mouse_pos,
                                                  int current_map,
                                                  uint8_t item_id = 0);

/**
 * @brief Helper to get parent map ID for multi-area maps
 *
 * Returns the parent map ID, handling the case where a map is its own parent.
 * Matches ZScream logic where ParentID == 255 means use current map.
 */
inline uint8_t GetParentMapId(const zelda3::OverworldMap* map,
                              int current_map) {
  uint8_t parent = map->parent();
  return (parent == 0xFF) ? static_cast<uint8_t>(current_map) : parent;
}

/**
 * @brief Snap position to 16x16 grid (standard entity positioning)
 */
inline ImVec2 SnapToEntityGrid(ImVec2 pos) {
  return ImVec2(static_cast<float>(static_cast<int>(pos.x / 16) * 16),
                static_cast<float>(static_cast<int>(pos.y / 16) * 16));
}

/**
 * @brief Clamp position to valid overworld bounds
 */
inline ImVec2 ClampToOverworldBounds(ImVec2 pos) {
  return ImVec2(std::clamp(pos.x, 0.0f, 4080.0f),  // 4096 - 16
                std::clamp(pos.y, 0.0f, 4080.0f));
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_OPERATIONS_H

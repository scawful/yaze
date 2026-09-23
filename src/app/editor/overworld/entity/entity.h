#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_H

#include <array>
#include <vector>

#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/overworld/diggable_tiles.h"
#include "zelda3/overworld/diggable_tiles_patch.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

/**
 * @brief Check if mouse is hovering over an entity
 * @param entity The entity to check
 * @param canvas_p0 Canvas origin point
 * @param scrolling Canvas scroll offset
 * @param scale Canvas scale factor (default 1.0f)
 * @return true if mouse is over the entity bounds
 */
bool IsMouseHoveringOverEntity(const zelda3::GameEntity& entity,
                               ImVec2 canvas_p0, ImVec2 scrolling,
                               float scale = 1.0f);

/**
 * @brief Check if mouse is hovering over an entity (CanvasRuntime version)
 * @param entity The entity to check
 * @param rt The canvas runtime with geometry info
 * @return true if mouse is over the entity bounds
 */
bool IsMouseHoveringOverEntity(const zelda3::GameEntity& entity,
                               const gui::CanvasRuntime& rt);

/**
 * @brief Move entity to grid-aligned position based on mouse
 * @param entity Entity to move
 * @param canvas_p0 Canvas origin point
 * @param scrolling Canvas scroll offset
 * @param free_movement If true, use 8x8 grid; else 16x16
 * @param scale Canvas scale factor (default 1.0f)
 */
void MoveEntityOnGrid(zelda3::GameEntity* entity, ImVec2 canvas_p0,
                      ImVec2 scrolling, bool free_movement = false,
                      float scale = 1.0f);

bool DrawEntranceInserterPopup();
bool DrawOverworldEntrancePopup(zelda3::OverworldEntrance& entrance);

void DrawExitInserterPopup();
bool DrawExitEditorPopup(zelda3::OverworldExit& exit);

void DrawItemInsertPopup();

bool DrawItemEditorPopup(zelda3::OverworldItem& item);

/**
 * @brief Column IDs for the sprite table.
 *
 */
enum SpriteItemColumnID {
  SpriteItemColumnID_ID,
  SpriteItemColumnID_Name,
  SpriteItemColumnID_Description
};

struct SpriteItem {
  int id;
  const char* name;
  static const ImGuiTableSortSpecs* s_current_sort_specs;

  static void SortWithSortSpecs(ImGuiTableSortSpecs* sort_specs,
                                std::vector<SpriteItem>& items) {
    s_current_sort_specs =
        sort_specs;  // Store for access by the compare function.
    if (items.size() > 1)
      std::sort(items.begin(), items.end(), SpriteItem::CompareWithSortSpecs);
    s_current_sort_specs = nullptr;
  }

  static bool CompareWithSortSpecs(const SpriteItem& a, const SpriteItem& b) {
    for (int n = 0; n < s_current_sort_specs->SpecsCount; n++) {
      const ImGuiTableColumnSortSpecs* sort_spec =
          &s_current_sort_specs->Specs[n];
      int delta = 0;
      switch (sort_spec->ColumnUserID) {
        case SpriteItemColumnID_ID:
          delta = (a.id - b.id);
          break;
        case SpriteItemColumnID_Name:
          delta = strcmp(a.name + 2, b.name + 2);
          break;
      }
      if (delta != 0)
        return (sort_spec->SortDirection == ImGuiSortDirection_Ascending)
                   ? delta < 0
                   : delta > 0;
    }
    return a.id < b.id;  // Fallback
  }
};

void DrawSpriteTable(std::function<void(int)> onSpriteSelect, int& selected_id);
void DrawSpriteInserterPopup();
bool DrawSpriteEditorPopup(zelda3::Sprite& sprite);

/**
 * @brief Draw popup dialog for editing diggable tiles configuration.
 *
 * Provides UI for:
 * - Viewing/editing which Map16 tiles are diggable
 * - Auto-detecting diggable tiles from tile types
 * - Exporting ASM patch for the digging routine
 *
 * @param diggable_tiles Pointer to the DiggableTiles instance to edit
 * @param tiles16 Vector of Map16 tile definitions
 * @param all_tiles_types Array of tile type bytes for auto-detection
 * @return true if changes were saved, false otherwise
 */
bool DrawDiggableTilesEditorPopup(
    zelda3::DiggableTiles* diggable_tiles,
    const std::vector<gfx::Tile16>& tiles16,
    const std::array<uint8_t, 0x200>& all_tiles_types);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_H

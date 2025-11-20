#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_H

#include "imgui/imgui.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

bool IsMouseHoveringOverEntity(const zelda3::GameEntity& entity,
                               ImVec2 canvas_p0, ImVec2 scrolling);

void MoveEntityOnGrid(zelda3::GameEntity* entity, ImVec2 canvas_p0,
                      ImVec2 scrolling, bool free_movement = false);

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

void DrawSpriteTable(std::function<void(int)> onSpriteSelect);
void DrawSpriteInserterPopup();
bool DrawSpriteEditorPopup(zelda3::Sprite& sprite);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_H

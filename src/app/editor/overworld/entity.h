#ifndef YAZE_APP_EDITOR_OVERWORLD_ENTITY_H
#define YAZE_APP_EDITOR_OVERWORLD_ENTITY_H

#include "imgui/imgui.h"

#include "app/editor/overworld/overworld_editor.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace app {
namespace editor {

bool IsMouseHoveringOverEntity(const zelda3::OverworldEntity &entity,
                               ImVec2 canvas_p0, ImVec2 scrolling);

void MoveEntityOnGrid(zelda3::OverworldEntity *entity, ImVec2 canvas_p0,
                      ImVec2 scrolling, bool free_movement = false);

void HandleEntityDragging(zelda3::OverworldEntity *entity, ImVec2 canvas_p0,
                          ImVec2 scrolling, bool &is_dragging_entity,
                          zelda3::OverworldEntity *&dragged_entity,
                          zelda3::OverworldEntity *&current_entity,
                          bool free_movement = false);

bool DrawEntranceInserterPopup();
bool DrawOverworldEntrancePopup(zelda3::overworld::OverworldEntrance &entrance);

void DrawExitInserterPopup();
bool DrawExitEditorPopup(zelda3::overworld::OverworldExit &exit);

void DrawItemInsertPopup();

bool DrawItemEditorPopup(zelda3::overworld::OverworldItem &item);

enum MyItemColumnID {
  MyItemColumnID_ID,
  MyItemColumnID_Name,
  MyItemColumnID_Action,
  MyItemColumnID_Quantity,
  MyItemColumnID_Description
};

struct SpriteItem {
  int id;
  const char *name;
  static const ImGuiTableSortSpecs *s_current_sort_specs;

  static void SortWithSortSpecs(ImGuiTableSortSpecs *sort_specs,
                                std::vector<SpriteItem> &items) {
    s_current_sort_specs =
        sort_specs;  // Store for access by the compare function.
    if (items.size() > 1)
      std::sort(items.begin(), items.end(), SpriteItem::CompareWithSortSpecs);
    s_current_sort_specs = nullptr;
  }

  static bool CompareWithSortSpecs(const SpriteItem &a, const SpriteItem &b) {
    for (int n = 0; n < s_current_sort_specs->SpecsCount; n++) {
      const ImGuiTableColumnSortSpecs *sort_spec =
          &s_current_sort_specs->Specs[n];
      int delta = 0;
      switch (sort_spec->ColumnUserID) {
        case MyItemColumnID_ID:
          delta = (a.id - b.id);
          break;
        case MyItemColumnID_Name:
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
bool DrawSpriteEditorPopup(zelda3::Sprite &sprite);

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_ENTITY_H
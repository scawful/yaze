#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PANEL_ACCESS_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PANEL_ACCESS_H

#include "app/editor/core/window_context.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"

namespace yaze::editor {

using DungeonWindowContext = TypedWindowContext<DungeonEditorV2>;

inline DungeonWindowContext CurrentDungeonWindowContext() {
  return CurrentTypedWindowContext<DungeonEditorV2>("Dungeon");
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_PANEL_ACCESS_H

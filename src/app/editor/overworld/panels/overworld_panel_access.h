#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H

#include "app/editor/core/window_context.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

using OverworldWindowContext = TypedWindowContext<OverworldEditor>;

inline OverworldWindowContext CurrentOverworldWindowContext() {
  return CurrentTypedWindowContext<OverworldEditor>("Overworld");
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H

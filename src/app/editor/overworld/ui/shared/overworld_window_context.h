#ifndef YAZE_APP_EDITOR_OVERWORLD_UI_SHARED_OVERWORLD_WINDOW_CONTEXT_H
#define YAZE_APP_EDITOR_OVERWORLD_UI_SHARED_OVERWORLD_WINDOW_CONTEXT_H

#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/registry/window_context.h"

namespace yaze::editor {

using OverworldWindowContext = TypedWindowContext<OverworldEditor>;

inline OverworldWindowContext CurrentOverworldWindowContext() {
  return CurrentTypedWindowContext<OverworldEditor>("Overworld");
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_UI_SHARED_OVERWORLD_WINDOW_CONTEXT_H

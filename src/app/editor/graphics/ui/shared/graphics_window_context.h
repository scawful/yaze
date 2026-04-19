#ifndef YAZE_APP_EDITOR_GRAPHICS_UI_SHARED_GRAPHICS_WINDOW_CONTEXT_H_
#define YAZE_APP_EDITOR_GRAPHICS_UI_SHARED_GRAPHICS_WINDOW_CONTEXT_H_

#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/registry/window_context.h"

namespace yaze::editor {

using GraphicsWindowContext = TypedWindowContext<GraphicsEditor>;

inline GraphicsWindowContext CurrentGraphicsWindowContext() {
  return CurrentTypedWindowContext<GraphicsEditor>("Graphics");
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_UI_SHARED_GRAPHICS_WINDOW_CONTEXT_H_

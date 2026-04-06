#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H

#include "app/editor/core/content_registry.h"
#include "app/editor/overworld/overworld_editor.h"

namespace yaze::editor {

inline OverworldEditor* CurrentOverworldEditor() {
  auto* editor = ContentRegistry::Context::current_editor();
  if (!editor) {
    return nullptr;
  }
  return dynamic_cast<OverworldEditor*>(editor);
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_PANEL_ACCESS_H

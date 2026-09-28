#include "app/editor/overworld/overworld_editor.h"

#include "app/editor/overworld/canvas/canvas_navigation_manager.h"

namespace yaze::editor {

// View changes are requests: the navigation manager applies them when the
// canvas child begins, so they are safe to call from shortcuts, the toolbar
// or a context menu.

void OverworldEditor::ZoomIn() {
  if (canvas_nav_)
    canvas_nav_->ZoomIn();
}

void OverworldEditor::ZoomOut() {
  if (canvas_nav_)
    canvas_nav_->ZoomOut();
}

void OverworldEditor::ZoomToFit() {
  if (canvas_nav_)
    canvas_nav_->ZoomToFit();
}

void OverworldEditor::ResetOverworldView() {
  if (canvas_nav_)
    canvas_nav_->ResetOverworldView();
}

void OverworldEditor::CenterOverworldView() {
  if (canvas_nav_)
    canvas_nav_->CenterOverworldView();
}

}  // namespace yaze::editor

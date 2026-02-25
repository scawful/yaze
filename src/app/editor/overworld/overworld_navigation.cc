#include "app/editor/overworld/overworld_editor.h"

#include "app/editor/overworld/canvas_navigation_manager.h"

namespace yaze::editor {

void OverworldEditor::HandleOverworldPan() {
  if (canvas_nav_) canvas_nav_->HandleOverworldPan();
}

void OverworldEditor::HandleOverworldZoom() {
  if (canvas_nav_) canvas_nav_->HandleOverworldZoom();
}

void OverworldEditor::ZoomIn() {
  if (canvas_nav_) canvas_nav_->ZoomIn();
}

void OverworldEditor::ZoomOut() {
  if (canvas_nav_) canvas_nav_->ZoomOut();
}

void OverworldEditor::ClampOverworldScroll() {
  if (canvas_nav_) canvas_nav_->ClampOverworldScroll();
}

void OverworldEditor::ResetOverworldView() {
  if (canvas_nav_) canvas_nav_->ResetOverworldView();
}

void OverworldEditor::CenterOverworldView() {
  if (canvas_nav_) canvas_nav_->CenterOverworldView();
}

}  // namespace yaze::editor

#include "app/editor/overworld/overworld_editor.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/overworld/canvas/canvas_navigation_manager.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "imgui/imgui.h"

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

OverworldCanvasLayout OverworldEditor::GetCanvasLayout() const {
  return OverworldCanvasLayout(canvas_layout_mode_);
}

void OverworldEditor::SetCanvasLayoutMode(OverworldCanvasLayoutMode mode) {
  if (canvas_layout_mode_ == mode) {
    return;
  }
  FinalizePaintOperation();
  if (tile_painting_) {
    tile_painting_->SuspendCanvasGestures();
  }
  lyra_canvas_.FinishStroke(undo_manager_);
  lyra_canvas_.CancelEntityPlacement();
  item_placement_armed_ = false;
  is_dragging_entity_ = false;
  dragged_entity_ = nullptr;
  dragged_entity_free_movement_ = false;
  drag_item_snapshot_.reset();
  if (canvas_renderer_) {
    canvas_renderer_->CancelEntityDrag();
  }

  if (mode == OverworldCanvasLayoutMode::kUnified) {
    pre_unified_scale_ = std::clamp(ow_map_canvas_.global_scale(),
                                    kOverworldMinZoom, kOverworldMaxZoom);
    canvas_layout_mode_ = mode;
    if (current_mode != EditingMode::MOUSE) {
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    }
    if (canvas_nav_) {
      canvas_nav_->ZoomToFit();
    }
  } else {
    canvas_layout_mode_ = mode;
    unified_readonly_banner_until_ = 0.0;
    const float restore_scale =
        std::clamp(pre_unified_scale_, kOverworldMinZoom, kOverworldMaxZoom);
    ow_map_canvas_.set_global_scale(restore_scale);
    if (canvas_nav_) {
      canvas_nav_->CenterOnMap(current_map_);
    }
  }
}

void OverworldEditor::ScrollToWorldBlock(int world) {
  if (canvas_nav_) {
    canvas_nav_->ScrollToWorldBlock(world);
  }
}

void OverworldEditor::SwitchToWorldUnified(int world) {
  const int clamped_world = std::clamp(world, 0, kOverworldWorldCount - 1);
  const int world_start = OverworldCanvasLayout::WorldStartMap(clamped_world);
  if (world_start >= DisplayedMapCount()) {
    return;
  }
  const int local_map = OverworldCanvasLayout::LocalIndex(current_map_);
  const int maps_remaining = DisplayedMapCount() - world_start;
  const int target_map =
      world_start + std::min(local_map, std::max(0, maps_remaining - 1));
  hovered_map_ = -1;
  if (!current_map_lock_) {
    SelectMapForEditing(target_map, /*respect_pin=*/false);
    NormalizeCurrentSelectionState();
  }
  if (canvas_nav_) {
    canvas_nav_->SuspendHoverFollowUntilMouseMoves();
    canvas_nav_->ScrollToWorldBlock(clamped_world);
  }
}

void OverworldEditor::NotifyUnifiedReadOnly() {
  if (ImGui::GetCurrentContext()) {
    unified_readonly_banner_until_ = ImGui::GetTime() + 3.5;
  }
  if (dependencies_.toast_manager) {
    dependencies_.toast_manager->Show(kUnifiedReadOnlyReason, ToastType::kInfo);
  }
}

bool OverworldEditor::ContributeHoverPosToStatusBar(
    StatusBar* status_bar) const {
  if (!status_bar) {
    return false;
  }
  if (!ow_map_canvas_.IsMouseHovering()) {
    status_bar->ClearCustomSegment("Pos");
    return false;
  }
  const auto mouse = ow_map_canvas_.hover_mouse_pos();
  const float scale =
      ow_map_canvas_.global_scale() > 0 ? ow_map_canvas_.global_scale() : 1.0f;
  const auto world_point = GetCanvasLayout().WorldLocalFromCanvas(
      ImVec2(mouse.x / scale, mouse.y / scale), current_world_);
  if (!world_point) {
    status_bar->ClearCustomSegment("Pos");
    return false;
  }
  const int tx = static_cast<int>(world_point->local_position.x / 16.0f) % 32;
  const int ty = static_cast<int>(world_point->local_position.y / 16.0f) % 32;
  status_bar->SetCustomSegment("Pos", absl::StrFormat("(%d, %d)", tx, ty));
  return true;
}

}  // namespace yaze::editor

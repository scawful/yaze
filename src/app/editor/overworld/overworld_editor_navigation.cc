#include "app/editor/overworld/overworld_editor.h"

#include <algorithm>

#include "app/editor/overworld/canvas/canvas_navigation_manager.h"
#include "app/editor/overworld/maps/map_texture_coordinator.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::editor {

bool OverworldEditor::NormalizeCurrentSelectionState() {
  bool changed = false;
  if (overworld_.is_lyra_preview()) {
    const int normalized =
        std::clamp(current_map_, 0, overworld_.inspected_map_count() - 1);
    const int world = normalized / 0x40;
    changed = normalized != current_map_ || world != current_world_;
    current_map_ = normalized;
    current_world_ = world;
  } else {
    changed = NormalizeMapSelection(current_world_, current_map_,
                                    DisplayedMapCount());
  }

  if (!IsProjectTerrainMap(current_map_)) {
    overworld_.set_current_world(current_world_);
    overworld_.set_current_map(current_map_);
  }
  if (const auto* map = overworld_.overworld_map(current_map_)) {
    current_parent_ = map->parent();
  } else {
    current_parent_ = current_map_;
  }

  if (changed) {
    LOG_WARN("OverworldEditor",
             "Normalized stale overworld selection to world=%d map=%d",
             current_world_, current_map_);
  }
  return changed;
}

void OverworldEditor::SelectMapForEditing(int map_id, bool respect_pin) {
  if (respect_pin && current_map_lock_) {
    return;
  }
  if (map_id < 0 || map_id >= DisplayedMapCount()) {
    return;
  }
  const bool project_terrain = IsProjectTerrainMap(map_id);
  const auto* map = overworld_.overworld_map(map_id);
  if (!project_terrain && !map) {
    return;
  }
  if (overworld_.is_lyra_preview()) {
    current_map_ = map_id;
    current_world_ = map_id / 0x40;
    current_parent_ = map->parent();
    overworld_.set_current_map(current_map_);
    overworld_.set_current_world(current_world_);
    status_ = overworld_.EnsureMapBuilt(map_id);
    if (status_.ok()) {
      EnsureMapTexture(map_id);
    }
    return;
  }

  // Cursor tracking may cross screens during one paint stroke. Preserve its
  // undo batch within the same world; explicit selection still finalizes it.
  const bool tracking_paint = respect_pin &&
                              (current_mode == EditingMode::DRAW_TILE ||
                               current_mode == EditingMode::FILL_TILE) &&
                              map_id / 0x40 == current_world_;
  if (!tracking_paint) {
    FinalizePaintOperation();
  }
  if (!tracking_paint || !project_terrain) {
    lyra_canvas_.FinishStroke(undo_manager_);
  }
  current_map_ = map_id;
  current_world_ = std::clamp(map_id / 0x40, 0, 2);
  current_parent_ = project_terrain ? map_id : map->parent();
  if (project_terrain) {
    SetCurrentEntity(nullptr);
    entity_edit_mode_ = EntityEditMode::NONE;
    return;
  }
  overworld_.set_current_map(current_map_);
  overworld_.set_current_world(current_world_);

  if (all_gfx_loaded_) {
    RefreshMapsForSheetEdits();
  }
  status_ = overworld_.EnsureMapBuilt(current_map_);
  if (!status_.ok()) {
    PRINT_IF_ERROR(status_);
    return;
  }

  EnsureMapTexture(current_map_);
  if (all_gfx_loaded_) {
    PRINT_IF_ERROR(RefreshTile16Blockset());
    RefreshMapProperties();
  }
}

void OverworldEditor::PrimeWorldMaps(int world, bool process_texture_queue) {
  if (map_texture_)
    map_texture_->PrimeWorldMaps(world, process_texture_queue);
}

void OverworldEditor::SwitchToWorld(int world) {
  FinalizePaintOperation();
  lyra_canvas_.FinishStroke(undo_manager_);

  const int clamped_world = std::clamp(world, 0, 2);
  const int local_map = current_map_ & 0x3F;
  const int world_start = clamped_world * 0x40;
  const int maps_remaining = DisplayedMapCount() - world_start;
  if (maps_remaining <= 0) {
    return;
  }

  hovered_map_ = -1;
  SelectMapForEditing(world_start + std::min(local_map, maps_remaining - 1),
                      false);
  NormalizeCurrentSelectionState();
  if (IsProjectTerrainMap(current_map_)) {
    return;
  }
  if (overworld_.is_lyra_preview()) {
    PrimeWorldMaps(current_world_);
    return;
  }

  status_ = overworld_.EnsureMapBuilt(current_map_);
  if (!status_.ok()) {
    PRINT_IF_ERROR(status_);
    return;
  }

  EnsureMapTexture(current_map_);
  ForceRefreshGraphics(current_map_);
  RefreshOverworldMapOnDemand(current_map_);
  PRINT_IF_ERROR(RefreshTile16Blockset());
  PrimeWorldMaps(current_world_);
}

void OverworldEditor::SelectAdjacentMap(int dx, int dy) {
  const int world = std::clamp(current_world_, 0, 2);
  const int local = current_map_ - world * 0x40;
  const int rows = std::min(64, DisplayedMapCount() - world * 0x40) / 8;
  const int x = local % 8 + dx;
  const int y = local / 8 + dy;
  if (x < 0 || x >= 8 || y < 0 || y >= rows) {
    return;
  }
  const int target = world * 0x40 + y * 8 + x;
  if (target >= DisplayedMapCount()) {
    return;
  }
  SelectMapForEditing(target, /*respect_pin=*/false);
  if (canvas_nav_) {
    // Keep the keyboard choice until the mouse moves, then resume following
    // the cursor (unless pinned).
    canvas_nav_->SuspendHoverFollowUntilMouseMoves();
    canvas_nav_->CenterOnMap(target);
  }
}

void OverworldEditor::JumpToMap(int map_id) {
  if (map_id < 0 || map_id >= DisplayedMapCount()) {
    return;
  }
  if (!overworld_.is_loaded()) {
    set_current_map(map_id);
    return;
  }
  const int world = std::clamp(map_id / 0x40, 0, 2);
  if (world != current_world_) {
    SwitchToWorld(world);
  }
  SelectMapForEditing(map_id, /*respect_pin=*/false);
  if (canvas_nav_) {
    // Same as keyboard navigation: keep the choice until the mouse moves.
    canvas_nav_->SuspendHoverFollowUntilMouseMoves();
    canvas_nav_->CenterOnMap(map_id);
  }
}

void OverworldEditor::LocateOverworldTile(int map_id, int world_x,
                                          int world_y) {
  JumpToMap(map_id);
  if (canvas_nav_) {
    canvas_nav_->CenterOnWorldTile(world_x, world_y);
  }
}

}  // namespace yaze::editor

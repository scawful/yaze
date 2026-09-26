#include "app/editor/overworld/canvas/overworld_canvas_renderer.h"

#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "app/editor/system/workspace/workspace_window_manager.h"

namespace yaze::editor {

bool OverworldCanvasRenderer::PrepareContextMenu(
    const ImVec2& screen_position) {
  auto& canvas = editor_->ow_map_canvas_;
  canvas.ClearContextMenuItems();
  // Select tool: plain right-click. Paint tools: right-click samples, so
  // only Shift+right-click opens the menu.
  const bool menu_gesture =
      editor_->current_mode == EditingMode::MOUSE || ImGui::GetIO().KeyShift;
  if (!editor_->rom_ || !editor_->rom_->is_loaded() ||
      !editor_->overworld_.is_loaded() || !editor_->map_properties_system_ ||
      !menu_gesture ||
      (editor_->entity_renderer_ &&
       editor_->entity_renderer_->hovered_entity() != nullptr)) {
    return false;
  }
  auto target = ResolveOverworldContextTarget(
      editor_->current_world_, editor_->game_state_, screen_position,
      canvas.zero_point(), canvas.scrolling(), canvas.global_scale());
  if (!target) {
    return false;
  }
  const auto* map = editor_->overworld_.overworld_map(target->map_id);
  if (!map) {
    return false;
  }
  target->parent_map_id =
      map->parent() == 0xFF ? target->map_id : map->parent();
  if (!target->valid()) {
    return false;
  }
  const auto* tiles = editor_->overworld_.mutable_map_tiles();
  const auto& world_tiles = target->world == 0   ? tiles->light_world
                            : target->world == 1 ? tiles->dark_world
                                                 : tiles->special_world;
  const int x = static_cast<int>(target->world_position.x) / 16;
  const int y = static_cast<int>(target->world_position.y) / 16;
  if (x < static_cast<int>(world_tiles.size()) &&
      y < static_cast<int>(world_tiles[x].size())) {
    target->tile16_id = world_tiles[x][y];
  }
  editor_->map_properties_system_->SetupCanvasContextMenu(
      canvas, *target, editor_->current_map_lock_,
      editor_->show_custom_bg_color_editor_, editor_->show_overlay_editor_,
      static_cast<int>(editor_->current_mode), editor_->dependencies_.project,
      editor_->dependencies_.shared_clipboard);
  return true;
}

void OverworldEditor::HandleEntityInsertion(
    const std::string& entity_type, const OverworldContextTarget& target) {
  if (target.valid()) {
    pending_entity_insertion_ =
        OverworldEntityInsertionRequest{entity_type, target};
  }
}

bool OverworldEditor::SampleContextTile16(
    const OverworldContextTarget& target) {
  if (!target.valid() || !overworld_.is_loaded() || !map_blockset_loaded_ ||
      target.tile16_id < 0 ||
      target.tile16_id >= static_cast<int>(overworld_.tiles16().size())) {
    return false;
  }
  // Use the existing switch guard so pending Tile16 edits are preserved.
  RequestTile16Selection(target.tile16_id);
  ScrollBlocksetCanvasToCurrentTile();
  return true;
}

void OverworldEditor::HandleTile16Edit(const OverworldContextTarget& target) {
  if (!SampleContextTile16(target)) {
    return;
  }
  if (dependencies_.window_manager) {
    const size_t session_id =
        dependencies_.window_manager->GetActiveSessionId();
    dependencies_.window_manager->OpenWindowFloating(
        session_id, OverworldPanelIds::kTile16Editor);
    dependencies_.window_manager->MarkWindowRecentlyUsed(
        OverworldPanelIds::kTile16Editor);
  }
}

}  // namespace yaze::editor

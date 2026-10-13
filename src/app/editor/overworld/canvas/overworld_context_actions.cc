#include "app/editor/overworld/canvas/overworld_canvas_renderer.h"

#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "util/i18n/tr.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze::editor {

bool OverworldCanvasRenderer::PrepareContextMenu(
    const ImVec2& screen_position) {
  auto& canvas = editor_->ow_map_canvas_;
  canvas.ClearContextMenuItems();
  if (editor_->overworld_.is_lyra_preview()) {
    return false;
  }
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
      static_cast<int>(editor_->current_mode), editor_->dependencies_.project,
      editor_->dependencies_.shared_clipboard);
  return true;
}

void OverworldEditor::HandleEntityInsertion(
    const std::string& entity_type, const OverworldContextTarget& target) {
  if (!target.valid()) {
    return;
  }
  if (entity_type == "item") {
    RequestItemInsertPopup(target);
    return;
  }
  pending_entity_insertion_ =
      OverworldEntityInsertionRequest{entity_type, target, 0};
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

absl::Status OverworldEditor::OpenGraphicsForContextTile16(
    const OverworldContextTarget& target) {
  if (!target.valid() || !overworld_.is_loaded() || target.tile16_id < 0 ||
      target.tile16_id >= static_cast<int>(overworld_.tiles16().size())) {
    return absl::InvalidArgumentError("The clicked Tile16 is unavailable");
  }
  // Explicit source navigation uses the clicked screen even when another map
  // is pinned. The context target was captured before menu hover can change it.
  const int previous_map = current_map_;
  SelectMapForEditing(target.map_id, /*respect_pin=*/false);
  if (!status_.ok()) {
    const auto failure = status_;
    SelectMapForEditing(previous_map, /*respect_pin=*/false);
    return failure;
  }
  const int quadrant =
      (static_cast<int>(target.world_position.x) % 16 >= 8 ? 1 : 0) +
      (static_cast<int>(target.world_position.y) % 16 >= 8 ? 2 : 0);
  const auto result = OpenGraphicsForTile16Quadrant(target.tile16_id, quadrant);
  if (!result.ok()) {
    // A floating paste or unsupported source must not leave navigation on the
    // clicked map when the Graphics editor refused the new context.
    SelectMapForEditing(previous_map, /*respect_pin=*/false);
  }
  return result;
}

void OverworldEditor::RequestItemInsertPopup(
    const std::optional<OverworldContextTarget>& target) {
  item_insert_target_ = target;
  item_insert_popup_requested_ = true;
}

void OverworldEditor::DrawItemInsertPopup() {
  if (item_insert_popup_requested_) {
    ImGui::OpenPopup("Insert Item");
    item_insert_popup_requested_ = false;
  }
  if (!ImGui::BeginPopup("Insert Item")) {
    return;
  }
  ImGui::TextUnformatted(tr("Choose an item"));
  ImGui::BeginChild("##ItemInsertList", ImVec2(220.0f, 220.0f),
                    ImGuiChildFlags_Borders);
  for (size_t i = 0; i < zelda3::kSecretItemNames.size(); ++i) {
    if (ImGui::Selectable(zelda3::kSecretItemNames[i].c_str())) {
      const auto item_id = static_cast<uint8_t>(i);
      if (item_insert_target_ && item_insert_target_->valid()) {
        pending_entity_insertion_ = OverworldEntityInsertionRequest{
            "item", *item_insert_target_, item_id};
        item_placement_armed_ = false;
      } else {
        armed_item_id_ = item_id;
        item_placement_armed_ = true;
        SetEntityEditMode(EntityEditMode::ITEMS);
      }
      item_insert_target_.reset();
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::EndChild();
  if (!item_insert_target_) {
    ImGui::TextDisabled(tr("Click the map to place the item."));
  }
  ImGui::EndPopup();
}

bool OverworldEditor::TryPlaceArmedItem() {
  if (!item_placement_armed_ || entity_edit_mode_ != EntityEditMode::ITEMS ||
      !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
      !ow_map_canvas_.IsMouseHovering()) {
    return false;
  }
  if (entity_renderer_ && entity_renderer_->hovered_entity() != nullptr) {
    return false;
  }
  const float scale = ow_map_canvas_.global_scale() > 0.0f
                          ? ow_map_canvas_.global_scale()
                          : 1.0f;
  auto target = ResolveOverworldContextTarget(
      current_world_, game_state_, ow_map_canvas_.hover_mouse_pos(),
      ImVec2(0.0f, 0.0f), ImVec2(0.0f, 0.0f), scale);
  if (!target) {
    return false;
  }
  const auto* map = overworld_.overworld_map(target->map_id);
  if (!map) {
    return false;
  }
  target->parent_map_id =
      map->parent() == 0xFF ? target->map_id : map->parent();
  if (!target->valid()) {
    return false;
  }
  pending_entity_insertion_ =
      OverworldEntityInsertionRequest{"item", *target, armed_item_id_};
  item_placement_armed_ = false;
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

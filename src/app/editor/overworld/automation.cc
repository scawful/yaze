#include "app/editor/overworld/overworld_editor.h"

#include "app/editor/overworld/entity_operations.h"
#include "app/editor/overworld/overworld_toolbar.h"
#include "app/editor/system/panel_manager.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/core/popup_id.h"

namespace yaze {
namespace editor {

// ============================================================================
// Canvas Automation API Integration (Phase 4)
// ============================================================================

void OverworldEditor::SetupCanvasAutomation() {
  auto* api = ow_map_canvas_.GetAutomationAPI();

  // Set tile paint callback
  api->SetTilePaintCallback([this](int x, int y, int tile_id) {
    return AutomationSetTile(x, y, tile_id);
  });

  // Set tile query callback
  api->SetTileQueryCallback(
      [this](int x, int y) { return AutomationGetTile(x, y); });
}

bool OverworldEditor::AutomationSetTile(int x, int y, int tile_id) {
  if (!overworld_.is_loaded()) {
    return false;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return false;
  }

  // Set current world based on current_map_
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  // Set the tile in the overworld data structure
  overworld_.SetTile(x, y, static_cast<uint16_t>(tile_id));

  // Update the bitmap
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (!tile_data.empty()) {
    RenderUpdatedMapBitmap(
        ImVec2(static_cast<float>(x * 16), static_cast<float>(y * 16)),
        tile_data);
    return true;
  }

  return false;
}

int OverworldEditor::AutomationGetTile(int x, int y) {
  if (!overworld_.is_loaded()) {
    return -1;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return -1;
  }

  // Set current world
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  return overworld_.GetTile(x, y);
}

void OverworldEditor::HandleEntityInsertion(const std::string& entity_type) {
  // Store for deferred processing outside context menu
  // This is needed because ImGui::OpenPopup() doesn't work correctly when
  // called from within another popup's callback (the context menu)
  pending_entity_insert_type_ = entity_type;
  pending_entity_insert_pos_ = ow_map_canvas_.hover_mouse_pos();
  
  LOG_DEBUG("OverworldEditor",
            "HandleEntityInsertion: queued type='%s' at pos=(%.0f,%.0f)",
            entity_type.c_str(), pending_entity_insert_pos_.x, 
            pending_entity_insert_pos_.y);
}

void OverworldEditor::ProcessPendingEntityInsertion() {
  if (pending_entity_insert_type_.empty()) {
    return;
  }

  if (!overworld_.is_loaded()) {
    LOG_ERROR("OverworldEditor", "Cannot insert entity: overworld not loaded");
    pending_entity_insert_type_.clear();
    return;
  }

  const std::string& entity_type = pending_entity_insert_type_;
  ImVec2 mouse_pos = pending_entity_insert_pos_;

  LOG_DEBUG("OverworldEditor",
            "ProcessPendingEntityInsertion: type='%s' at pos=(%.0f,%.0f) map=%d",
            entity_type.c_str(), mouse_pos.x, mouse_pos.y, current_map_);

  if (entity_type == "entrance") {
    auto result = InsertEntrance(&overworld_, mouse_pos, current_map_, false);
    if (result.ok()) {
      current_entrance_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld,
                           gui::PopupNames::kEntranceEditor)
              .c_str());
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Entrance inserted successfully at map=%d",
                current_map_);
    } else {
      entity_insert_error_message_ = 
          "Cannot insert entrance: " + std::string(result.status().message());
      ImGui::OpenPopup("Entity Insert Error");
      LOG_ERROR("OverworldEditor", "Failed to insert entrance: %s",
                result.status().message().data());
    }

  } else if (entity_type == "hole") {
    auto result = InsertEntrance(&overworld_, mouse_pos, current_map_, true);
    if (result.ok()) {
      current_entrance_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld,
                           gui::PopupNames::kEntranceEditor)
              .c_str());
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Hole inserted successfully at map=%d",
                current_map_);
    } else {
      entity_insert_error_message_ = 
          "Cannot insert hole: " + std::string(result.status().message());
      ImGui::OpenPopup("Entity Insert Error");
      LOG_ERROR("OverworldEditor", "Failed to insert hole: %s",
                result.status().message().data());
    }

  } else if (entity_type == "exit") {
    auto result = InsertExit(&overworld_, mouse_pos, current_map_);
    if (result.ok()) {
      current_exit_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld,
                           gui::PopupNames::kExitEditor)
              .c_str());
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Exit inserted successfully at map=%d",
                current_map_);
    } else {
      entity_insert_error_message_ = 
          "Cannot insert exit: " + std::string(result.status().message());
      ImGui::OpenPopup("Entity Insert Error");
      LOG_ERROR("OverworldEditor", "Failed to insert exit: %s",
                result.status().message().data());
    }

  } else if (entity_type == "item") {
    auto result = InsertItem(&overworld_, mouse_pos, current_map_, 0x00);
    if (result.ok()) {
      current_item_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld,
                           gui::PopupNames::kItemEditor)
              .c_str());
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Item inserted successfully at map=%d",
                current_map_);
    } else {
      entity_insert_error_message_ = 
          "Cannot insert item: " + std::string(result.status().message());
      ImGui::OpenPopup("Entity Insert Error");
      LOG_ERROR("OverworldEditor", "Failed to insert item: %s",
                result.status().message().data());
    }

  } else if (entity_type == "sprite") {
    auto result =
        InsertSprite(&overworld_, mouse_pos, current_map_, game_state_, 0x00);
    if (result.ok()) {
      current_sprite_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld,
                           gui::PopupNames::kSpriteEditor)
              .c_str());
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Sprite inserted successfully at map=%d",
                current_map_);
    } else {
      entity_insert_error_message_ = 
          "Cannot insert sprite: " + std::string(result.status().message());
      ImGui::OpenPopup("Entity Insert Error");
      LOG_ERROR("OverworldEditor", "Failed to insert sprite: %s",
                result.status().message().data());
    }

  } else {
    LOG_WARN("OverworldEditor", "Unknown entity type: %s", entity_type.c_str());
  }

  // Clear the pending state after processing
  pending_entity_insert_type_.clear();
}

void OverworldEditor::HandleTile16Edit() {
  if (!overworld_.is_loaded() || !map_blockset_loaded_) {
    LOG_ERROR("OverworldEditor", "Cannot edit tile16: overworld or blockset not loaded");
    return;
  }

  // Simply open the tile16 editor - don't try to switch tiles here
  // The tile16 editor will use its current tile, user can select a different one
  if (dependencies_.panel_manager) {
    dependencies_.panel_manager->ShowPanel(OverworldPanelIds::kTile16Editor);
  }
}

}  // namespace yaze::editor

}
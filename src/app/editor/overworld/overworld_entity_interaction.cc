// Related header
#include "overworld_entity_interaction.h"

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/gui/core/popup_id.h"

namespace yaze::editor {

void OverworldEntityInteraction::HandleContextMenus(
    zelda3::GameEntity* hovered_entity) {
  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    return;
  }

  if (!hovered_entity) {
    return;
  }

  current_entity_ = hovered_entity;
  switch (hovered_entity->entity_type_) {
    case zelda3::GameEntity::EntityType::kExit:
      current_exit_ = *static_cast<zelda3::OverworldExit*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor").c_str());
      break;
    case zelda3::GameEntity::EntityType::kEntrance:
      current_entrance_ =
          *static_cast<zelda3::OverworldEntrance*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kItem:
      current_item_ = *static_cast<zelda3::OverworldItem*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Item Editor").c_str());
      break;
    case zelda3::GameEntity::EntityType::kSprite:
      current_sprite_ = *static_cast<zelda3::Sprite*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Sprite Editor")
              .c_str());
      break;
    default:
      break;
  }
}

int OverworldEntityInteraction::HandleDoubleClick(
    zelda3::GameEntity* hovered_entity) {
  if (!hovered_entity || !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return -1;
  }

  if (hovered_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
    return static_cast<zelda3::OverworldExit*>(hovered_entity)->room_id_;
  } else if (hovered_entity->entity_type_ ==
             zelda3::GameEntity::EntityType::kEntrance) {
    return static_cast<zelda3::OverworldEntrance*>(hovered_entity)
        ->entrance_id_;
  }
  return -1;
}

bool OverworldEntityInteraction::HandleDragDrop(
    zelda3::GameEntity* hovered_entity, ImVec2 mouse_delta) {
  // Start drag if clicking on an entity
  if (!is_dragging_ && hovered_entity &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    dragged_entity_ = hovered_entity;
    is_dragging_ = true;
    free_movement_ = (dragged_entity_->entity_type_ ==
                      zelda3::GameEntity::EntityType::kExit);
  }

  // Update drag position
  if (is_dragging_ && dragged_entity_ &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // Apply movement delta to entity position
    dragged_entity_->x_ += static_cast<uint16_t>(mouse_delta.x);
    dragged_entity_->y_ += static_cast<uint16_t>(mouse_delta.y);

    // Mark ROM as dirty
    if (rom_) {
      rom_->set_dirty(true);
    }
    return true;
  }

  // End drag on mouse release
  if (is_dragging_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    FinishDrag();
  }

  return is_dragging_;
}

void OverworldEntityInteraction::FinishDrag() {
  is_dragging_ = false;
  free_movement_ = false;
  dragged_entity_ = nullptr;
}

}  // namespace yaze::editor


#include "overworld_entity_renderer.h"

#include "absl/strings/str_format.h"
#include "app/core/features.h"
#include "app/editor/overworld/entity.h"
#include "app/gui/canvas.h"
#include "app/zelda3/common.h"
#include "util/hex.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using namespace ImGui;

// Entity colors - these could be theme-dependent in the future
namespace {
ImVec4 GetEntranceColor() { return ImVec4(0.0f, 1.0f, 0.0f, 0.5f); }  // Green
ImVec4 GetExitColor() { return ImVec4(1.0f, 0.0f, 0.0f, 0.5f); }      // Red
ImVec4 GetItemColor() { return ImVec4(1.0f, 1.0f, 0.0f, 0.5f); }      // Yellow
ImVec4 GetSpriteColor() { return ImVec4(1.0f, 0.5f, 0.0f, 0.5f); }    // Orange
}  // namespace

void OverworldEntityRenderer::DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                           int current_world,
                                           int current_mode) {
  int i = 0;
  for (auto& each : overworld_->entrances()) {
    if (each.map_id_ < 0x40 + (current_world * 0x40) &&
        each.map_id_ >= (current_world * 0x40) && !each.deleted) {
      // Use theme-aware color with proper transparency
      ImVec4 entrance_color = GetEntranceColor();
      if (each.is_hole_) {
        // Holes are more opaque for visibility
        entrance_color.w = 0.78f;  // 200/255 alpha
      }
      canvas_->DrawRect(each.x_, each.y_, 16, 16, entrance_color);
      std::string str = util::HexByte(each.entrance_id_);

      if (current_mode == 1) {  // EditingMode::ENTRANCES
        HandleEntityDragging(&each, canvas_p0, scrolling, is_dragging_entity_,
                             dragged_entity_, current_entity_);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.entrance_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_entrance_id_ = i;
          current_entrance_ = each;
        }
      }

      canvas_->DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  if (DrawEntranceInserterPopup()) {
    // Get the deleted entrance ID and insert it at the mouse position
    auto deleted_entrance_id = overworld_->deleted_entrances().back();
    overworld_->deleted_entrances().pop_back();
    auto& entrance = overworld_->entrances()[deleted_entrance_id];
    entrance.map_id_ = current_map_;
    entrance.entrance_id_ = deleted_entrance_id;
    entrance.x_ = canvas_->hover_mouse_pos().x;
    entrance.y_ = canvas_->hover_mouse_pos().y;
    entrance.deleted = false;
  }

  if (current_mode == 1) {  // EditingMode::ENTRANCES
    const auto is_hovering =
        IsMouseHoveringOverEntity(current_entrance_, canvas_p0, scrolling);

    if (!is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Entrance Inserter");
    } else {
      if (DrawOverworldEntrancePopup(
              overworld_->entrances()[current_entrance_id_])) {
        overworld_->entrances()[current_entrance_id_] = current_entrance_;
      }

      if (overworld_->entrances()[current_entrance_id_].deleted) {
        overworld_->mutable_deleted_entrances()->emplace_back(
            current_entrance_id_);
      }
    }
  }
}

void OverworldEntityRenderer::DrawExits(ImVec2 canvas_p0, ImVec2 scrolling,
                                       int current_world,
                                       int current_mode) {
  int i = 0;
  for (auto& each : *overworld_->mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world * 0x40) &&
        each.map_id_ >= (current_world * 0x40) && !each.deleted_) {
      canvas_->DrawRect(each.x_, each.y_, 16, 16, GetExitColor());
      if (current_mode == 2) {  // EditingMode::EXITS
        each.entity_id_ = i;
        HandleEntityDragging(&each, canvas_->zero_point(),
                             canvas_->scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_, true);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.room_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_exit_id_ = i;
          current_exit_ = each;
          current_entity_ = &each;
          current_entity_->entity_id_ = i;
          ImGui::OpenPopup("Exit editor");
        }
      }

      std::string str = util::HexByte(i);
      canvas_->DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  DrawExitInserterPopup();
  if (current_mode == 2) {  // EditingMode::EXITS
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_->mutable_exits()->at(current_exit_id_),
        canvas_->zero_point(), canvas_->scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Exit Inserter");
    } else {
      if (DrawExitEditorPopup(
              overworld_->mutable_exits()->at(current_exit_id_))) {
        overworld_->mutable_exits()->at(current_exit_id_) = current_exit_;
      }
    }
  }
}

void OverworldEntityRenderer::DrawItems(int current_world, int current_mode) {
  int i = 0;
  for (auto& item : *overworld_->mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world * 0x40) &&
        item.room_map_id_ >= (current_world * 0x40) && !item.deleted) {
      canvas_->DrawRect(item.x_, item.y_, 16, 16, GetItemColor());

      if (current_mode == 3) {  // EditingMode::ITEMS
        // Check if this item is being clicked and dragged
        HandleEntityDragging(&item, canvas_->zero_point(),
                             canvas_->scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);

        const auto hovering = IsMouseHoveringOverEntity(
            item, canvas_->zero_point(), canvas_->scrolling());
        if (hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_item_id_ = i;
          current_item_ = item;
          current_entity_ = &item;
        }
      }
      std::string item_name = "";
      if (item.id_ < zelda3::kSecretItemNames.size()) {
        item_name = zelda3::kSecretItemNames[item.id_];
      } else {
        item_name = absl::StrFormat("0x%02X", item.id_);
      }
      canvas_->DrawText(item_name, item.x_, item.y_);
    }
    i++;
  }

  DrawItemInsertPopup();
  if (current_mode == 3) {  // EditingMode::ITEMS
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_->mutable_all_items()->at(current_item_id_),
        canvas_->zero_point(), canvas_->scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Item Inserter");
    } else {
      if (DrawItemEditorPopup(
              overworld_->mutable_all_items()->at(current_item_id_))) {
        overworld_->mutable_all_items()->at(current_item_id_) = current_item_;
      }
    }
  }
}

void OverworldEntityRenderer::DrawSprites(int current_world, int game_state,
                                         int current_mode) {
  int i = 0;
  for (auto& sprite : *overworld_->mutable_sprites(game_state)) {
    // Filter sprites by current world - only show sprites for the current world
    if (!sprite.deleted() && sprite.map_id() < 0x40 + (current_world * 0x40) &&
        sprite.map_id() >= (current_world * 0x40)) {
      // Sprites are already stored with global coordinates (realX, realY from
      // ROM loading) So we can use sprite.x_ and sprite.y_ directly
      int sprite_x = sprite.x_;
      int sprite_y = sprite.y_;

      // Temporarily update sprite coordinates for entity interaction
      int original_x = sprite.x_;
      int original_y = sprite.y_;

      canvas_->DrawRect(sprite_x, sprite_y, 16, 16, GetSpriteColor());
      if (current_mode == 4) {  // EditingMode::SPRITES
        HandleEntityDragging(&sprite, canvas_->zero_point(),
                             canvas_->scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);
        if (IsMouseHoveringOverEntity(sprite, canvas_->zero_point(),
                                      canvas_->scrolling()) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_sprite_id_ = i;
          current_sprite_ = sprite;
        }
      }
      if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
        if ((*sprite_previews_)[sprite.id()].is_active()) {
          canvas_->DrawBitmap((*sprite_previews_)[sprite.id()], sprite_x,
                             sprite_y, 2.0f);
        }
      }

      canvas_->DrawText(absl::StrFormat("%s", sprite.name()), sprite_x,
                       sprite_y);

      // Restore original coordinates
      sprite.x_ = original_x;
      sprite.y_ = original_y;
    }
    i++;
  }

  DrawSpriteInserterPopup();
  if (current_mode == 4) {  // EditingMode::SPRITES
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_->mutable_sprites(game_state)->at(current_sprite_id_),
        canvas_->zero_point(), canvas_->scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Sprite Inserter");
    } else {
      if (DrawSpriteEditorPopup(overworld_->mutable_sprites(game_state)
                                    ->at(current_sprite_id_))) {
        overworld_->mutable_sprites(game_state)->at(current_sprite_id_) =
            current_sprite_;
      }
    }
  }
}

}  // namespace editor
}  // namespace yaze


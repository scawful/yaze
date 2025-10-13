#include "overworld_entity_renderer.h"

#include "absl/strings/str_format.h"
#include "app/core/features.h"
#include "app/editor/overworld/entity.h"
#include "app/gui/canvas.h"
#include "zelda3/common.h"
#include "util/hex.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using namespace ImGui;

// Entity colors - solid with good visibility
namespace {
ImVec4 GetEntranceColor() { return ImVec4{1.0f, 1.0f, 0.0f, 1.0f}; }      // Solid yellow (#FFFF00FF, fully opaque)
ImVec4 GetExitColor()    { return ImVec4{1.0f, 1.0f, 1.0f, 1.0f}; }        // Solid white (#FFFFFFFF, fully opaque)
ImVec4 GetItemColor()    { return ImVec4{1.0f, 0.0f, 0.0f, 1.0f}; }        // Solid red (#FF0000FF, fully opaque)
ImVec4 GetSpriteColor()  { return ImVec4{1.0f, 0.0f, 1.0f, 1.0f}; }        // Solid magenta (#FF00FFFF, fully opaque)
}  // namespace

void OverworldEntityRenderer::DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                           int current_world,
                                           int current_mode) {
  hovered_entity_ = nullptr;
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
      if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling)) {
        hovered_entity_ = &each;
      }
      std::string str = util::HexByte(each.entrance_id_);





      canvas_->DrawText(str, each.x_, each.y_);
    }
    i++;
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
      if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling)) {
        hovered_entity_ = &each;
      }
      each.entity_id_ = i;
      


      std::string str = util::HexByte(i);
      canvas_->DrawText(str, each.x_, each.y_);
    }
    i++;
  }


}

void OverworldEntityRenderer::DrawItems(int current_world, int current_mode) {
  int i = 0;
  for (auto& item : *overworld_->mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world * 0x40) &&
        item.room_map_id_ >= (current_world * 0x40) && !item.deleted) {
      canvas_->DrawRect(item.x_, item.y_, 16, 16, GetItemColor());

      if (IsMouseHoveringOverEntity(item, canvas_->zero_point(),
                                     canvas_->scrolling())) {
        hovered_entity_ = &item;
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
      if (IsMouseHoveringOverEntity(sprite, canvas_->zero_point(),
                                      canvas_->scrolling())) {
        hovered_entity_ = &sprite;
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


}

}  // namespace editor
}  // namespace yaze


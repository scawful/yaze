#include "overworld_entity_renderer.h"

#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/entity.h"
#include "app/gui/canvas/canvas.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "util/hex.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze {
namespace editor {

using namespace ImGui;

// Entity colors - solid with good visibility
namespace {
ImVec4 GetEntranceColor() {
  return ImVec4{1.0f, 1.0f, 0.0f, 1.0f};
}  // Solid yellow (#FFFF00FF, fully opaque)
ImVec4 GetExitColor() {
  return ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
}  // Solid white (#FFFFFFFF, fully opaque)
ImVec4 GetItemColor() {
  return ImVec4{1.0f, 0.0f, 0.0f, 1.0f};
}  // Solid red (#FF0000FF, fully opaque)
ImVec4 GetSpriteColor() {
  return ImVec4{1.0f, 0.0f, 1.0f, 1.0f};
}  // Solid magenta (#FF00FFFF, fully opaque)
ImVec4 GetDiggableTileColor() {
  return ImVec4{0.6f, 0.4f, 0.2f, 0.5f};
}  // Semi-transparent brown for diggable ground
}  // namespace

// =============================================================================
// Modern CanvasRuntime-based rendering methods (Phase 2)
// =============================================================================

void OverworldEntityRenderer::DrawEntrances(const gui::CanvasRuntime& rt,
                                            int current_world) {
  // Don't reset hovered_entity_ here - DrawExits resets it (called first)
  for (auto& each : overworld_->entrances()) {
    if (each.map_id_ < 0x40 + (current_world * 0x40) &&
        each.map_id_ >= (current_world * 0x40) && !each.deleted) {
      ImVec4 entrance_color = GetEntranceColor();
      if (each.is_hole_) {
        entrance_color.w = 0.78f;
      }
      gui::DrawRect(rt, each.x_, each.y_, 16, 16, entrance_color);
      if (IsMouseHoveringOverEntity(each, rt)) {
        hovered_entity_ = &each;
      }
      std::string str = util::HexByte(each.entrance_id_);
      gui::DrawText(rt, str, each.x_, each.y_);
    }
  }
}

void OverworldEntityRenderer::DrawExits(const gui::CanvasRuntime& rt,
                                        int current_world) {
  // Reset hover state at the start of entity rendering (DrawExits is called first)
  hovered_entity_ = nullptr;

  int i = 0;
  for (auto& each : *overworld_->mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world * 0x40) &&
        each.map_id_ >= (current_world * 0x40) && !each.deleted_) {
      gui::DrawRect(rt, each.x_, each.y_, 16, 16, GetExitColor());
      if (IsMouseHoveringOverEntity(each, rt)) {
        hovered_entity_ = &each;
      }
      each.entity_id_ = i;
      std::string str = util::HexByte(i);
      gui::DrawText(rt, str, each.x_, each.y_);
    }
    i++;
  }
}

void OverworldEntityRenderer::DrawItems(const gui::CanvasRuntime& rt,
                                        int current_world) {
  for (auto& item : *overworld_->mutable_all_items()) {
    if (item.room_map_id_ < 0x40 + (current_world * 0x40) &&
        item.room_map_id_ >= (current_world * 0x40) && !item.deleted) {
      gui::DrawRect(rt, item.x_, item.y_, 16, 16, GetItemColor());
      if (IsMouseHoveringOverEntity(item, rt)) {
        hovered_entity_ = &item;
      }
      std::string item_name = "";
      if (item.id_ < zelda3::kSecretItemNames.size()) {
        item_name = zelda3::kSecretItemNames[item.id_];
      } else {
        item_name = absl::StrFormat("0x%02X", item.id_);
      }
      gui::DrawText(rt, item_name, item.x_, item.y_);
    }
  }
}

void OverworldEntityRenderer::DrawSprites(const gui::CanvasRuntime& rt,
                                          int current_world, int game_state) {
  for (auto& sprite : *overworld_->mutable_sprites(game_state)) {
    if (!sprite.deleted() && sprite.map_id() < 0x40 + (current_world * 0x40) &&
        sprite.map_id() >= (current_world * 0x40)) {
      int sprite_x = sprite.x_;
      int sprite_y = sprite.y_;

      gui::DrawRect(rt, sprite_x, sprite_y, 16, 16, GetSpriteColor());
      if (IsMouseHoveringOverEntity(sprite, rt)) {
        hovered_entity_ = &sprite;
      }

      if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
        if ((*sprite_previews_)[sprite.id()].is_active()) {
          // For bitmap drawing, we still use the canvas pointer for now
          // as runtime-based bitmap drawing needs the texture
          canvas_->DrawBitmap((*sprite_previews_)[sprite.id()], sprite_x,
                              sprite_y, 2.0f);
        }
      }

      gui::DrawText(rt, absl::StrFormat("%s", sprite.name()), sprite_x,
                    sprite_y);
    }
  }
}

// =============================================================================
// Legacy rendering methods (kept for backward compatibility)
// =============================================================================

void OverworldEntityRenderer::DrawEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                            int current_world,
                                            int current_mode) {
  // Don't reset hovered_entity_ here - DrawExits resets it (called first)
  float scale = canvas_->global_scale();
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
      if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling, scale)) {
        hovered_entity_ = &each;
      }
      std::string str = util::HexByte(each.entrance_id_);

      canvas_->DrawText(str, each.x_, each.y_);
    }
    i++;
  }
}

void OverworldEntityRenderer::DrawExits(ImVec2 canvas_p0, ImVec2 scrolling,
                                        int current_world, int current_mode) {
  // Reset hover state at the start of entity rendering (DrawExits is called
  // first)
  hovered_entity_ = nullptr;
  float scale = canvas_->global_scale();

  int i = 0;
  for (auto& each : *overworld_->mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world * 0x40) &&
        each.map_id_ >= (current_world * 0x40) && !each.deleted_) {
      canvas_->DrawRect(each.x_, each.y_, 16, 16, GetExitColor());

      if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling, scale)) {
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
  float scale = canvas_->global_scale();
  int i = 0;
  for (auto& item : *overworld_->mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world * 0x40) &&
        item.room_map_id_ >= (current_world * 0x40) && !item.deleted) {
      canvas_->DrawRect(item.x_, item.y_, 16, 16, GetItemColor());

      if (IsMouseHoveringOverEntity(item, canvas_->zero_point(),
                                    canvas_->scrolling(), scale)) {
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
  float scale = canvas_->global_scale();
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
                                    canvas_->scrolling(), scale)) {
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

void OverworldEntityRenderer::DrawDiggableTileHighlights(int current_world,
                                                         int current_map) {
  if (!show_diggable_tiles_) {
    return;
  }

  const auto& diggable_tiles = overworld_->diggable_tiles();
  const auto& map_tiles = overworld_->GetMapTiles(current_world);

  // Calculate map bounds based on current_map
  // Each map is 32x32 tiles (512x512 pixels)
  // Maps are arranged in an 8x8 grid per world
  int map_x = (current_map % 8) * 32;  // Tile position in world
  int map_y = (current_map / 8) * 32;

  // Iterate through the 32x32 tiles in this map
  for (int ty = 0; ty < 32; ++ty) {
    for (int tx = 0; tx < 32; ++tx) {
      int world_tx = map_x + tx;
      int world_ty = map_y + ty;

      // Get the Map16 tile ID at this position
      // Map tiles are stored [y][x]
      if (world_ty >= 256 || world_tx >= 256) {
        continue;  // Out of bounds
      }

      uint16_t tile_id = map_tiles[world_ty][world_tx];

      // Check if this tile is marked as diggable
      if (diggable_tiles.IsDiggable(tile_id)) {
        // Calculate pixel position (each tile is 16x16 pixels)
        int pixel_x = world_tx * 16;
        int pixel_y = world_ty * 16;

        // Draw a semi-transparent highlight
        canvas_->DrawRect(pixel_x, pixel_y, 16, 16, GetDiggableTileColor());
      }
    }
  }
}

}  // namespace editor
}  // namespace yaze

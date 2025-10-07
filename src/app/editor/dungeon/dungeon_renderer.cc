#include "dungeon_renderer.h"

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"

namespace yaze::editor {

void DungeonRenderer::RenderObjectInCanvas(const zelda3::RoomObject& object,
                                           const gfx::SnesPalette& palette) {
  // Validate ROM is loaded
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Convert room coordinates to canvas coordinates
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Check if object is within canvas bounds
  if (!IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
    return;  // Skip objects outside visible area
  }

  // Calculate palette hash for caching
  uint64_t palette_hash = 0;
  for (size_t i = 0; i < palette.size() && i < 16; ++i) {
    palette_hash ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 +
                    (palette_hash << 6) + (palette_hash >> 2);
  }

  // Check cache first
  for (auto& cached : object_render_cache_) {
    if (cached.object_id == object.id_ && cached.object_x == object.x_ &&
        cached.object_y == object.y_ && cached.object_size == object.size_ &&
        cached.palette_hash == palette_hash && cached.is_valid) {
      canvas_->DrawBitmap(cached.rendered_bitmap, canvas_x, canvas_y, 1.0f, 255);
      return;
    }
  }

  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Try to render the object with proper graphics
  auto render_result = object_renderer_.RenderObject(mutable_object, palette);
  if (render_result.ok()) {
    auto object_bitmap = std::move(render_result.value());
    
    // Ensure the bitmap is valid and has meaningful content
    if (object_bitmap.width() > 0 && object_bitmap.height() > 0 && 
        object_bitmap.data() != nullptr) {
      object_bitmap.SetPalette(palette);
      // Queue texture creation for the object bitmap via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &object_bitmap);
      canvas_->DrawBitmap(object_bitmap, canvas_x, canvas_y, 1.0f, 255);
      // Cache the successfully rendered bitmap
      ObjectRenderCache cache_entry;
      cache_entry.object_id = object.id_;
      cache_entry.object_x = object.x_;
      cache_entry.object_y = object.y_;
      cache_entry.object_size = object.size_;
      cache_entry.palette_hash = palette_hash;
      cache_entry.rendered_bitmap = object_bitmap;
      cache_entry.is_valid = true;

      // Add to cache (limit cache size)
      if (object_render_cache_.size() >= 100) {
        object_render_cache_.erase(object_render_cache_.begin());
      }
      object_render_cache_.push_back(std::move(cache_entry));
      return;
    }
  }
  
  // Fallback: Draw object as colored rectangle with ID if rendering fails
  ImVec4 object_color;
  
  // Color-code objects based on layer for better identification
  switch (object.layer_) {
    case zelda3::RoomObject::LayerType::BG1:
      object_color = ImVec4(0.8f, 0.4f, 0.4f, 0.8f); // Red-ish for BG1
      break;
    case zelda3::RoomObject::LayerType::BG2:
      object_color = ImVec4(0.4f, 0.8f, 0.4f, 0.8f); // Green-ish for BG2
      break;
    case zelda3::RoomObject::LayerType::BG3:
      object_color = ImVec4(0.4f, 0.4f, 0.8f, 0.8f); // Blue-ish for BG3
      break;
    default:
      object_color = ImVec4(0.6f, 0.6f, 0.6f, 0.8f); // Gray for unknown
      break;
  }
  
  // Calculate object size (16x16 is base, size affects width/height)
  int object_width = 16 + (object.size_ & 0x0F) * 8;
  int object_height = 16 + ((object.size_ >> 4) & 0x0F) * 8;
  
  canvas_->DrawRect(canvas_x, canvas_y, object_width, object_height, object_color);
  canvas_->DrawRect(canvas_x, canvas_y, object_width, object_height, 
                   ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black border
}

void DungeonRenderer::DisplayObjectInfo(const zelda3::RoomObject& object,
                                         int canvas_x, int canvas_y) {
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);
  canvas_->DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonRenderer::RenderSprites(const zelda3::Room& room) {
  // Render sprites as simple 16x16 squares with sprite name/ID  
  for (const auto& sprite : room.GetSprites()) {
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(sprite.x(), sprite.y());
    
    if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      // Draw 16x16 square for sprite
      ImVec4 sprite_color;
      
      // Color-code sprites based on layer for identification
      if (sprite.layer() == 0) {
        sprite_color = ImVec4(0.2f, 0.8f, 0.2f, 0.8f); // Green for layer 0
      } else {
        sprite_color = ImVec4(0.2f, 0.2f, 0.8f, 0.8f); // Blue for layer 1
      }
      
      canvas_->DrawRect(canvas_x, canvas_y, 16, 16, sprite_color);
      canvas_->DrawRect(canvas_x, canvas_y, 16, 16, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Border
    }
  }
}

void DungeonRenderer::RenderRoomBackgroundLayers(int room_id) {
  // Get canvas dimensions to limit rendering
  int canvas_width = canvas_->width();
  int canvas_height = canvas_->height();
  
  // Validate canvas dimensions
  if (canvas_width <= 0 || canvas_height <= 0) {
    return;
  }
  
  // BG1 (background layer 1) - main room graphics
  auto& bg1_bitmap = gfx::Arena::Get().bg1().bitmap();
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0 &&
      bg1_bitmap.height() > 0) {
    float scale_x = static_cast<float>(canvas_width) / bg1_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg1_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg1_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg1_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_->DrawBitmap(bg1_bitmap, offset_x, offset_y, scale, 255);
  }
  
  // BG2 (background layer 2) - sprite graphics (overlay)
  auto& bg2_bitmap = gfx::Arena::Get().bg2().bitmap();
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0 &&
      bg2_bitmap.height() > 0) {
    float scale_x = static_cast<float>(canvas_width) / bg2_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg2_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg2_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg2_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_->DrawBitmap(bg2_bitmap, offset_x, offset_y, scale, 200);
  }
}

absl::Status DungeonRenderer::RefreshGraphics(int room_id, uint64_t palette_id,
                                              const gfx::PaletteGroup& palette_group) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // This would need access to room data - will be called from main editor
  return absl::OkStatus();
}

std::pair<int, int> DungeonRenderer::RoomToCanvasCoordinates(int room_x, int room_y) const {
  // Dungeon tiles are 8x8 pixels, convert room coordinates (tiles) to pixels
  return {room_x * 8, room_y * 8};
}

std::pair<int, int> DungeonRenderer::CanvasToRoomCoordinates(int canvas_x, int canvas_y) const {
  // Convert canvas pixels back to room coordinates (tiles)
  return {canvas_x / 8, canvas_y / 8};
}

bool DungeonRenderer::IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin) const {
  auto canvas_size = canvas_->canvas_size();
  auto global_scale = canvas_->global_scale();
  int scaled_width = static_cast<int>(canvas_size.x * global_scale);
  int scaled_height = static_cast<int>(canvas_size.y * global_scale);
  
  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= scaled_width + margin &&
          canvas_y <= scaled_height + margin);
}

}  // namespace yaze::editor

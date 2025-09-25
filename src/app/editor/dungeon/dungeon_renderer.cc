#include "dungeon_renderer.h"

#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gui/color.h"

namespace yaze::editor {

using core::Renderer;

void DungeonRenderer::RenderObjectInCanvas(const zelda3::RoomObject& object,
                                           const gfx::SnesPalette& palette) {
  // Validate ROM is loaded
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Check if tiles were loaded successfully
  if (mutable_object.tiles().empty()) {
    return;  // Skip objects without tiles
  }

  // Calculate palette hash for caching
  uint64_t palette_hash = 0;
  for (size_t i = 0; i < palette.size() && i < 16; ++i) {
    palette_hash ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 +
                    (palette_hash << 6) + (palette_hash >> 2);
  }

  // Convert room coordinates to canvas coordinates
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Check if object is within canvas bounds
  if (!IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
    return;  // Skip objects outside visible area
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

  // Render the object to a bitmap
  auto render_result = object_renderer_.RenderObject(mutable_object, palette);
  if (!render_result.ok()) {
    return;  // Skip if rendering failed
  }

  auto object_bitmap = std::move(render_result.value());
  object_bitmap.SetPalette(palette);
  core::Renderer::Get().RenderBitmap(&object_bitmap);

  // Draw the object bitmap to the canvas
  canvas_->DrawBitmap(object_bitmap, canvas_x, canvas_y, 1.0f, 255);

  // Cache the rendered bitmap
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
}

void DungeonRenderer::DisplayObjectInfo(const zelda3::RoomObject& object,
                                         int canvas_x, int canvas_y) {
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);
  canvas_->DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonRenderer::RenderLayoutObjects(const zelda3::RoomLayout& layout,
                                          const gfx::SnesPalette& palette) {
  for (const auto& layout_obj : layout.GetObjects()) {
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(layout_obj.x(), layout_obj.y());

    if (!IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      continue;
    }

    // Choose color based on object type
    gfx::SnesColor color;
    switch (layout_obj.type()) {
      case zelda3::RoomLayoutObject::Type::kWall:
        color = gfx::SnesColor(0x7FFF);  // Gray
        break;
      case zelda3::RoomLayoutObject::Type::kFloor:
        color = gfx::SnesColor(0x4210);  // Dark brown
        break;
      case zelda3::RoomLayoutObject::Type::kCeiling:
        color = gfx::SnesColor(0x739C);  // Light gray
        break;
      case zelda3::RoomLayoutObject::Type::kPit:
        color = gfx::SnesColor(0x0000);  // Black
        break;
      case zelda3::RoomLayoutObject::Type::kWater:
        color = gfx::SnesColor(0x001F);  // Blue
        break;
      case zelda3::RoomLayoutObject::Type::kStairs:
        color = gfx::SnesColor(0x7E0F);  // Yellow
        break;
      case zelda3::RoomLayoutObject::Type::kDoor:
        color = gfx::SnesColor(0xF800);  // Red
        break;
      default:
        color = gfx::SnesColor(0x7C1F);  // Magenta for unknown
        break;
    }

    canvas_->DrawRect(canvas_x, canvas_y, 16, 16,
                      gui::ConvertSnesColorToImVec4(color));
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
  return {room_x * 16, room_y * 16};
}

std::pair<int, int> DungeonRenderer::CanvasToRoomCoordinates(int canvas_x, int canvas_y) const {
  return {canvas_x / 16, canvas_y / 16};
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

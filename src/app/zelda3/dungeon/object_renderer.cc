#include "object_renderer.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"

namespace yaze {
namespace zelda3 {

absl::StatusOr<gfx::Bitmap> ObjectRenderer::RenderObject(
    const RoomObject& object, const gfx::SnesPalette& palette) {
  
  // Ensure object has tiles loaded
  if (object.tiles().empty()) {
    return absl::FailedPreconditionError("Object has no tiles loaded");
  }

  // Create bitmap for the object
  gfx::Bitmap bitmap = CreateBitmap(32, 32); // Default 32x32 pixels

  // Render each tile
  for (size_t i = 0; i < object.tiles().size(); ++i) {
    int tile_x = (i % 2) * 16; // 2 tiles per row
    int tile_y = (i / 2) * 16;
    
    auto status = RenderTile(object.tiles()[i], bitmap, tile_x, tile_y, palette);
    if (!status.ok()) {
      return status;
    }
  }

  return bitmap;
}

absl::StatusOr<gfx::Bitmap> ObjectRenderer::RenderObjects(
    const std::vector<RoomObject>& objects, const gfx::SnesPalette& palette,
    int width, int height) {
  
  gfx::Bitmap bitmap = CreateBitmap(width, height);

  for (const auto& object : objects) {
    if (object.tiles().empty()) {
      continue; // Skip objects without tiles
    }

    // Calculate object position in the bitmap
    int obj_x = object.x_ * 16; // Convert room coordinates to pixel coordinates
    int obj_y = object.y_ * 16;

    // Render each tile of the object
    for (size_t i = 0; i < object.tiles().size(); ++i) {
      int tile_x = obj_x + (i % 2) * 16;
      int tile_y = obj_y + (i / 2) * 16;
      
      // Check bounds
      if (tile_x >= 0 && tile_x < width && tile_y >= 0 && tile_y < height) {
        auto status = RenderTile(object.tiles()[i], bitmap, tile_x, tile_y, palette);
        if (!status.ok()) {
          return status;
        }
      }
    }
  }

  return bitmap;
}

absl::StatusOr<gfx::Bitmap> ObjectRenderer::RenderObjectWithSize(
    const RoomObject& object, const gfx::SnesPalette& palette,
    const ObjectSizeInfo& size_info) {
  
  if (object.tiles().empty()) {
    return absl::FailedPreconditionError("Object has no tiles loaded");
  }

  // Calculate bitmap size based on object size
  int bitmap_width = size_info.width_tiles * 16;
  int bitmap_height = size_info.height_tiles * 16;
  
  gfx::Bitmap bitmap = CreateBitmap(bitmap_width, bitmap_height);

  // Render tiles based on orientation
  if (size_info.is_horizontal) {
    // Horizontal rendering
    for (int repeat = 0; repeat < size_info.repeat_count; ++repeat) {
      for (size_t i = 0; i < object.tiles().size(); ++i) {
        int tile_x = (repeat * 2) + (i % 2);
        int tile_y = i / 2;
        
        if (tile_x < size_info.width_tiles && tile_y < size_info.height_tiles) {
          auto status = RenderTile(object.tiles()[i], bitmap, 
                                 tile_x * 16, tile_y * 16, palette);
          if (!status.ok()) {
            return status;
          }
        }
      }
    }
  } else {
    // Vertical rendering
    for (int repeat = 0; repeat < size_info.repeat_count; ++repeat) {
      for (size_t i = 0; i < object.tiles().size(); ++i) {
        int tile_x = i % 2;
        int tile_y = (repeat * 2) + (i / 2);
        
        if (tile_x < size_info.width_tiles && tile_y < size_info.height_tiles) {
          auto status = RenderTile(object.tiles()[i], bitmap, 
                                 tile_x * 16, tile_y * 16, palette);
          if (!status.ok()) {
            return status;
          }
        }
      }
    }
  }

  return bitmap;
}

absl::StatusOr<gfx::Bitmap> ObjectRenderer::GetObjectPreview(
    const RoomObject& object, const gfx::SnesPalette& palette) {
  
  if (object.tiles().empty()) {
    return absl::FailedPreconditionError("Object has no tiles loaded");
  }

  // Create a smaller preview bitmap (16x16 pixels)
  gfx::Bitmap bitmap = CreateBitmap(16, 16);

  // Render only the first tile as a preview
  auto status = RenderTile(object.tiles()[0], bitmap, 0, 0, palette);
  if (!status.ok()) {
    return status;
  }

  return bitmap;
}

absl::Status ObjectRenderer::RenderTile(const gfx::Tile16& tile, 
                                               gfx::Bitmap& bitmap,
                                               int x, int y, 
                                               const gfx::SnesPalette& palette) {
  
  // Check if bitmap is valid
  if (!bitmap.is_active() || bitmap.surface() == nullptr) {
    return absl::FailedPreconditionError("Bitmap is not properly initialized");
  }
  
  // Get the graphics sheet from Arena - this contains the actual pixel data
  auto& arena = gfx::Arena::Get();
  
  // Render the 4 sub-tiles of the Tile16
  std::array<gfx::TileInfo, 4> sub_tiles = {
    tile.tile0_, tile.tile1_, tile.tile2_, tile.tile3_
  };

  for (int i = 0; i < 4; ++i) {
    const auto& tile_info = sub_tiles[i];
    int sub_x = x + (i % 2) * 8;
    int sub_y = y + (i / 2) * 8;
    
    // Get the graphics sheet that contains this tile
    // Tile IDs are typically organized in sheets of 256 tiles each
    int sheet_index = tile_info.id_ / 256;
    if (sheet_index >= 223) { // Arena has 223 graphics sheets
      sheet_index = 0; // Fallback to first sheet
    }
    
    auto graphics_sheet = arena.gfx_sheet(sheet_index);
    if (!graphics_sheet.is_active()) {
      // If graphics sheet is not loaded, create a simple pattern
      RenderTilePattern(bitmap, sub_x, sub_y, tile_info, palette);
      continue;
    }
    
    // Calculate tile position within the graphics sheet
    int tile_x = (tile_info.id_ % 16) * 8; // 16 tiles per row, 8 pixels per tile
    int tile_y = ((tile_info.id_ % 256) / 16) * 8; // 16 rows per sheet
    
    // Render the 8x8 tile from the graphics sheet
    for (int py = 0; py < 8; ++py) {
      for (int px = 0; px < 8; ++px) {
        if (sub_x + px < bitmap.width() && sub_y + py < bitmap.height()) {
          // Get pixel from graphics sheet
          int src_x = tile_x + px;
          int src_y = tile_y + py;
          
          if (src_x < graphics_sheet.width() && src_y < graphics_sheet.height()) {
            int pixel_index = src_y * graphics_sheet.width() + src_x;
            if (pixel_index < (int)graphics_sheet.size()) {
              uint8_t color_index = graphics_sheet.at(pixel_index);
              
              // Apply palette
              if (color_index < palette.size()) {
                // Apply mirroring if needed
                int final_x = sub_x + px;
                int final_y = sub_y + py;
                
                if (tile_info.horizontal_mirror_) {
                  final_x = sub_x + (7 - px);
                }
                if (tile_info.vertical_mirror_) {
                  final_y = sub_y + (7 - py);
                }
                
                if (final_x < bitmap.width() && final_y < bitmap.height()) {
                  bitmap.SetPixel(final_x, final_y, palette[color_index]);
                }
              }
            }
          }
        }
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectRenderer::ApplyObjectSize(gfx::Bitmap& bitmap, 
                                                   const ObjectSizeInfo& size_info) {
  // This method would apply size and orientation transformations
  // For now, it's a placeholder
  return absl::OkStatus();
}

gfx::Bitmap ObjectRenderer::CreateBitmap(int width, int height) {
  // Create a bitmap with proper initialization
  std::vector<uint8_t> data(width * height, 0); // Initialize with zeros
  gfx::Bitmap bitmap(width, height, 8, data); // 8-bit depth
  return bitmap;
}

void ObjectRenderer::RenderTilePattern(gfx::Bitmap& bitmap, int x, int y, 
                                      const gfx::TileInfo& tile_info, 
                                      const gfx::SnesPalette& palette) {
  // Create a simple pattern based on tile ID and palette
  // This is used when the graphics sheet is not available
  
  for (int py = 0; py < 8; ++py) {
    for (int px = 0; px < 8; ++px) {
      if (x + px < bitmap.width() && y + py < bitmap.height()) {
        // Create a simple pattern based on tile ID
        int pattern_value = (tile_info.id_ + px + py) % 16;
        
        // Use different colors based on the pattern
        int color_index = pattern_value % palette.size();
        if (color_index > 0) { // Skip transparent color (index 0)
          bitmap.SetPixel(x + px, y + py, palette[color_index]);
        }
      }
    }
  }
}

}  // namespace zelda3
}  // namespace yaze
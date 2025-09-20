#ifndef YAZE_APP_ZELDA3_DUNGEON_IMPROVED_OBJECT_RENDERER_H
#define YAZE_APP_ZELDA3_DUNGEON_IMPROVED_OBJECT_RENDERER_H

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_parser.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Improved dungeon object renderer using direct ROM parsing
 * 
 * This class replaces the SNES emulation approach with direct ROM parsing,
 * providing better performance, reliability, and maintainability.
 */
class ImprovedObjectRenderer {
 public:
  explicit ImprovedObjectRenderer(Rom* rom) : rom_(rom), parser_(rom) {}

  /**
   * @brief Render a single object to a bitmap
   * 
   * @param object The room object to render
   * @param palette The palette to use for rendering
   * @return StatusOr containing the rendered bitmap
   */
  absl::StatusOr<gfx::Bitmap> RenderObject(const RoomObject& object, 
                                          const gfx::SnesPalette& palette);

  /**
   * @brief Render multiple objects to a single bitmap
   * 
   * @param objects Vector of room objects to render
   * @param palette The palette to use for rendering
   * @param width Width of the output bitmap
   * @param height Height of the output bitmap
   * @return StatusOr containing the rendered bitmap
   */
  absl::StatusOr<gfx::Bitmap> RenderObjects(const std::vector<RoomObject>& objects,
                                           const gfx::SnesPalette& palette,
                                           int width = 256, int height = 256);

  /**
   * @brief Render object with size and orientation
   * 
   * @param object The room object to render
   * @param palette The palette to use for rendering
   * @param size_info Size and orientation information
   * @return StatusOr containing the rendered bitmap
   */
  absl::StatusOr<gfx::Bitmap> RenderObjectWithSize(const RoomObject& object,
                                                  const gfx::SnesPalette& palette,
                                                  const ObjectSizeInfo& size_info);

  /**
   * @brief Get object preview (smaller version for UI)
   * 
   * @param object The room object to preview
   * @param palette The palette to use
   * @return StatusOr containing the preview bitmap
   */
  absl::StatusOr<gfx::Bitmap> GetObjectPreview(const RoomObject& object,
                                              const gfx::SnesPalette& palette);

 private:
  /**
   * @brief Render a single tile to the bitmap
   */
  absl::Status RenderTile(const gfx::Tile16& tile, gfx::Bitmap& bitmap,
                         int x, int y, const gfx::SnesPalette& palette);

  /**
   * @brief Apply object size and orientation
   */
  absl::Status ApplyObjectSize(gfx::Bitmap& bitmap, const ObjectSizeInfo& size_info);

  /**
   * @brief Create a bitmap with the specified dimensions
   */
  gfx::Bitmap CreateBitmap(int width, int height);

  Rom* rom_;
  ObjectParser parser_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_IMPROVED_OBJECT_RENDERER_H
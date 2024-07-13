#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include "absl/status/status.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @class SpriteEditor
 * @brief Allows the user to edit sprites.
 *
 * This class provides functionality for updating the sprite editor, drawing the
 * editor table, drawing the sprite canvas, and drawing the current sheets.
 */
class SpriteEditor : public SharedRom {
 public:
  /**
   * @brief Updates the sprite editor.
   *
   * @return An absl::Status indicating the success or failure of the update.
   */
  absl::Status Update();

 private:
  /**
   * @brief Draws the sprite canvas.
   */
  void DrawSpriteCanvas();

  /**
   * @brief Draws the current sheets.
   */
  void DrawCurrentSheets();

  uint8_t current_sheets_[8]; /**< Array to store the current sheets. */
  bool sheets_loaded_ =
      false; /**< Flag indicating whether the sheets are loaded or not. */

  // OAM Configuration
  struct OAMConfig {
    uint16_t x;       /**< X offset. */
    uint16_t y;       /**< Y offset. */
    uint8_t tile;     /**< Tile number. */
    uint8_t palette;  /**< Palette number. */
    uint8_t priority; /**< Priority. */
    bool flip_x;      /**< Flip X. */
    bool flip_y;      /**< Flip Y. */
  };

  OAMConfig oam_config_; /**< OAM configuration. */
  gui::Bitmap oam_bitmap_; /**< OAM bitmap. */

  gui::Canvas sprite_canvas_{
      ImVec2(0x200, 0x200), gui::CanvasGridSize::k32x32}; /**< Sprite canvas. */
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H
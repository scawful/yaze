#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include "absl/status/status.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @brief The SpriteEditor class represents a sprite editor that inherits from
 * SharedROM.
 *
 * This class provides functionality for updating the sprite editor, drawing the
 * editor table, drawing the sprite canvas, and drawing the current sheets.
 */
class SpriteEditor : public SharedROM {
 public:
  /**
   * @brief Updates the sprite editor.
   *
   * @return An absl::Status indicating the success or failure of the update.
   */
  absl::Status Update();

 private:
  /**
   * @brief Draws the editor table.
   */
  void DrawEditorTable();

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
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H
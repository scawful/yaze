#ifndef YAZE_APP_EDITOR_SPRITE_EDITOR_H
#define YAZE_APP_EDITOR_SPRITE_EDITOR_H

#include "absl/status/status.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {



constexpr ImGuiTabItemFlags kSpriteTabFlags =
    ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip;

constexpr ImGuiTabBarFlags kSpriteTabBarFlags =
    ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
    ImGuiTabBarFlags_FittingPolicyResizeDown |
    ImGuiTabBarFlags_TabListPopupButton;

constexpr ImGuiTableFlags kSpriteTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

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
   * @brief Draws the sprites list.
   */
  void DrawSpritesList();

  /**
   * @brief Draws the sprite canvas.
   */
  void DrawSpriteCanvas();

  /**
   * @brief Draws the current sheets.
   */
  void DrawCurrentSheets();

  ImVector<int> active_sprites_; /**< Active sprites. */

  int current_sprite_id_;     /**< Current sprite ID. */
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

  OAMConfig oam_config_;   /**< OAM configuration. */
  gui::Bitmap oam_bitmap_; /**< OAM bitmap. */

  gui::Canvas sprite_canvas_{
      ImVec2(0x200, 0x200), gui::CanvasGridSize::k32x32}; /**< Sprite canvas. */

  gui::Canvas graphics_sheet_canvas_{
      ImVec2(0x80 * 2 + 2, 0x40 * 8 + 2),
      gui::CanvasGridSize::k16x16}; /**< Graphics sheet canvas. */
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_EDITOR_H
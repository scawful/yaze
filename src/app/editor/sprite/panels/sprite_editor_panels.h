#ifndef YAZE_APP_EDITOR_SPRITE_PANELS_SPRITE_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_SPRITE_PANELS_SPRITE_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for SpriteEditor panels
// =============================================================================

/**
 * @brief EditorPanel for Vanilla Sprite Editor
 *
 * Displays the vanilla sprite browser and editor for ROM sprites.
 * Includes sprite list, canvas preview, and tile selector.
 */
class VanillaSpriteEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit VanillaSpriteEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "sprite.vanilla_editor"; }
  std::string GetDisplayName() const override { return "Vanilla Sprites"; }
  std::string GetIcon() const override { return ICON_MD_SMART_TOY; }
  std::string GetEditorCategory() const override { return "Sprite"; }
  int GetPriority() const override { return 10; }
  bool IsVisibleByDefault() const override { return true; }
  float GetPreferredWidth() const override { return 900.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Custom Sprite Editor (ZSM format)
 *
 * Allows creating and editing custom sprites in ZSM format.
 * Includes animation editor, properties panel, and user routines.
 */
class CustomSpriteEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit CustomSpriteEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "sprite.custom_editor"; }
  std::string GetDisplayName() const override { return "Custom Sprites"; }
  std::string GetIcon() const override { return ICON_MD_ADD_CIRCLE; }
  std::string GetEditorCategory() const override { return "Sprite"; }
  int GetPriority() const override { return 20; }
  float GetPreferredWidth() const override { return 1000.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_PANELS_SPRITE_EDITOR_PANELS_H_

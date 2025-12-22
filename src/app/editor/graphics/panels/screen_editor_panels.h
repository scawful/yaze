#ifndef YAZE_APP_EDITOR_GRAPHICS_PANELS_SCREEN_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_GRAPHICS_PANELS_SCREEN_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for ScreenEditor panels
// =============================================================================

/**
 * @brief EditorPanel for Dungeon Maps Editor
 */
class DungeonMapsPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit DungeonMapsPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "screen.dungeon_maps"; }
  std::string GetDisplayName() const override { return "Dungeon Maps"; }
  std::string GetIcon() const override { return ICON_MD_MAP; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 10; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Inventory Menu Editor
 */
class InventoryMenuPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit InventoryMenuPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "screen.inventory_menu"; }
  std::string GetDisplayName() const override { return "Inventory Menu"; }
  std::string GetIcon() const override { return ICON_MD_INVENTORY; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 20; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Overworld Map Screen Editor
 */
class OverworldMapScreenPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit OverworldMapScreenPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "screen.overworld_map"; }
  std::string GetDisplayName() const override { return "Overworld Map"; }
  std::string GetIcon() const override { return ICON_MD_PUBLIC; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 30; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Title Screen Editor
 */
class TitleScreenPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit TitleScreenPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "screen.title_screen"; }
  std::string GetDisplayName() const override { return "Title Screen"; }
  std::string GetIcon() const override { return ICON_MD_TITLE; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 40; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Naming Screen Editor
 */
class NamingScreenPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit NamingScreenPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "screen.naming_screen"; }
  std::string GetDisplayName() const override { return "Naming Screen"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 50; }

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

#endif  // YAZE_APP_EDITOR_GRAPHICS_PANELS_SCREEN_EDITOR_PANELS_H_

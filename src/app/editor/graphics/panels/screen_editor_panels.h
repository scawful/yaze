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
 * @brief EditorPanel for the Menu Tilemap (2bpp) Editor
 */
class MenuTilemapPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;
  using EnabledCallback = std::function<bool()>;

  MenuTilemapPanel(DrawCallback draw_callback, EnabledCallback enabled_callback)
      : draw_callback_(std::move(draw_callback)),
        enabled_callback_(std::move(enabled_callback)) {}

  std::string GetId() const override { return "screen.menu_tilemap"; }
  std::string GetDisplayName() const override { return "Menu Tilemap (2bpp)"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 60; }

  // Post-#262 admission policy: this is a Screen-editor-hosted embedded
  // tool, not a default global workspace window. It stays reachable via
  // its keyboard shortcut (kPanel-scope toggle, see GetShortcutHint()) and
  // the command palette/workflow surfaces (GetWorkflowGroup/Label/
  // Description below), without adding a row to the cross-editor window
  // browser.
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::EmbeddedTool();
  }

  // Named workflow + reason (the "why does this panel exist" the #262
  // admission rule requires): editing Oracle of Secrets' 2bpp BG3 menu
  // tilemap files (Menu/tilemaps/*.tilemap|*.bin, Menu/rings/*.tilemap),
  // first-class use case being the "Masks & Rings" page 3 prototype
  // (ring_box.tilemap, Menu/menu_page3.asm).
  std::string GetWorkflowGroup() const override { return "Editors"; }
  std::string GetWorkflowLabel() const override {
    return "Edit Menu Tilemap (2bpp)";
  }
  std::string GetWorkflowDescription() const override {
    return "Edit an Oracle of Secrets 2bpp BG3 menu tilemap file (e.g. "
           "ring_box.tilemap for the \"Masks & Rings\" page 3 prototype) "
           "with a live preview rendered from real CHR + palette sources.";
  }

  std::string GetShortcutHint() const override { return "Alt+6"; }

  // Responsive-size contract: a canvas + tool-rail layout, not a fixed
  // content grid, so this is a first-open hint (LayoutManager may resize
  // it), not an exact width like a tile picker's HasExactPreferredWidth().
  float GetPreferredWidth() const override { return 620.0f; }
  float GetPreferredHeight() const override { return 520.0f; }

  bool IsEnabled() const override {
    return enabled_callback_ ? enabled_callback_() : true;
  }
  std::string GetDisabledTooltip() const override { return "Load a ROM first"; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
  EnabledCallback enabled_callback_;
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

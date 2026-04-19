#ifndef YAZE_APP_EDITOR_GRAPHICS_SCREEN_SCREEN_EDITOR_VIEWS_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCREEN_SCREEN_EDITOR_VIEWS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// WindowContent views for the ScreenEditor workspace
// =============================================================================

class ScreenWorkspaceView : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;
  using EnabledCallback = std::function<bool()>;

  ScreenWorkspaceView(DrawCallback draw_callback,
                      EnabledCallback enabled_callback,
                      std::string shortcut_hint)
      : draw_callback_(std::move(draw_callback)),
        enabled_callback_(std::move(enabled_callback)),
        shortcut_hint_(std::move(shortcut_hint)) {}

  bool IsEnabled() const override {
    return enabled_callback_ ? enabled_callback_() : true;
  }

  std::string GetDisabledTooltip() const override { return "Load a ROM first"; }
  std::string GetShortcutHint() const override { return shortcut_hint_; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
  EnabledCallback enabled_callback_;
  std::string shortcut_hint_;
};

/**
 * @brief Dungeon map editing view.
 */
class DungeonMapsView : public ScreenWorkspaceView {
 public:
  DungeonMapsView(DrawCallback draw_callback, EnabledCallback enabled_callback,
                  std::string shortcut_hint)
      : ScreenWorkspaceView(std::move(draw_callback),
                            std::move(enabled_callback),
                            std::move(shortcut_hint)) {}

  std::string GetId() const override { return "screen.dungeon_maps"; }
  std::string GetDisplayName() const override { return "Dungeon Maps"; }
  std::string GetIcon() const override { return ICON_MD_MAP; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 10; }
};

/**
 * @brief Inventory menu editing view.
 */
class InventoryMenuView : public ScreenWorkspaceView {
 public:
  InventoryMenuView(DrawCallback draw_callback,
                    EnabledCallback enabled_callback, std::string shortcut_hint)
      : ScreenWorkspaceView(std::move(draw_callback),
                            std::move(enabled_callback),
                            std::move(shortcut_hint)) {}

  std::string GetId() const override { return "screen.inventory_menu"; }
  std::string GetDisplayName() const override { return "Inventory Menu"; }
  std::string GetIcon() const override { return ICON_MD_INVENTORY; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 20; }
};

/**
 * @brief Overworld map editing view.
 */
class OverworldMapScreenView : public ScreenWorkspaceView {
 public:
  OverworldMapScreenView(DrawCallback draw_callback,
                         EnabledCallback enabled_callback,
                         std::string shortcut_hint)
      : ScreenWorkspaceView(std::move(draw_callback),
                            std::move(enabled_callback),
                            std::move(shortcut_hint)) {}

  std::string GetId() const override { return "screen.overworld_map"; }
  std::string GetDisplayName() const override { return "Overworld Map"; }
  std::string GetIcon() const override { return ICON_MD_PUBLIC; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 30; }
};

/**
 * @brief Title screen editing view.
 */
class TitleScreenView : public ScreenWorkspaceView {
 public:
  TitleScreenView(DrawCallback draw_callback, EnabledCallback enabled_callback,
                  std::string shortcut_hint)
      : ScreenWorkspaceView(std::move(draw_callback),
                            std::move(enabled_callback),
                            std::move(shortcut_hint)) {}

  std::string GetId() const override { return "screen.title_screen"; }
  std::string GetDisplayName() const override { return "Title Screen"; }
  std::string GetIcon() const override { return ICON_MD_TITLE; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 40; }
};

/**
 * @brief Naming screen editing view.
 */
class NamingScreenView : public ScreenWorkspaceView {
 public:
  NamingScreenView(DrawCallback draw_callback, EnabledCallback enabled_callback,
                   std::string shortcut_hint)
      : ScreenWorkspaceView(std::move(draw_callback),
                            std::move(enabled_callback),
                            std::move(shortcut_hint)) {}

  std::string GetId() const override { return "screen.naming_screen"; }
  std::string GetDisplayName() const override { return "Naming Screen"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Screen"; }
  int GetPriority() const override { return 50; }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_SCREEN_EDITOR_VIEWS_H_

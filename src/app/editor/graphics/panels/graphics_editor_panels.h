#ifndef YAZE_APP_EDITOR_GRAPHICS_PANELS_GRAPHICS_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_GRAPHICS_PANELS_GRAPHICS_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for GraphicsEditor panels
// =============================================================================

/**
 * @brief Sheet browser panel for navigating graphics sheets
 */
class GraphicsSheetBrowserPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsSheetBrowserPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.sheet_browser_v2"; }
  std::string GetDisplayName() const override { return "Sheet Browser"; }
  std::string GetIcon() const override { return ICON_MD_VIEW_LIST; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 10; }
  bool IsVisibleByDefault() const override { return true; }
  float GetPreferredWidth() const override { return 350.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Main pixel editing panel for graphics sheets
 */
class GraphicsPixelEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsPixelEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.pixel_editor"; }
  std::string GetDisplayName() const override { return "Pixel Editor"; }
  std::string GetIcon() const override { return ICON_MD_DRAW; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 20; }
  bool IsVisibleByDefault() const override { return true; }
  float GetPreferredWidth() const override { return 800.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Palette controls panel for managing graphics palettes
 */
class GraphicsPaletteControlsPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsPaletteControlsPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.palette_controls"; }
  std::string GetDisplayName() const override { return "Palette Controls"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 30; }
  float GetPreferredWidth() const override { return 300.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Link sprite editor panel for editing Link's graphics
 */
class GraphicsLinkSpritePanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsLinkSpritePanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.link_sprite_editor"; }
  std::string GetDisplayName() const override { return "Link Sprite Editor"; }
  std::string GetIcon() const override { return ICON_MD_PERSON; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 35; }
  float GetPreferredWidth() const override { return 600.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief 3D polyhedral object editor panel
 */
class GraphicsPolyhedralPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsPolyhedralPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.polyhedral_editor"; }
  std::string GetDisplayName() const override { return "3D Objects"; }
  std::string GetIcon() const override { return ICON_MD_VIEW_IN_AR; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 38; }
  float GetPreferredWidth() const override { return 600.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Graphics group editor panel for managing GFX groups
 */
class GraphicsGfxGroupPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsGfxGroupPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.gfx_group_editor"; }
  std::string GetDisplayName() const override { return "Graphics Groups"; }
  std::string GetIcon() const override { return ICON_MD_VIEW_MODULE; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 39; }
  float GetPreferredWidth() const override { return 500.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Prototype graphics viewer for Super Donkey and dev format imports
 */
class GraphicsPrototypeViewerPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsPrototypeViewerPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.prototype_viewer"; }
  std::string GetDisplayName() const override { return "Prototype Viewer"; }
  std::string GetIcon() const override { return ICON_MD_CONSTRUCTION; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 50; }
  float GetPreferredWidth() const override { return 800.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Paletteset editor panel for managing dungeon palette associations
 */
class GraphicsPalettesetPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit GraphicsPalettesetPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "graphics.paletteset_editor"; }
  std::string GetDisplayName() const override { return "Palettesets"; }
  std::string GetIcon() const override { return ICON_MD_COLOR_LENS; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 45; }
  float GetPreferredWidth() const override { return 500.0f; }

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

#endif  // YAZE_APP_EDITOR_GRAPHICS_PANELS_GRAPHICS_EDITOR_PANELS_H_


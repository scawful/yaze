#ifndef YAZE_APP_EDITOR_CODE_PANELS_ASSEMBLY_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_CODE_PANELS_ASSEMBLY_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for AssemblyEditor panels
// =============================================================================

/**
 * @brief Main code editor panel with text editing
 */
class AssemblyCodeEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AssemblyCodeEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "assembly.code_editor"; }
  std::string GetDisplayName() const override { return "Code Editor"; }
  std::string GetIcon() const override { return ICON_MD_CODE; }
  std::string GetEditorCategory() const override { return "Assembly"; }
  int GetPriority() const override { return 10; }
  bool IsVisibleByDefault() const override { return true; }
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
 * @brief File browser panel for navigating project files
 */
class AssemblyFileBrowserPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AssemblyFileBrowserPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "assembly.file_browser"; }
  std::string GetDisplayName() const override { return "File Browser"; }
  std::string GetIcon() const override { return ICON_MD_FOLDER_OPEN; }
  std::string GetEditorCategory() const override { return "Assembly"; }
  int GetPriority() const override { return 20; }
  bool IsVisibleByDefault() const override { return true; }
  float GetPreferredWidth() const override { return 280.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Symbol table viewer panel
 */
class AssemblySymbolsPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AssemblySymbolsPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "assembly.symbols"; }
  std::string GetDisplayName() const override { return "Symbols"; }
  std::string GetIcon() const override { return ICON_MD_LIST_ALT; }
  std::string GetEditorCategory() const override { return "Assembly"; }
  int GetPriority() const override { return 30; }
  float GetPreferredWidth() const override { return 320.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Build output / errors panel
 */
class AssemblyBuildOutputPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AssemblyBuildOutputPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "assembly.build_output"; }
  std::string GetDisplayName() const override { return "Build Output"; }
  std::string GetIcon() const override { return ICON_MD_TERMINAL; }
  std::string GetEditorCategory() const override { return "Assembly"; }
  int GetPriority() const override { return 40; }
  float GetPreferredWidth() const override { return 400.0f; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief Toolbar panel with quick actions
 */
class AssemblyToolbarPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AssemblyToolbarPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "assembly.toolbar"; }
  std::string GetDisplayName() const override { return "Toolbar"; }
  std::string GetIcon() const override { return ICON_MD_CONSTRUCTION; }
  std::string GetEditorCategory() const override { return "Assembly"; }
  int GetPriority() const override { return 5; }
  bool IsVisibleByDefault() const override { return true; }

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

#endif  // YAZE_APP_EDITOR_CODE_PANELS_ASSEMBLY_EDITOR_PANELS_H_


#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

#include <string>

#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Phase 3 scaffolding for the WYSIWYG layout designer. Registers a
// cross-editor System panel with a three-column body (palette | canvas |
// properties) and a disabled File action row. No tree state, selection,
// persistence, or drag-drop yet — those arrive in Phases 4–8.
class LayoutDesignerPanel : public WindowContent {
 public:
  LayoutDesignerPanel() = default;

  std::string GetId() const override { return "layout.designer"; }
  std::string GetDisplayName() const override { return "Layout Designer"; }
  std::string GetIcon() const override { return ICON_MD_DASHBOARD_CUSTOMIZE; }
  std::string GetEditorCategory() const override { return "System"; }

  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::CrossEditor;
  }

  void Draw(bool* p_open) override;

 private:
  std::string active_name_ = "(unnamed)";
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

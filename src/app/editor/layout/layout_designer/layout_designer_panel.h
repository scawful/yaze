#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

#include <string>

#include "app/editor/layout/layout_designer/dock_tree.h"
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
  LayoutDesignerPanel();

  std::string GetId() const override { return "layout.designer"; }
  std::string GetDisplayName() const override { return "Layout Designer"; }
  std::string GetIcon() const override { return ICON_MD_DASHBOARD_CUSTOMIZE; }
  // Matches the other cross-editor shell panels (About, Settings). The
  // workspace manager treats category strings literally (exact match vs
  // active_category or else draws only when pinned) — see the rev-17
  // migration in user_settings.cc which force-pins this panel so "Show"
  // is sufficient from any editor.
  std::string GetEditorCategory() const override { return "Settings"; }

  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::CrossEditor;
  }

  void Draw(bool* p_open) override;

 private:
  DockTree tree_;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

#include <string>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/tree_undo_stack.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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
  // Active drag state for split-boundary resize. `split_node` is nullptr
  // between drags; while set, the panel is mid-drag and all other input
  // is ignored until the mouse button releases.
  struct ActiveDrag {
    DockNode* split_node = nullptr;
    float start_ratio = 0.0f;
    ImVec2 start_mouse{0.0f, 0.0f};
    ImRect start_rect{};
  };

  // Undo/redo helpers. Callers push BEFORE mutating `tree_`; the
  // stack clears on any new push so the redo branch follows the mutation
  // that replaced it.
  void PushUndoSnapshot();

  // Properties column body — split into its own method to keep Draw()
  // focused on layout/input routing.
  void DrawPropertiesColumn();

  DockTree tree_;
  const DockNode* selected_ = nullptr;
  ActiveDrag drag_;
  std::string palette_query_;
  TreeUndoStack undo_;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

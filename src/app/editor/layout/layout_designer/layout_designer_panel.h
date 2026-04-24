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

  // Replace the currently-edited tree wholesale. Any op that swaps `tree_`
  // (New, Open, apply-from-elsewhere) MUST go through this helper — the
  // raw `selected_` pointer references DockNodes inside the old tree and
  // would dangle across a move. Same for `drag_.split_node`. Undo history
  // is cleared because its snapshots reference the replaced tree's
  // identity (and mid-session undo across Load boundaries is confusing
  // UX — users can undo *within* a loaded layout, not across loads).
  void ReplaceTree(DockTree new_tree);

  // Test-only accessors. The live Draw path is the only production
  // mutator of `selected_`/`drag_`; tests need to observe both to verify
  // the tree-swap invariant holds.
  const DockTree& tree_for_test() const { return tree_; }
  const DockNode* selected_for_test() const { return selected_; }
  bool has_active_drag_for_test() const { return drag_.split_node != nullptr; }
  const TreeUndoStack& undo_for_test() const { return undo_; }
  // Test-only setters so ReplaceTree's clear behavior can be exercised
  // without standing up an ImGui context. Production callers should rely
  // on Draw() to set selection/drag via mouse input.
  void set_selected_for_test(const DockNode* n) { selected_ = n; }
  void set_drag_node_for_test(DockNode* n) { drag_.split_node = n; }

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

  // File row / popup bodies.
  void DrawFileRow();
  void DrawOpenPopup();
  void DrawSaveAsPopup();

  // Named-layout persistence (all operate on Context::user_settings()).
  // No-ops with logged warnings when user_settings is unavailable.
  void SaveCurrentTreeToNamedLayouts();
  void LoadNamedLayoutIntoTree(const std::string& name);
  void ApplyCurrentTreeToLiveDockspace();

  DockTree tree_;
  const DockNode* selected_ = nullptr;
  ActiveDrag drag_;
  std::string palette_query_;
  TreeUndoStack undo_;

  // Popup state. `*_popup_open_` flags drive one-shot OpenPopup calls
  // next frame; the text buffers back the inline editors.
  bool open_popup_requested_ = false;
  bool save_as_popup_requested_ = false;
  std::string save_as_buffer_;
  std::string
      open_selection_;  // Which named layout the Open popup has highlighted.
  // Transient status line below the File row. Cleared each frame.
  std::string status_message_;
  bool status_is_error_ = false;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_LAYOUT_DESIGNER_PANEL_H_

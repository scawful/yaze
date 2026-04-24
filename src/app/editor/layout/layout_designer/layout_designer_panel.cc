#include "app/editor/layout/layout_designer/layout_designer_panel.h"

#include <algorithm>
#include <string>
#include <utility>

#include "app/editor/layout/layout_designer/dock_tree_hit_test.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "app/editor/layout/layout_designer/drop_zone_suggester.h"
#include "app/editor/layout/layout_designer/panel_palette.h"
#include "app/editor/layout/layout_designer/split_boundary_drag.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/registry/panel_registration.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/drag_drop.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

constexpr float kPaletteInitialWidth = 240.0f;
constexpr float kPropertiesInitialWidth = 280.0f;

void DrawDisabledAction(const char* label) {
  ImGui::BeginDisabled();
  ImGui::SmallButton(label);
  ImGui::EndDisabled();
}

bool IsSplitHorizontal(SplitDirection d) {
  return d == SplitDirection::kLeft || d == SplitDirection::kRight;
}

const char* SplitDirectionLabel(SplitDirection d) {
  switch (d) {
    case SplitDirection::kLeft:
      return "Left";
    case SplitDirection::kRight:
      return "Right";
    case SplitDirection::kUp:
      return "Up";
    case SplitDirection::kDown:
      return "Down";
  }
  return "Left";
}

}  // namespace

LayoutDesignerPanel::LayoutDesignerPanel() : tree_(MakeEmptyTree("Untitled")) {}

void LayoutDesignerPanel::PushUndoSnapshot() {
  undo_.Push(tree_);
}

void LayoutDesignerPanel::Draw(bool* p_open) {
  (void)p_open;  // Closing the window is handled by the workspace shell.

  // Keyboard shortcuts: Ctrl/Cmd+Z (undo) and Ctrl/Cmd+Shift+Z (redo).
  // Gated on focus so typing in palette/property text fields doesn't
  // trip them.
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool chord = io.KeyCtrl || io.KeySuper;
    if (chord && ImGui::IsKeyPressed(ImGuiKey_Z, /*repeat=*/false)) {
      if (io.KeyShift) {
        if (undo_.Redo(&tree_))
          selected_ = nullptr;
      } else {
        if (undo_.Undo(&tree_))
          selected_ = nullptr;
      }
    }
  }

  // WindowContent::Draw contract: no ImGui::Begin/End and no BeginMenuBar
  // (the outer PanelWindow does not opt into ImGuiWindowFlags_MenuBar). A
  // disabled-button row stands in for the File menu until a menubar-capable
  // host arrives in Phase 8.
  ImGui::TextUnformatted("File:");
  ImGui::SameLine();
  DrawDisabledAction("New");
  ImGui::SameLine();
  DrawDisabledAction("Open...");
  ImGui::SameLine();
  DrawDisabledAction("Save");
  ImGui::SameLine();
  DrawDisabledAction("Save As...");
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::BeginDisabled(!undo_.CanUndo());
  if (ImGui::SmallButton("Undo")) {
    if (undo_.Undo(&tree_))
      selected_ = nullptr;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!undo_.CanRedo());
  if (ImGui::SmallButton("Redo")) {
    if (undo_.Redo(&tree_))
      selected_ = nullptr;
  }
  ImGui::EndDisabled();
  ImGui::Separator();

  constexpr ImGuiTableFlags kBodyFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;

  if (ImGui::BeginTable("layout_designer_body", 3, kBodyFlags)) {
    ImGui::TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthFixed,
                            kPaletteInitialWidth);
    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed,
                            kPropertiesInitialWidth);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (ImGui::BeginChild("layout_designer_palette", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      // Exclude self so the designer can't be dropped inside its own
      // canvas.
      const auto entries = CollectPaletteEntries(GetId());
      DrawPanelPalette(entries, &palette_query_);
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    if (ImGui::BeginChild("layout_designer_canvas", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
      const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
      if (canvas_size.x >= kMinCellSize && canvas_size.y >= kMinCellSize) {
        const ImRect viewport(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x,
                                                 canvas_pos.y + canvas_size.y));
        const DockTreeLayout layout = ComputeLayout(tree_, viewport);
        RenderDockTree(tree_, layout, selected_, ImGui::GetWindowDrawList());
        ImGui::Dummy(canvas_size);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool hovered = ImGui::IsItemHovered();

        if (ImGui::BeginDragDropTarget()) {
          const DockNode* leaf_const = HitTestNode(layout, mouse);
          if (leaf_const != nullptr &&
              leaf_const->type == DockNode::Type::kLeaf) {
            const ImRect& leaf_rect = layout.node_rects.at(leaf_const);
            const DropSuggestion suggestion = SuggestDrop(leaf_rect, mouse);
            const ImRect preview =
                ComputeDropPreviewRect(leaf_rect, suggestion);
            if (preview.GetWidth() > 0.0f && preview.GetHeight() > 0.0f) {
              ImGui::GetWindowDrawList()->AddRectFilled(
                  preview.Min, preview.Max,
                  ImGui::ColorConvertFloat4ToU32(
                      ImVec4(0.25f, 0.55f, 0.95f, 0.25f)));
            }
            gui::PanelDragPayload payload;
            if (gui::AcceptPanelDropWithinTarget(&payload)) {
              PanelEntry new_panel;
              new_panel.panel_id = payload.panel_id;
              if (const WindowContent* registered =
                      ContentRegistry::Panels::Get(new_panel.panel_id)) {
                new_panel.display_name = registered->GetDisplayName();
                new_panel.icon = registered->GetIcon();
              }
              DockNode* leaf_mut = const_cast<DockNode*>(leaf_const);
              // Snapshot before mutation so this drop is reversible.
              PushUndoSnapshot();
              if (DockNode* selected_leaf = ApplyDropSuggestion(
                      &tree_, leaf_mut, suggestion, std::move(new_panel))) {
                selected_ = selected_leaf;
              } else {
                // Applier refused (e.g. duplicate id); drop the speculative
                // undo snapshot so the stack stays clean.
                (void)undo_.Undo(&tree_);
              }
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (drag_.split_node != nullptr) {
          // Mid-drag: keep consuming mouse motion until release. Do not
          // gate on hovered — the mouse can leave the canvas bounds mid-
          // drag and we still want to track it until mouse-up.
          if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const bool horizontal =
                IsSplitHorizontal(drag_.split_node->split_direction);
            const float axis_delta = horizontal
                                         ? (mouse.x - drag_.start_mouse.x)
                                         : (mouse.y - drag_.start_mouse.y);
            const float axis_size = horizontal ? drag_.start_rect.GetWidth()
                                               : drag_.start_rect.GetHeight();
            drag_.split_node->split_ratio = ComputeDraggedSplitRatio(
                drag_.start_ratio, axis_delta, axis_size);
            ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW
                                             : ImGuiMouseCursor_ResizeNS);
          } else {
            drag_ = ActiveDrag{};
          }
        } else if (hovered) {
          const SplitBoundaryHit boundary =
              HitTestSplitBoundary(tree_, layout, mouse);
          if (boundary.split_node != nullptr) {
            ImGui::SetMouseCursor(boundary.horizontal
                                      ? ImGuiMouseCursor_ResizeEW
                                      : ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
              // Snapshot before the first ratio edit so the whole drag
              // collapses into a single undo step.
              PushUndoSnapshot();
              drag_.split_node = const_cast<DockNode*>(boundary.split_node);
              drag_.start_ratio = drag_.split_node->split_ratio;
              drag_.start_mouse = mouse;
              drag_.start_rect = layout.node_rects.at(boundary.split_node);
              // Also surface the split as the current selection so the
              // accent outline tracks the resize target.
              selected_ = drag_.split_node;
            }
          } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selected_ = HitTestNode(layout, mouse);
          }
        }
      }
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(2);
    if (ImGui::BeginChild("layout_designer_properties", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      DrawPropertiesColumn();
    }
    ImGui::EndChild();

    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Editing: %s | Undo: %zu / Redo: %zu",
              tree_.name.empty() ? "(unnamed)" : tree_.name.c_str(),
              undo_.UndoDepth(), undo_.RedoDepth());
}

void LayoutDesignerPanel::DrawPropertiesColumn() {
  // Helper: wraps ImGui::SliderFloat with PushUndo-on-activate so
  // continuous drags produce exactly one undo snapshot.
  auto slider_with_undo = [this](const char* label, float* value, float mn,
                                 float mx) {
    const float before = *value;
    ImGui::SliderFloat(label, value, mn, mx, "%.2f");
    if (ImGui::IsItemActivated()) {
      PushUndoSnapshot();
      property_edit_in_progress_ = true;
    }
    if (!ImGui::IsItemActive() && property_edit_in_progress_) {
      property_edit_in_progress_ = false;
    }
    return *value != before;
  };

  if (selected_ == nullptr) {
    ImGui::SeparatorText("Layout");
    std::string name_buf = tree_.name;
    if (ImGui::InputText("Name", &name_buf,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      if (name_buf != tree_.name) {
        PushUndoSnapshot();
        tree_.name = std::move(name_buf);
      }
    }
    std::string desc_buf = tree_.description;
    if (ImGui::InputTextMultiline("Description", &desc_buf,
                                  ImVec2(0.0f, 80.0f))) {
      if (desc_buf != tree_.description) {
        PushUndoSnapshot();
        tree_.description = std::move(desc_buf);
      }
    }
    ImGui::TextDisabled("Select a cell in the canvas to edit it.");
    return;
  }

  DockNode* node = const_cast<DockNode*>(selected_);
  if (node->type == DockNode::Type::kSplit) {
    ImGui::SeparatorText("Split");

    const SplitDirection current_dir = node->split_direction;
    if (ImGui::BeginCombo("Direction", SplitDirectionLabel(current_dir))) {
      for (int i = 0; i < 4; ++i) {
        const auto candidate = static_cast<SplitDirection>(i);
        const bool is_selected = candidate == current_dir;
        if (ImGui::Selectable(SplitDirectionLabel(candidate), is_selected)) {
          if (candidate != current_dir) {
            PushUndoSnapshot();
            node->split_direction = candidate;
          }
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    (void)slider_with_undo("Ratio", &node->split_ratio, kMinSplitRatio,
                           kMaxSplitRatio);
    return;
  }

  // Leaf properties.
  ImGui::SeparatorText("Leaf");
  if (node->panels.empty()) {
    ImGui::TextDisabled("(no panels — drop from the palette)");
    return;
  }

  // Active-tab selector.
  const int panel_count = static_cast<int>(node->panels.size());
  int active = std::clamp(node->active_tab_index, 0, panel_count - 1);
  if (ImGui::SliderInt("Active tab", &active, 0, panel_count - 1)) {
    if (active != node->active_tab_index) {
      PushUndoSnapshot();
      node->active_tab_index = active;
    }
  }

  ImGui::Separator();
  // Panel list with reorder + remove actions.
  for (int i = 0; i < panel_count; ++i) {
    ImGui::PushID(i);
    const auto& entry = node->panels[i];
    const std::string label = entry.icon.empty()
                                  ? entry.display_name
                                  : entry.icon + "  " + entry.display_name;
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine();

    ImGui::BeginDisabled(i == 0);
    if (ImGui::SmallButton("Up")) {
      PushUndoSnapshot();
      std::swap(node->panels[i], node->panels[i - 1]);
      if (node->active_tab_index == i)
        --node->active_tab_index;
      else if (node->active_tab_index == i - 1)
        ++node->active_tab_index;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(i + 1 >= panel_count);
    if (ImGui::SmallButton("Down")) {
      PushUndoSnapshot();
      std::swap(node->panels[i], node->panels[i + 1]);
      if (node->active_tab_index == i)
        ++node->active_tab_index;
      else if (node->active_tab_index == i + 1)
        --node->active_tab_index;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
      PushUndoSnapshot();
      node->panels.erase(node->panels.begin() + i);
      if (!node->panels.empty()) {
        node->active_tab_index =
            std::clamp(node->active_tab_index, 0,
                       static_cast<int>(node->panels.size()) - 1);
      } else {
        node->active_tab_index = 0;
      }
      ImGui::PopID();
      break;
    }

    ImGui::PopID();
  }
}

REGISTER_PANEL(LayoutDesignerPanel);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

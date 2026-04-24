#include "app/editor/layout/layout_designer/layout_designer_panel.h"

#include "app/editor/layout/layout_designer/dock_tree_hit_test.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "app/editor/layout/layout_designer/panel_palette.h"
#include "app/editor/layout/layout_designer/split_boundary_drag.h"
#include "app/editor/registry/panel_registration.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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

}  // namespace

LayoutDesignerPanel::LayoutDesignerPanel() : tree_(MakeEmptyTree("Untitled")) {}

void LayoutDesignerPanel::Draw(bool* p_open) {
  (void)p_open;  // Closing the window is handled by the workspace shell.

  // WindowContent::Draw contract: no ImGui::Begin/End and no BeginMenuBar
  // (the outer PanelWindow does not opt into ImGuiWindowFlags_MenuBar). A
  // disabled-button row stands in for the File menu until a menubar-capable
  // host arrives in a later phase.
  ImGui::TextUnformatted("File:");
  ImGui::SameLine();
  DrawDisabledAction("New");
  ImGui::SameLine();
  DrawDisabledAction("Open...");
  ImGui::SameLine();
  DrawDisabledAction("Save");
  ImGui::SameLine();
  DrawDisabledAction("Save As...");
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
      // canvas. Phase 6.2 will wire BeginPanelDragSource around each row;
      // for now a click just selects (no-op from the panel's perspective).
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
              drag_.split_node = const_cast<DockNode*>(boundary.split_node);
              drag_.start_ratio = drag_.split_node->split_ratio;
              drag_.start_mouse = mouse;
              drag_.start_rect = layout.node_rects.at(boundary.split_node);
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
      ImGui::TextUnformatted("Properties (Phase 7)");
    }
    ImGui::EndChild();

    ImGui::EndTable();
  }

  ImGui::Separator();
  ImGui::Text("Editing: %s",
              tree_.name.empty() ? "(unnamed)" : tree_.name.c_str());
}

REGISTER_PANEL(LayoutDesignerPanel);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

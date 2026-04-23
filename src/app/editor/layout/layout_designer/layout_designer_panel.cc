#include "app/editor/layout/layout_designer/layout_designer_panel.h"

#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
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
      ImGui::TextUnformatted("Palette (Phase 6)");
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
        RenderDockTree(tree_, layout, /*selected=*/nullptr,
                       ImGui::GetWindowDrawList());
        // Reserve space so the child's content-size tracks the rendered
        // tree; selection input (Phase 5) will consume this dummy.
        ImGui::Dummy(canvas_size);
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

#include "app/editor/layout/layout_designer/layout_designer_panel.h"

#include "app/editor/registry/panel_registration.h"
#include "imgui/imgui.h"

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
      ImGui::TextUnformatted("Canvas (Phase 4)");
      // TODO(phase-4): render dock tree here.
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
  ImGui::Text("Editing: %s", active_name_.c_str());
}

REGISTER_PANEL(LayoutDesignerPanel);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

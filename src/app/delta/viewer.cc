#include "viewer.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"

namespace yaze {
namespace app {
namespace delta {
namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

void NewMasterFrame() {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, kMainEditorFlags)) {
    ImGui::End();
    return;
  }
}

}  // namespace

void Viewer::Update() {
  NewMasterFrame();
  DrawYazeMenu();
  DrawFileDialog();

  ImGui::Text(ICON_MD_CHANGE_HISTORY);
  ImGui::SameLine();
  ImGui::Text("%s", rom_.GetTitle());

  ImGui::Separator();

  ImGui::Button(ICON_MD_SYNC);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_ARROW_UPWARD);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_ARROW_DOWNWARD);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_MERGE);
  ImGui::SameLine();

  ImGui::Button(ICON_MD_MANAGE_HISTORY);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_LAN);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_COMMIT);
  ImGui::SameLine();
  ImGui::Button(ICON_MD_DIFFERENCE);

  ImGui::Separator();

  ImGui::SetNextItemWidth(75.f);
  ImGui::Button(ICON_MD_SEND);
  ImGui::SameLine();
  ImGui::InputText("Server Address", &client_address_);

  ImGui::SetNextItemWidth(75.f);
  ImGui::Button(ICON_MD_DOWNLOAD);
  ImGui::SameLine();
  ImGui::InputText("Repository Source", &client_address_);

  ImGui::Separator();
  DrawBranchTree();
  ImGui::End();
}

void Viewer::DrawFileDialog() {
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      rom_.LoadFromFile(filePathName);
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void Viewer::DrawYazeMenu() {
  MENU_BAR()
  DrawFileMenu();
  DrawViewMenu();
  END_MENU_BAR()
}

void Viewer::DrawFileMenu() const {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
    }

    MENU_ITEM2("Save", "Ctrl+S") {}

    ImGui::EndMenu();
  }
}

void Viewer::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_imgui_style_editor = false;
  static bool show_memory_editor = false;
  static bool show_imgui_demo = false;

  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_memory_editor) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow("Memory Editor", (void*)&rom_, rom_.size());
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }

  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("HEX Editor", nullptr, &show_memory_editor);
    ImGui::MenuItem("ImGui Demo", nullptr, &show_imgui_demo);
    ImGui::Separator();
    if (ImGui::BeginMenu("GUI Tools")) {
      ImGui::MenuItem("Metrics (ImGui)", nullptr, &show_imgui_metrics);
      ImGui::MenuItem("Style Editor (ImGui)", nullptr,
                      &show_imgui_style_editor);
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void Viewer::DrawBranchTree() {
  static ImGuiTableFlags flags =
      ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_NoBordersInBody;

  if (ImGui::BeginTable("3ways", 3, flags)) {
    // The first column will use the default _WidthStretch when ScrollX is Off
    // and _WidthFixed when ScrollX is On
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed,
                            10 * 12.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                            10 * 18.0f);
    ImGui::TableHeadersRow();

    // Simple storage to output a dummy file-system.
    struct MyTreeNode {
      const char* Name;
      const char* Type;
      int Size;
      int ChildIdx;
      int ChildCount;
      static void DisplayNode(const MyTreeNode* node,
                              const MyTreeNode* all_nodes) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const bool is_folder = (node->ChildCount > 0);
        if (is_folder) {
          bool open =
              ImGui::TreeNodeEx(node->Name, ImGuiTreeNodeFlags_SpanFullWidth);
          ImGui::TableNextColumn();
          ImGui::TextDisabled("--");
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(node->Type);
          if (open) {
            for (int child_n = 0; child_n < node->ChildCount; child_n++)
              DisplayNode(&all_nodes[node->ChildIdx + child_n], all_nodes);
            ImGui::TreePop();
          }
        } else {
          ImGui::TreeNodeEx(
              node->Name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                              ImGuiTreeNodeFlags_NoTreePushOnOpen |
                              ImGuiTreeNodeFlags_SpanFullWidth);
          ImGui::TableNextColumn();
          ImGui::Text("%d", node->Size);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(node->Type);
        }
      }
    };
    static const MyTreeNode nodes[] = {
        {"lttp-redux", "Repository", -1, 1, 3},
        {"main", "Branch", -1, 4, 2},
        {"hyrule-castle", "Branch", -1, 4, 2},
        {"lost-woods", "Branch", -1, 6, 3},
        {"Added some bushes", "Commit", 1024, -1, -1},
        {"Constructed a new house", "Commit", 123000, -1, -1},
        {"File1_b.wav", "Commit", 456000, -1, -1},
        {"Image001.png", "Commit", 203128, -1, -1},
        {"Copy of Image001.png", "Commit", 203256, -1, -1},
        {"Copy of Image001 (Final2).png", "Commit", 203512, -1, -1},
    };

    MyTreeNode::DisplayNode(&nodes[0], nodes);

    ImGui::EndTable();
  }
}
}  // namespace delta
}  // namespace app
}  // namespace yaze
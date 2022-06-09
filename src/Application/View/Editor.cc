#include "Editor.h"

namespace yaze {
namespace Application {
namespace View {

void Editor::UpdateScreen() const {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_MenuBar;

  if (!ImGui::Begin("Main", nullptr, flags)) {
    ImGui::End();
    return;
  }
  DrawYazeMenu();
  ImGui::End();
}

void Editor::DrawYazeMenu() const {
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      DrawFileMenu();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      DrawEditMenu();
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
}

void Editor::DrawFileMenu() const {
  if (ImGui::MenuItem("Open", "Ctrl+O")) {
    // TODO: Add the ability to open ALTTP ROM
  }
  if (ImGui::BeginMenu("Open Recent")) {
    ImGui::MenuItem("alttp.sfc");
    // TODO: Display recently accessed files here 
    ImGui::EndMenu();
  }
  if (ImGui::MenuItem("Save", "Ctrl+S")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Save As..")) {
    // TODO: Implement this
  }

  ImGui::Separator();

  // TODO: Make these options matter
  if (ImGui::BeginMenu("Options")) {
      static bool enabled = true;
      ImGui::MenuItem("Enabled", "", &enabled);
      ImGui::BeginChild("child", ImVec2(0, 60), true);
      for (int i = 0; i < 10; i++)
          ImGui::Text("Scrolling Text %d", i);
      ImGui::EndChild();
      static float f = 0.5f;
      static int n = 0;
      ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
      ImGui::InputFloat("Input", &f, 0.1f);
      ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
      ImGui::EndMenu();
  }
}


void Editor::DrawEditMenu() const {
  if (ImGui::MenuItem("Undo", "Ctrl+O")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Undo", "Ctrl+O")) {
    // TODO: Implement this
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Cut", "Ctrl+X")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Copy", "Ctrl+C")) {
    // TODO: Implement this
  }
  if (ImGui::MenuItem("Paste", "Ctrl+V")) {
    // TODO: Implement this
  }
  ImGui::Separator();
  if (ImGui::MenuItem("Find Tiles", "Ctrl+F")) {
    // TODO: Implement this
  }
}


}  // namespace View
}  // namespace Application
}  // namespace yaze
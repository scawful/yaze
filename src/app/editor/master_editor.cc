#include "master_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/imgui_memory_editor.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/editor/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "gui/canvas.h"
#include "gui/icons.h"
#include "gui/input.h"
#include "gui/widgets.h"

namespace yaze {
namespace app {
namespace editor {

MasterEditor::~MasterEditor() { rom_.Close(); }

void MasterEditor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
  rom_.SetupRenderer(renderer);
}

void MasterEditor::UpdateScreen() {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, main_editor_flags_)) {
    ImGui::End();
    return;
  }

  DrawYazeMenu();

  TAB_BAR("##TabBar")
  DrawOverworldEditor();
  DrawDungeonEditor();
  DrawSpriteEditor();
  DrawScreenEditor();
  END_TAB_BAR()

  if (!status_.ok()) {
    gui::widgets::DisplayStatus(status_);
  }

  ImGui::End();
}

void MasterEditor::DrawYazeMenu() {
  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  DrawViewMenu();
  DrawHelpMenu();
  END_MENU_BAR()

  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      rom_.LoadFromFile(filePathName);
      status_ = rom_.OpenFromFile(filePathName);
      overworld_editor_.SetupROM(rom_);
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void MasterEditor::DrawFileMenu() const {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
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
      for (int i = 0; i < 10; i++) ImGui::Text("Scrolling Text %d", i);
      ImGui::EndChild();
      static float f = 0.5f;
      static int n = 0;
      ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
      ImGui::InputFloat("Input", &f, 0.1f);
      ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawEditMenu() const {
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      // TODO: Implement this
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
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
    if (ImGui::MenuItem("Find", "Ctrl+F")) {
      // TODO: Implement this
    }
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_imgui_style_editor = false;
  static bool show_memory_editor = false;
  static bool show_asm_editor = false;
  static bool show_imgui_demo = false;

  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_memory_editor) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow("Memory Editor", (void *)rom_.data(), rom_.getSize());
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_asm_editor) {
    assembly_editor_.Update();
  }

  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }

  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("HEX Editor", nullptr, &show_memory_editor);
    ImGui::MenuItem("ASM Editor", nullptr, &show_asm_editor);
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

void MasterEditor::DrawHelpMenu() const {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) {
      // insert the about window here
    }
    ImGui::Text("Title: %s", rom_.getTitle());
    ImGui::Text("ROM Size: %ld", rom_.getSize());
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawOverworldEditor() {
  TAB_ITEM("Overworld")
  overworld_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawDungeonEditor() {
  TAB_ITEM("Dungeon")
  dungeon_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawScreenEditor() {
  TAB_ITEM("Screens")
  screen_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawSpriteEditor() {
  TAB_ITEM("Sprites")
  END_TAB_ITEM()
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
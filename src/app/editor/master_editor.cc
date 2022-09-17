#include "master_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

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

namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

void NewMasterFrame() {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, kMainEditorFlags)) {
    ImGui::End();
    return;
  }
}

bool BeginCentered(const char *name) {
  ImGuiIO const &io = ImGui::GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return ImGui::Begin(name, nullptr, flags);
}

void DisplayStatus(absl::Status &status) {
  if (BeginCentered("StatusWindow")) {
    ImGui::Text("%s", status.ToString().c_str());
    ImGui::Spacing();
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::NewLine();
    ImGui::SameLine(270);
    if (ImGui::Button("OK", ImVec2(200, 0))) {
      status = absl::OkStatus();
    }
    ImGui::End();
  }
}

}  // namespace

void MasterEditor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
  rom_.SetupRenderer(renderer);
}

void MasterEditor::UpdateScreen() {
  NewMasterFrame();

  DrawYazeMenu();
  DrawFileDialog();
  DrawStatusPopup();
  DrawAboutPopup();
  DrawInfoPopup();

  TAB_BAR("##TabBar")
  DrawOverworldEditor();
  DrawDungeonEditor();
  DrawPaletteEditor();
  DrawSpriteEditor();
  DrawScreenEditor();
  END_TAB_BAR()

  ImGui::End();
}

void MasterEditor::DrawFileDialog() {
  if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      status_ = rom_.LoadFromFile(filePathName);
      overworld_editor_.SetupROM(rom_);
      screen_editor_.SetupROM(rom_);
      palette_editor_.SetupROM(rom_);
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void MasterEditor::DrawStatusPopup() {
  if (!status_.ok()) {
    DisplayStatus(status_);
  }
}

void MasterEditor::DrawAboutPopup() {
  if (about_) ImGui::OpenPopup("About");
  if (ImGui::BeginPopupModal("About", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Yet Another Zelda3 Editor - v0.02");
    ImGui::Text("Written by: scawful");
    ImGui::Spacing();
    ImGui::Text("Special Thanks: Zarby89, JaredBrian");
    ImGui::Separator();

    if (ImGui::Button("Close", ImVec2(200, 0))) {
      about_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::DrawInfoPopup() {
  if (rom_info_) ImGui::OpenPopup("ROM Information");
  if (ImGui::BeginPopupModal("ROM Information", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Title: %s", rom_.GetTitle());
    ImGui::Text("ROM Size: %ld", rom_.size());

    if (ImGui::Button("Close", ImVec2(200, 0))) {
      rom_info_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::DrawYazeMenu() {
  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  DrawViewMenu();
  DrawHelpMenu();
  END_MENU_BAR()
}

void MasterEditor::DrawFileMenu() const {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
    }

    MENU_ITEM2("Save", "Ctrl+S") {}
    MENU_ITEM("Save As..") {}

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

void MasterEditor::DrawEditMenu() {
  if (ImGui::BeginMenu("Edit")) {
    MENU_ITEM2("Undo", "Ctrl+Z") { status_ = overworld_editor_.Undo(); }
    MENU_ITEM2("Redo", "Ctrl+Y") { status_ = overworld_editor_.Redo(); }
    ImGui::Separator();
    MENU_ITEM2("Cut", "Ctrl+X") { status_ = overworld_editor_.Cut(); }
    MENU_ITEM2("Copy", "Ctrl+C") { status_ = overworld_editor_.Copy(); }
    MENU_ITEM2("Paste", "Ctrl+V") { status_ = overworld_editor_.Paste(); }
    ImGui::Separator();
    MENU_ITEM2("Find", "Ctrl+F") {}
    ImGui::Separator();
    MENU_ITEM("ROM Information") rom_info_ = true;
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
    mem_edit.DrawWindow("Memory Editor", (void *)&rom_, rom_.size());
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

void MasterEditor::DrawHelpMenu() {
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("About")) about_ = true;
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawOverworldEditor() {
  TAB_ITEM("Overworld")
  status_ = overworld_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawDungeonEditor() {
  TAB_ITEM("Dungeon")
  dungeon_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawPaletteEditor() {
  TAB_ITEM("Palettes")
  status_ = palette_editor_.Update();
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
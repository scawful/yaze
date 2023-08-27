#include "master_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/core/pipeline.h"
#include "app/editor/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/music_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/widgets.h"
#include "app/rom.h"

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
  ImGui::SetNextWindowPos(gui::kZeroPos);
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

}  // namespace

void MasterEditor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer) {
  sdl_renderer_ = renderer;
  rom()->SetupRenderer(renderer);
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
  DrawGraphicsEditor();
  DrawMusicEditor();
  DrawSpriteEditor();
  DrawScreenEditor();
  DrawPaletteEditor();
  END_TAB_BAR()

  ImGui::End();
}

void MasterEditor::DrawFileDialog() {
  core::FileDialogPipeline("ChooseFileDlgKey", ".sfc,.smc", std::nullopt,
                           [&]() {
                             std::string filePathName =
                                 ImGuiFileDialog::Instance()->GetFilePathName();
                             status_ = rom()->LoadFromFile(filePathName);
                           });
}

void MasterEditor::DrawStatusPopup() {
  if (!status_.ok()) {
    show_status_ = true;
    prev_status_ = status_;
  }

  if (show_status_ && (BeginCentered("StatusWindow"))) {
    ImGui::Text("%s", prev_status_.ToString().c_str());
    ImGui::Spacing();
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::NewLine();
    ImGui::SameLine(270);
    if (ImGui::Button("OK", gui::kDefaultModalSize)) {
      show_status_ = false;
    }
    ImGui::End();
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

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
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
    ImGui::Text("Title: %s", rom()->title());
    ImGui::Text("ROM Size: %ld", rom()->size());

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
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

void MasterEditor::DrawFileMenu() {
  static bool save_as_menu = false;

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                              ".sfc,.smc", ".");
    }

    MENU_ITEM2("Save", "Ctrl+S") { status_ = rom()->SaveToFile(backup_rom_); }
    MENU_ITEM("Save As..") { save_as_menu = true; }

    ImGui::Separator();

    if (ImGui::BeginMenu("Options")) {
      ImGui::MenuItem("Backup ROM", "", &backup_rom_);
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    ImGui::Begin("Save As..", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText("Filename", &save_as_filename);
    if (ImGui::Button("Save", gui::kDefaultModalSize)) {
      status_ = rom()->SaveToFile(backup_rom_, save_as_filename);
    }
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    ImGui::End();
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
  static bool show_memory_viewer = false;
  static bool show_palette_editor = false;
  static bool show_emulator = false;

  if (show_emulator) {
    ImGui::Begin("Emulator", &show_emulator, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    ImGui::End();
  }

  if (show_imgui_metrics) {
    ImGui::ShowMetricsWindow(&show_imgui_metrics);
  }

  if (show_memory_editor) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow("Memory Editor", (void *)&(*rom()), rom()->size());
  }

  if (show_imgui_demo) {
    ImGui::ShowDemoWindow();
  }

  if (show_asm_editor) {
    assembly_editor_.Update(show_asm_editor);
  }

  if (show_palette_editor) {
    ImGui::Begin("Palette Editor", &show_palette_editor);
    status_ = palette_editor_.Update();
    ImGui::End();
  }

  if (show_imgui_style_editor) {
    ImGui::Begin("Style Editor (ImGui)", &show_imgui_style_editor);
    ImGui::ShowStyleEditor();
    ImGui::End();
  }

  if (show_memory_viewer) {
    ImGui::Begin("Memory Viewer (ImGui)", &show_memory_viewer);

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    static ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    if (auto outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 5.5f);
        ImGui::BeginTable("table1", 3, flags, outer_size)) {
      for (int row = 0; row < 10; row++) {
        ImGui::TableNextRow();
        for (int column = 0; column < 3; column++) {
          ImGui::TableNextColumn();
          ImGui::Text("Cell %d,%d", column, row);
        }
      }
      ImGui::EndTable();
    }

    ImGui::End();
  }

  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("Emulator", nullptr, &show_emulator);
    ImGui::MenuItem("HEX Editor", nullptr, &show_memory_editor);
    ImGui::MenuItem("ASM Editor", nullptr, &show_asm_editor);
    ImGui::MenuItem("Palette Editor", nullptr, &show_palette_editor);
    ImGui::MenuItem("Memory Viewer", nullptr, &show_memory_viewer);
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
  static bool open_rom_help = false;
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("How to open a ROM")) open_rom_help = true;
    if (ImGui::MenuItem("About")) about_ = true;
    ImGui::EndMenu();
  }

  if (open_rom_help) ImGui::OpenPopup("Open a ROM");
  if (ImGui::BeginPopupModal("Open a ROM", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("File -> Open");
    ImGui::Text("Select a ROM file to open");
    ImGui::Text("Supported ROMs (headered or unheadered):");
    ImGui::Text("The Legend of Zelda: A Link to the Past");
    ImGui::Text("US Version 1.0");
    ImGui::Text("JP Version 1.0");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_rom_help = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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

void MasterEditor::DrawGraphicsEditor() {
  TAB_ITEM("Graphics")
  status_ = graphics_editor_.Update();
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

void MasterEditor::DrawMusicEditor() {
  TAB_ITEM("Music")
  music_editor_.Update();
  END_TAB_ITEM()
}

void MasterEditor::DrawSpriteEditor() {
  TAB_ITEM("Sprites")
  status_ = sprite_editor_.Update();
  END_TAB_ITEM()
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
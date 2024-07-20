#include "master_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <abseil-cpp/absl/strings/match.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/core/platform/file_dialog.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon_editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/utils/recent_files.h"
#include "app/emu/emulator.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/gui/style.h"
#include "app/gui/widgets.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::BeginMenu;
using ImGui::BulletText;
using ImGui::Checkbox;
using ImGui::MenuItem;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::Spacing;
using ImGui::Text;

namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

void NewMasterFrame() {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(gui::kZeroPos);
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  ImGui::SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!ImGui::Begin("##YazeMain", nullptr, kMainEditorFlags)) {
    ImGui::End();
    return;
  }
}

bool BeginCentered(const char* name) {
  ImGuiIO const& io = ImGui::GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return ImGui::Begin(name, nullptr, flags);
}

// Function to switch the active tab in a tab bar
void SetTabBarTab(ImGuiTabBar* tab_bar, ImGuiID tab_id) {
  if (tab_bar == NULL) return;

  // Find the tab item with the specified tab_id
  ImGuiTabItem* tab_item = &tab_bar->Tabs[tab_id];
  tab_item->LastFrameVisible = -1;
  tab_item->LastFrameSelected = -1;
  tab_bar->VisibleTabId = tab_id;
  tab_bar->VisibleTabWasSubmitted = true;
  tab_bar->SelectedTabId = tab_id;
  tab_bar->NextSelectedTabId = tab_id;
  tab_bar->ReorderRequestTabId = tab_id;
  tab_bar->CurrFrameVisible = -1;
}

bool IsEditorActive(Editor* editor, std::vector<Editor*>& active_editors) {
  return std::find(active_editors.begin(), active_editors.end(), editor) !=
         active_editors.end();
}

}  // namespace

void MasterEditor::SetupScreen(std::shared_ptr<SDL_Renderer> renderer,
                               std::string filename) {
  sdl_renderer_ = renderer;
  rom()->SetupRenderer(renderer);
  if (!filename.empty()) {
    PRINT_IF_ERROR(rom()->LoadFromFile(filename));
  }
  overworld_editor_.InitializeZeml();
}

absl::Status MasterEditor::Update() {
  NewMasterFrame();

  DrawYazeMenu();
  DrawFileDialog();
  DrawStatusPopup();
  DrawAboutPopup();
  DrawInfoPopup();

  if (rom()->is_loaded() && !rom_assets_loaded_) {
    // Load all of the graphics data from the game.
    RETURN_IF_ERROR(rom()->LoadAllGraphicsData())
    // Initialize overworld graphics, maps, and palettes
    RETURN_IF_ERROR(overworld_editor_.LoadGraphics());
    rom_assets_loaded_ = true;
  }

  ManageActiveEditors();

  ImGui::End();

  return absl::OkStatus();
}

void MasterEditor::ManageActiveEditors() {
  // Show popup pane to select an editor to add
  static bool show_add_editor = false;
  if (show_add_editor) ImGui::OpenPopup("AddEditor");

  if (ImGui::BeginPopup("AddEditor", ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::MenuItem("Overworld", nullptr, false,
                        !IsEditorActive(&overworld_editor_, active_editors_))) {
      active_editors_.push_back(&overworld_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Dungeon", nullptr, false,
                        !IsEditorActive(&dungeon_editor_, active_editors_))) {
      active_editors_.push_back(&dungeon_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Graphics", nullptr, false,
                        !IsEditorActive(&graphics_editor_, active_editors_))) {
      active_editors_.push_back(&graphics_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Music", nullptr, false,
                        !IsEditorActive(&music_editor_, active_editors_))) {
      active_editors_.push_back(&music_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Palette", nullptr, false,
                        !IsEditorActive(&palette_editor_, active_editors_))) {
      active_editors_.push_back(&palette_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Screen", nullptr, false,
                        !IsEditorActive(&screen_editor_, active_editors_))) {
      active_editors_.push_back(&screen_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Sprite", nullptr, false,
                        !IsEditorActive(&sprite_editor_, active_editors_))) {
      active_editors_.push_back(&sprite_editor_);
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Code", nullptr, false,
                        !IsEditorActive(&assembly_editor_, active_editors_))) {
      active_editors_.push_back(&assembly_editor_);
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (!ImGui::IsPopupOpen("AddEditor")) {
    show_add_editor = false;
  }

  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_Reorderable |
                                         ImGuiTabBarFlags_AutoSelectNewTabs)) {
    for (auto editor : active_editors_) {
      bool open = true;
      switch (editor->type()) {
        case EditorType::kOverworld:
          if (overworld_editor_.jump_to_tab() == -1) {
            if (ImGui::BeginTabItem("Overworld", &open)) {
              current_editor_ = &overworld_editor_;
              status_ = overworld_editor_.Update();
              ImGui::EndTabItem();
            }
          }
          break;
        case EditorType::kDungeon:
          if (ImGui::BeginTabItem("Dungeon", &open)) {
            current_editor_ = &dungeon_editor_;
            status_ = dungeon_editor_.Update();
            if (overworld_editor_.jump_to_tab() != -1) {
              dungeon_editor_.add_room(overworld_editor_.jump_to_tab());
              overworld_editor_.jump_to_tab_ = -1;
            }
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kGraphics:
          if (ImGui::BeginTabItem("Graphics", &open)) {
            current_editor_ = &graphics_editor_;
            status_ = graphics_editor_.Update();
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kMusic:
          if (ImGui::BeginTabItem("Music", &open)) {
            current_editor_ = &music_editor_;

            status_ = music_editor_.Update();
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kPalette:
          if (ImGui::BeginTabItem("Palette", &open)) {
            current_editor_ = &palette_editor_;
            status_ = palette_editor_.Update();
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kScreen:
          if (ImGui::BeginTabItem("Screen", &open)) {
            current_editor_ = &screen_editor_;
            status_ = screen_editor_.Update();
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kSprite:
          if (ImGui::BeginTabItem("Sprite", &open)) {
            current_editor_ = &sprite_editor_;
            status_ = sprite_editor_.Update();
            ImGui::EndTabItem();
          }
          break;
        case EditorType::kAssembly:
          if (ImGui::BeginTabItem("Code", &open)) {
            current_editor_ = &assembly_editor_;
            assembly_editor_.UpdateCodeView();
            ImGui::EndTabItem();
          }
          break;
        default:
          break;
      }
      if (!open) {
        active_editors_.erase(
            std::remove(active_editors_.begin(), active_editors_.end(), editor),
            active_editors_.end());
      }
    }

    if (ImGui::TabItemButton(ICON_MD_ADD, ImGuiTabItemFlags_Trailing)) {
      show_add_editor = true;
    }

    ImGui::EndTabBar();
  }
}

void MasterEditor::DrawFileDialog() {
  gui::FileDialogPipeline("ChooseFileDlgKey", ".sfc,.smc", std::nullopt, [&]() {
    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
    status_ = rom()->LoadFromFile(filePathName);
    static RecentFilesManager manager("recent_files.txt");

    // Load existing recent files
    manager.Load();

    // Add a new file
    manager.AddFile(filePathName);

    // Save the updated list
    manager.Save();
  });
}

void MasterEditor::DrawStatusPopup() {
  if (!status_.ok()) {
    show_status_ = true;
    prev_status_ = status_;
  }

  if (show_status_ && (BeginCentered("StatusWindow"))) {
    Text("%s", ICON_MD_ERROR);
    Text("%s", prev_status_.ToString().c_str());
    Spacing();
    ImGui::NextColumn();
    ImGui::Columns(1);
    Separator();
    ImGui::NewLine();
    SameLine(128);
    if (ImGui::Button("OK", gui::kDefaultModalSize)) {
      show_status_ = false;
      status_ = absl::OkStatus();
    }
    SameLine();
    if (ImGui::Button(ICON_MD_CONTENT_COPY, ImVec2(50, 0))) {
      ImGui::SetClipboardText(prev_status_.ToString().c_str());
    }
    ImGui::End();
  }
}

void MasterEditor::DrawAboutPopup() {
  if (about_) ImGui::OpenPopup("About");
  if (ImGui::BeginPopupModal("About", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Yet Another Zelda3 Editor - v%.2f", core::kYazeVersion);
    Text("Written by: scawful");
    Spacing();
    Text("Special Thanks: Zarby89, JaredBrian");
    Separator();

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
    Text("Title: %s", rom()->title());
    Text("ROM Size: %ld", rom()->size());

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      rom_info_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::DrawYazeMenu() {
  static bool show_display_settings = false;
  static bool show_command_line_interface = false;

  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawEditMenu();
    DrawViewMenu();
    DrawProjectMenu();
    DrawHelpMenu();

    SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x -
             ImGui::CalcTextSize(ICON_MD_DISPLAY_SETTINGS).x - 150);
    // Modify the style of the button to have no background color
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (ImGui::Button(ICON_MD_DISPLAY_SETTINGS)) {
      show_display_settings = !show_display_settings;
    }

    if (ImGui::Button(ICON_MD_TERMINAL)) {
      show_command_line_interface = !show_command_line_interface;
    }
    ImGui::PopStyleColor();

    Text("%s", absl::StrCat("yaze v", core::kYazeVersion).c_str());

    ImGui::EndMenuBar();
  }

  if (show_display_settings) {
    ImGui::Begin("Display Settings", &show_display_settings,
                 ImGuiWindowFlags_None);
    gui::DrawDisplaySettings();
    ImGui::End();
  }

  if (show_command_line_interface) {
    ImGui::Begin("Command Line Interface", &show_command_line_interface,
                 ImGuiWindowFlags_None);
    Text("Enter a command:");
    ImGui::End();
  }
}

void MasterEditor::DrawFileMenu() {
  static bool save_as_menu = false;
  static bool new_project_menu = false;

  if (BeginMenu("File")) {
    if (MenuItem("Open", "Ctrl+O")) {
      LoadRom();
    }

    if (BeginMenu("Open Recent")) {
      static RecentFilesManager manager("recent_files.txt");
      manager.Load();
      if (manager.GetRecentFiles().empty()) {
        MenuItem("No Recent Files", nullptr, false, false);
      } else {
        for (const auto& filePath : manager.GetRecentFiles()) {
          if (MenuItem(filePath.c_str())) {
            if (absl::StrContains(filePath, ".yaze")) {
              status_ = current_project_.Open(filePath);
              if (status_.ok()) {
                status_ = OpenProject();
              }
            } else {
              status_ = rom()->LoadFromFile(filePath);
            }
          }
        }
      }
      ImGui::EndMenu();
    }

    MENU_ITEM2("Save", "Ctrl+S") {
      if (rom()->is_loaded()) {
        SaveRom();
      }
    }
    MENU_ITEM("Save As..") { save_as_menu = true; }

    if (rom()->is_loaded()) {
      MENU_ITEM("Reload") { status_ = rom()->Reload(); }
      MENU_ITEM("Close") {
        status_ = rom()->Close();
        rom_assets_loaded_ = false;
      }
    }

    Separator();

    if (BeginMenu("Project")) {
      if (MenuItem("Create New Project")) {
        // Create a new project
        new_project_menu = true;
      }
      if (MenuItem("Open Project")) {
        // Open an existing project
        status_ =
            current_project_.Open(FileDialogWrapper::ShowOpenFileDialog());
        if (status_.ok()) {
          status_ = OpenProject();
        }
      }
      if (MenuItem("Save Project")) {
        // Save the current project
        status_ = current_project_.Save();
      }

      ImGui::EndMenu();
    }

    if (BeginMenu("Options")) {
      MenuItem("Backup ROM", "", &backup_rom_);
      MenuItem("Save New Auto", "", &save_new_auto_);
      Separator();
      if (BeginMenu("Experiment Flags")) {
        if (BeginMenu("Overworld Flags")) {
          Checkbox("Enable Overworld Sprites",
                   &mutable_flags()->overworld.kDrawOverworldSprites);
          Separator();
          Checkbox("Save Overworld Maps",
                   &mutable_flags()->overworld.kSaveOverworldMaps);
          Checkbox("Save Overworld Entrances",
                   &mutable_flags()->overworld.kSaveOverworldEntrances);
          Checkbox("Save Overworld Exits",
                   &mutable_flags()->overworld.kSaveOverworldExits);
          Checkbox("Save Overworld Items",
                   &mutable_flags()->overworld.kSaveOverworldItems);
          Checkbox("Save Overworld Properties",
                   &mutable_flags()->overworld.kSaveOverworldProperties);
          ImGui::EndMenu();
        }

        if (BeginMenu("Dungeon Flags")) {
          Checkbox("Draw Dungeon Room Graphics",
                   &mutable_flags()->kDrawDungeonRoomGraphics);
          Separator();
          Checkbox("Save Dungeon Maps", &mutable_flags()->kSaveDungeonMaps);
          ImGui::EndMenu();
        }

        if (BeginMenu("Emulator Flags")) {
          Checkbox("Load Audio Device", &mutable_flags()->kLoadAudioDevice);
          ImGui::EndMenu();
        }

        Checkbox("Use built-in file dialog",
                 &mutable_flags()->kNewFileDialogWrapper);
        Checkbox("Enable Console Logging", &mutable_flags()->kLogToConsole);
        Checkbox("Enable Texture Streaming",
                 &mutable_flags()->kLoadTexturesAsStreaming);
        Checkbox("Use Bitmap Manager", &mutable_flags()->kUseBitmapManager);
        Checkbox("Log Instructions to Debugger",
                 &mutable_flags()->kLogInstructions);
        Checkbox("Save All Palettes", &mutable_flags()->kSaveAllPalettes);
        Checkbox("Save Gfx Groups", &mutable_flags()->kSaveGfxGroups);
        Checkbox("Use New ImGui Input", &mutable_flags()->kUseNewImGuiInput);
        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }

    Separator();

    if (MenuItem("Quit", "Ctrl+Q")) {
      quit_ = true;
    }

    ImGui::EndMenu();
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    ImGui::Begin("Save As..", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText("Filename", &save_as_filename);
    if (ImGui::Button("Save", gui::kDefaultModalSize)) {
      SaveRom();
      save_as_menu = false;
    }
    SameLine();
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    ImGui::End();
  }

  if (new_project_menu) {
    ImGui::Begin("New Project", &new_project_menu,
                 ImGuiWindowFlags_AlwaysAutoResize);
    static std::string save_as_filename = "";
    ImGui::InputText("Project Name", &save_as_filename);
    if (ImGui::Button("Destination Filepath", gui::kDefaultModalSize)) {
      current_project_.filepath = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.filepath.c_str());
    if (ImGui::Button("ROM File", gui::kDefaultModalSize)) {
      current_project_.rom_filename_ = FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.rom_filename_.c_str());
    if (ImGui::Button("Labels File", gui::kDefaultModalSize)) {
      current_project_.labels_filename_ =
          FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.labels_filename_.c_str());
    if (ImGui::Button("Code Folder", gui::kDefaultModalSize)) {
      current_project_.code_folder_ = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.code_folder_.c_str());

    Separator();
    if (ImGui::Button("Create", gui::kDefaultModalSize)) {
      new_project_menu = false;
      status_ = current_project_.Create(save_as_filename);
      if (status_.ok()) {
        status_ = current_project_.Save();
      }
    }
    SameLine();
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      new_project_menu = false;
    }
    ImGui::End();
  }
}

void MasterEditor::DrawEditMenu() {
  if (BeginMenu("Edit")) {
    MENU_ITEM2("Undo", "Ctrl+Z") { status_ = current_editor_->Undo(); }
    MENU_ITEM2("Redo", "Ctrl+Y") { status_ = current_editor_->Redo(); }
    Separator();
    MENU_ITEM2("Cut", "Ctrl+X") { status_ = current_editor_->Cut(); }
    MENU_ITEM2("Copy", "Ctrl+C") { status_ = current_editor_->Copy(); }
    MENU_ITEM2("Paste", "Ctrl+V") { status_ = current_editor_->Paste(); }
    Separator();
    MENU_ITEM2("Find", "Ctrl+F") {}
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawViewMenu() {
  static bool show_imgui_metrics = false;
  static bool show_memory_editor = false;
  static bool show_asm_editor = false;
  static bool show_imgui_demo = false;
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
    memory_editor_.Update(show_memory_editor);
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

  if (BeginMenu("View")) {
    MenuItem("Emulator", nullptr, &show_emulator);
    Separator();
    MenuItem("Memory Editor", nullptr, &show_memory_editor);
    MenuItem("Assembly Editor", nullptr, &show_asm_editor);
    MenuItem("Palette Editor", nullptr, &show_palette_editor);
    Separator();
    MENU_ITEM("ROM Information") rom_info_ = true;
    Separator();
    MenuItem("ImGui Demo", nullptr, &show_imgui_demo);
    MenuItem("ImGui Metrics", nullptr, &show_imgui_metrics);
    ImGui::EndMenu();
  }
}

void MasterEditor::DrawProjectMenu() {
  static bool show_resource_label_manager = false;

  if (current_project_.project_opened_) {
    // Project Menu
    if (BeginMenu("Project")) {
      Text("Name: %s", current_project_.name.c_str());
      Text("ROM: %s", current_project_.rom_filename_.c_str());
      Text("Labels: %s", current_project_.labels_filename_.c_str());
      Text("Code: %s", current_project_.code_folder_.c_str());
      Separator();
      MenuItem("Resource Labels", nullptr, &show_resource_label_manager);
      ImGui::EndMenu();
    }
  }
  if (show_resource_label_manager) {
    rom()->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened_ &&
        !current_project_.labels_filename_.empty()) {
      current_project_.labels_filename_ = rom()->resource_label()->filename_;
    }
  }
}

void MasterEditor::DrawHelpMenu() {
  static bool open_rom_help = false;
  static bool open_supported_features = false;
  static bool open_manage_project = false;
  if (BeginMenu("Help")) {
    if (MenuItem("How to open a ROM")) open_rom_help = true;
    if (MenuItem("Supported Features")) open_supported_features = true;
    if (MenuItem("How to manage a project")) open_manage_project = true;

    if (MenuItem("About")) about_ = true;
    ImGui::EndMenu();
  }

  if (open_supported_features) ImGui::OpenPopup("Supported Features");
  if (ImGui::BeginPopupModal("Supported Features", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Overworld");
    BulletText("LW/DW/SW Tilemap Editing");
    BulletText("LW/DW/SW Map Properties");
    BulletText("Create/Delete/Update Entrances");
    BulletText("Create/Delete/Update Exits");
    BulletText("Create/Delete/Update Sprites");
    BulletText("Create/Delete/Update Items");

    Text("Dungeon");
    BulletText("View Room Header Properties");
    BulletText("View Entrance Properties");

    Text("Graphics");
    BulletText("View Decompressed Graphics Sheets");
    BulletText("View/Update Graphics Groups");

    Text("Palettes");
    BulletText("View Palette Groups");

    Text("Saveable");
    BulletText("All Listed Overworld Features");
    BulletText("Hex Editor Changes");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_supported_features = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (open_rom_help) ImGui::OpenPopup("Open a ROM");
  if (ImGui::BeginPopupModal("Open a ROM", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("File -> Open");
    Text("Select a ROM file to open");
    Text("Supported ROMs (headered or unheadered):");
    Text("The Legend of Zelda: A Link to the Past");
    Text("US Version 1.0");
    Text("JP Version 1.0");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_rom_help = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (open_manage_project) ImGui::OpenPopup("Manage Project");
  if (ImGui::BeginPopupModal("Manage Project", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Project Menu");
    Text("Create a new project or open an existing one.");
    Text("Save the project to save the current state of the project.");
    Text(
        "To save a project, you need to first open a ROM and initialize your "
        "code path and labels file. Label resource manager can be found in "
        "the "
        "View menu. Code path is set in the Code editor after opening a "
        "folder.");

    if (ImGui::Button("Close", gui::kDefaultModalSize)) {
      open_manage_project = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void MasterEditor::LoadRom() {
  if (flags()->kNewFileDialogWrapper) {
    auto file_name = FileDialogWrapper::ShowOpenFileDialog();
    PRINT_IF_ERROR(rom()->LoadFromFile(file_name));
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    manager.AddFile(file_name);
    manager.Save();
  } else {
    ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Open ROM",
                                            ".sfc,.smc", ".");
  }
}

void MasterEditor::SaveRom() {
  if (flags()->kSaveDungeonMaps) {
    status_ = screen_editor_.SaveDungeonMaps();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldMaps) {
    RETURN_VOID_IF_ERROR(
        status_ = overworld_editor_.overworld()->CreateTile32Tilemap());
    status_ = overworld_editor_.overworld()->SaveMap32Tiles();
    RETURN_VOID_IF_ERROR(status_);
    status_ = overworld_editor_.overworld()->SaveMap16Tiles();
    RETURN_VOID_IF_ERROR(status_);
    status_ = overworld_editor_.overworld()->SaveOverworldMaps();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldEntrances) {
    status_ = overworld_editor_.overworld()->SaveEntrances();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldExits) {
    status_ = overworld_editor_.overworld()->SaveExits();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldItems) {
    status_ = overworld_editor_.overworld()->SaveItems();
    RETURN_VOID_IF_ERROR(status_);
  }
  if (flags()->overworld.kSaveOverworldProperties) {
    status_ = overworld_editor_.overworld()->SaveMapProperties();
    RETURN_VOID_IF_ERROR(status_);
  }

  status_ = rom()->SaveToFile(backup_rom_, save_new_auto_);
}

absl::Status MasterEditor::OpenProject() {
  RETURN_IF_ERROR(rom()->LoadFromFile(current_project_.rom_filename_));

  if (!rom()->resource_label()->LoadLabels(current_project_.labels_filename_)) {
    return absl::InternalError(
        "Could not load labels file, update your project file.");
  }

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(current_project_.filepath + "/" + current_project_.name +
                  ".yaze");
  manager.Save();

  assembly_editor_.OpenFolder(current_project_.code_folder_);

  current_project_.project_opened_ = true;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze

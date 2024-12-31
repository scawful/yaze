#include "editor_manager.h"

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "app/core/constants.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/flags.h"
#include "app/emu/emulator.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "editor/editor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {

using namespace ImGui;
using core::FileDialogWrapper;

namespace {

bool BeginCentered(const char *name) {
  ImGuiIO const &io = GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return Begin(name, nullptr, flags);
}

bool IsEditorActive(Editor *editor, std::vector<Editor *> &active_editors) {
  return std::find(active_editors.begin(), active_editors.end(), editor) !=
         active_editors.end();
}

} // namespace

void EditorManager::Initialize(std::string filename) {
  if (!filename.empty()) {
    PRINT_IF_ERROR(rom()->LoadFromFile(filename));
  }
  overworld_editor_.Initialize();
}

absl::Status EditorManager::Update() {
  ManageKeyboardShortcuts();

  DrawYazeMenu();
  DrawStatusPopup();
  DrawAboutPopup();
  DrawInfoPopup();

  if (rom()->is_loaded() && !rom_assets_loaded_) {
    RETURN_IF_ERROR(rom()->LoadAllGraphicsData())
    RETURN_IF_ERROR(overworld_editor_.LoadGraphics());
    rom_assets_loaded_ = true;
  }

  ManageActiveEditors();

  return absl::OkStatus();
}

void EditorManager::ManageActiveEditors() {
  // Show popup pane to select an editor to add
  static bool show_add_editor = false;
  if (show_add_editor)
    OpenPopup("AddEditor");

  if (BeginPopup("AddEditor", ImGuiWindowFlags_AlwaysAutoResize)) {
    if (MenuItem("Overworld", nullptr, false,
                 !IsEditorActive(&overworld_editor_, active_editors_))) {
      active_editors_.push_back(&overworld_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Dungeon", nullptr, false,
                 !IsEditorActive(&dungeon_editor_, active_editors_))) {
      active_editors_.push_back(&dungeon_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Graphics", nullptr, false,
                 !IsEditorActive(&graphics_editor_, active_editors_))) {
      active_editors_.push_back(&graphics_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Music", nullptr, false,
                 !IsEditorActive(&music_editor_, active_editors_))) {
      active_editors_.push_back(&music_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Palette", nullptr, false,
                 !IsEditorActive(&palette_editor_, active_editors_))) {
      active_editors_.push_back(&palette_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Screen", nullptr, false,
                 !IsEditorActive(&screen_editor_, active_editors_))) {
      active_editors_.push_back(&screen_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Sprite", nullptr, false,
                 !IsEditorActive(&sprite_editor_, active_editors_))) {
      active_editors_.push_back(&sprite_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Code", nullptr, false,
                 !IsEditorActive(&assembly_editor_, active_editors_))) {
      active_editors_.push_back(&assembly_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Message", nullptr, false,
                 !IsEditorActive(&message_editor_, active_editors_))) {
      active_editors_.push_back(&message_editor_);
      CloseCurrentPopup();
    }
    if (MenuItem("Settings", nullptr, false,
                 !IsEditorActive(&settings_editor_, active_editors_))) {
      active_editors_.push_back(&settings_editor_);
      CloseCurrentPopup();
    }
    EndPopup();
  }

  if (!IsPopupOpen("AddEditor")) {
    show_add_editor = false;
  }

  if (BeginTabBar("##TabBar", ImGuiTabBarFlags_Reorderable |
                                  ImGuiTabBarFlags_AutoSelectNewTabs)) {
    for (auto editor : active_editors_) {
      bool open = true;
      switch (editor->type()) {
      case EditorType::kOverworld:
        if (overworld_editor_.jump_to_tab() == -1) {
          if (BeginTabItem("Overworld", &open)) {
            current_editor_ = &overworld_editor_;
            status_ = overworld_editor_.Update();
            EndTabItem();
          }
        }
        break;
      case EditorType::kDungeon:
        if (BeginTabItem("Dungeon", &open)) {
          current_editor_ = &dungeon_editor_;
          status_ = dungeon_editor_.Update();
          if (overworld_editor_.jump_to_tab() != -1) {
            dungeon_editor_.add_room(overworld_editor_.jump_to_tab());
            overworld_editor_.jump_to_tab_ = -1;
          }
          EndTabItem();
        }
        break;
      case EditorType::kGraphics:
        if (BeginTabItem("Graphics", &open)) {
          current_editor_ = &graphics_editor_;
          status_ = graphics_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kMusic:
        if (BeginTabItem("Music", &open)) {
          current_editor_ = &music_editor_;

          status_ = music_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kPalette:
        if (BeginTabItem("Palette", &open)) {
          current_editor_ = &palette_editor_;
          status_ = palette_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kScreen:
        if (BeginTabItem("Screen", &open)) {
          current_editor_ = &screen_editor_;
          status_ = screen_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kSprite:
        if (BeginTabItem("Sprite", &open)) {
          current_editor_ = &sprite_editor_;
          status_ = sprite_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kAssembly:
        if (BeginTabItem("Code", &open)) {
          current_editor_ = &assembly_editor_;
          assembly_editor_.UpdateCodeView();
          EndTabItem();
        }
        break;
      case EditorType::kSettings:
        if (BeginTabItem("Settings", &open)) {
          current_editor_ = &settings_editor_;
          status_ = settings_editor_.Update();
          EndTabItem();
        }
        break;
      case EditorType::kMessage:
        if (BeginTabItem("Message", &open)) {
          current_editor_ = &message_editor_;
          status_ = message_editor_.Update();
          EndTabItem();
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

    if (TabItemButton(ICON_MD_ADD, ImGuiTabItemFlags_Trailing)) {
      show_add_editor = true;
    }

    EndTabBar();
  }
}

void EditorManager::ManageKeyboardShortcuts() {
  bool ctrl_or_super = (GetIO().KeyCtrl || GetIO().KeySuper);

  editor_context_.command_manager.ShowWhichKey();

  // If CMD + R is pressed, reload the top result of recent files
  if (IsKeyDown(ImGuiKey_R) && ctrl_or_super) {
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    if (!manager.GetRecentFiles().empty()) {
      auto front = manager.GetRecentFiles().front();
      OpenRomOrProject(front);
    }
  }

  if (IsKeyDown(ImGuiKey_F1)) {
    about_ = true;
  }

  // If CMD + Q is pressed, quit the application
  if (IsKeyDown(ImGuiKey_Q) && ctrl_or_super) {
    quit_ = true;
  }

  // If CMD + O is pressed, open a file dialog
  if (IsKeyDown(ImGuiKey_O) && ctrl_or_super) {
    LoadRom();
  }

  // If CMD + S is pressed, save the current ROM
  if (IsKeyDown(ImGuiKey_S) && ctrl_or_super) {
    SaveRom();
  }

  if (IsKeyDown(ImGuiKey_X) && ctrl_or_super) {
    status_ = current_editor_->Cut();
  }

  if (IsKeyDown(ImGuiKey_C) && ctrl_or_super) {
    status_ = current_editor_->Copy();
  }

  if (IsKeyDown(ImGuiKey_V) && ctrl_or_super) {
    status_ = current_editor_->Paste();
  }

  if (IsKeyDown(ImGuiKey_Z) && ctrl_or_super) {
    status_ = current_editor_->Undo();
  }

  if (IsKeyDown(ImGuiKey_Y) && ctrl_or_super) {
    status_ = current_editor_->Redo();
  }

  if (IsKeyDown(ImGuiKey_F) && ctrl_or_super) {
    status_ = current_editor_->Find();
  }
}

void EditorManager::DrawStatusPopup() {
  static absl::Status prev_status;
  if (!status_.ok()) {
    show_status_ = true;
    prev_status = status_;
  }

  if (show_status_ && (BeginCentered("StatusWindow"))) {
    Text("%s", ICON_MD_ERROR);
    Text("%s", prev_status.ToString().c_str());
    Spacing();
    NextColumn();
    Columns(1);
    Separator();
    NewLine();
    SameLine(128);
    if (Button("OK", gui::kDefaultModalSize) || IsKeyPressed(ImGuiKey_Space)) {
      show_status_ = false;
      status_ = absl::OkStatus();
    }
    SameLine();
    if (Button(ICON_MD_CONTENT_COPY, ImVec2(50, 0))) {
      SetClipboardText(prev_status.ToString().c_str());
    }
    End();
  }
}

void EditorManager::DrawAboutPopup() {
  if (about_)
    OpenPopup("About");
  if (BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Yet Another Zelda3 Editor - v%s", core::kYazeVersion.data());
    Text("Written by: scawful");
    Spacing();
    Text("Special Thanks: Zarby89, JaredBrian");
    Separator();

    if (Button("Close", gui::kDefaultModalSize)) {
      about_ = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }
}

void EditorManager::DrawInfoPopup() {
  if (rom_info_)
    OpenPopup("ROM Information");
  if (BeginPopupModal("ROM Information", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Title: %s", rom()->title().c_str());
    Text("ROM Size: %s", core::HexLongLong(rom()->size()).c_str());

    if (Button("Close", gui::kDefaultModalSize) ||
        IsKeyPressed(ImGuiKey_Escape)) {
      rom_info_ = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }
}

void EditorManager::DrawYazeMenu() {
  static bool show_display_settings = false;

  if (BeginMenuBar()) {
    DrawYazeMenuBar();
    SameLine(GetWindowWidth() - GetStyle().ItemSpacing.x -
             CalcTextSize(ICON_MD_DISPLAY_SETTINGS).x - 110);
    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (Button(ICON_MD_DISPLAY_SETTINGS)) {
      show_display_settings = !show_display_settings;
    }
    PopStyleColor();
    Text("yaze v%s", core::kYazeVersion.data());
    EndMenuBar();
  }

  if (show_display_settings) {
    Begin("Display Settings", &show_display_settings, ImGuiWindowFlags_None);
    gui::DrawDisplaySettings();
    End();
  }
}

void EditorManager::DrawYazeMenuBar() {
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
        for (const auto &filePath : manager.GetRecentFiles()) {
          if (MenuItem(filePath.c_str())) {
            OpenRomOrProject(filePath);
          }
        }
      }
      EndMenu();
    }

    MENU_ITEM2("Save", "Ctrl+S") {
      if (rom()->is_loaded()) {
        SaveRom();
      }
    }
    MENU_ITEM("Save As..") { save_as_menu = true; }

    if (rom()->is_loaded()) {
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
        status_ = current_project_.Open(
            core::FileDialogWrapper::ShowOpenFileDialog());
        if (status_.ok()) {
          status_ = OpenProject();
        }
      }
      if (MenuItem("Save Project")) {
        // Save the current project
        status_ = current_project_.Save();
      }

      EndMenu();
    }

    if (BeginMenu("Options")) {
      MenuItem("Backup ROM", "", &backup_rom_);
      MenuItem("Save New Auto", "", &save_new_auto_);
      Separator();
      if (BeginMenu("Experiment Flags")) {
        static FlagsMenu flags_menu;
        flags_menu.Draw();
        EndMenu();
      }
      EndMenu();
    }

    Separator();

    if (MenuItem("Quit", "Ctrl+Q")) {
      quit_ = true;
    }

    EndMenu();
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    Begin("Save As..", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);
    InputText("Filename", &save_as_filename);
    if (Button("Save", gui::kDefaultModalSize)) {
      SaveRom();
      save_as_menu = false;
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) {
      save_as_menu = false;
    }
    End();
  }

  if (new_project_menu) {
    Begin("New Project", &new_project_menu, ImGuiWindowFlags_AlwaysAutoResize);
    static std::string save_as_filename = "";
    InputText("Project Name", &save_as_filename);
    if (Button("Destination Filepath", gui::kDefaultModalSize)) {
      current_project_.filepath = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.filepath.c_str());
    if (Button("ROM File", gui::kDefaultModalSize)) {
      current_project_.rom_filename_ = FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.rom_filename_.c_str());
    if (Button("Labels File", gui::kDefaultModalSize)) {
      current_project_.labels_filename_ =
          FileDialogWrapper::ShowOpenFileDialog();
    }
    SameLine();
    Text("%s", current_project_.labels_filename_.c_str());
    if (Button("Code Folder", gui::kDefaultModalSize)) {
      current_project_.code_folder_ = FileDialogWrapper::ShowOpenFolderDialog();
    }
    SameLine();
    Text("%s", current_project_.code_folder_.c_str());

    Separator();
    if (Button("Create", gui::kDefaultModalSize)) {
      new_project_menu = false;
      status_ = current_project_.Create(save_as_filename);
      if (status_.ok()) {
        status_ = current_project_.Save();
      }
    }
    SameLine();
    if (Button("Cancel", gui::kDefaultModalSize)) {
      new_project_menu = false;
    }
    End();
  }

  if (BeginMenu("Edit")) {
    MENU_ITEM2("Undo", "Ctrl+Z") { status_ = current_editor_->Undo(); }
    MENU_ITEM2("Redo", "Ctrl+Y") { status_ = current_editor_->Redo(); }
    Separator();
    MENU_ITEM2("Cut", "Ctrl+X") { status_ = current_editor_->Cut(); }
    MENU_ITEM2("Copy", "Ctrl+C") { status_ = current_editor_->Copy(); }
    MENU_ITEM2("Paste", "Ctrl+V") { status_ = current_editor_->Paste(); }
    Separator();
    MENU_ITEM2("Find", "Ctrl+F") { status_ = current_editor_->Find(); }
    EndMenu();
  }

  static bool show_imgui_metrics = false;
  static bool show_memory_editor = false;
  static bool show_asm_editor = false;
  static bool show_imgui_demo = false;
  static bool show_palette_editor = false;
  static bool show_emulator = false;

  if (show_imgui_demo)
    ShowDemoWindow();
  if (show_imgui_metrics)
    ShowMetricsWindow(&show_imgui_metrics);
  if (show_memory_editor)
    memory_editor_.Update(show_memory_editor);
  if (show_asm_editor)
    assembly_editor_.Update(show_asm_editor);

  if (show_emulator) {
    Begin("Emulator", &show_emulator, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    End();
  }

  if (show_palette_editor) {
    Begin("Palette Editor", &show_palette_editor);
    status_ = palette_editor_.Update();
    End();
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
    EndMenu();
  }

  static bool show_resource_label_manager = false;
  if (current_project_.project_opened_) {
    if (BeginMenu("Project")) {
      Text("Name: %s", current_project_.name.c_str());
      Text("ROM: %s", current_project_.rom_filename_.c_str());
      Text("Labels: %s", current_project_.labels_filename_.c_str());
      Text("Code: %s", current_project_.code_folder_.c_str());
      Separator();
      MenuItem("Resource Labels", nullptr, &show_resource_label_manager);
      EndMenu();
    }
  }

  static bool open_rom_help = false;
  static bool open_supported_features = false;
  static bool open_manage_project = false;
  if (BeginMenu("Help")) {
    if (MenuItem("How to open a ROM"))
      open_rom_help = true;
    if (MenuItem("Supported Features"))
      open_supported_features = true;
    if (MenuItem("How to manage a project"))
      open_manage_project = true;

    if (MenuItem("About", "F1"))
      about_ = true;
    EndMenu();
  }

  if (open_supported_features)
    OpenPopup("Supported Features");
  if (BeginPopupModal("Supported Features", nullptr,
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

    if (Button("Close", gui::kDefaultModalSize)) {
      open_supported_features = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }

  if (open_rom_help)
    OpenPopup("Open a ROM");
  if (BeginPopupModal("Open a ROM", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("File -> Open");
    Text("Select a ROM file to open");
    Text("Supported ROMs (headered or unheadered):");
    Text("The Legend of Zelda: A Link to the Past");
    Text("US Version 1.0");
    Text("JP Version 1.0");

    if (Button("Close", gui::kDefaultModalSize)) {
      open_rom_help = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }

  if (open_manage_project)
    OpenPopup("Manage Project");
  if (BeginPopupModal("Manage Project", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Project Menu");
    Text("Create a new project or open an existing one.");
    Text("Save the project to save the current state of the project.");
    TextWrapped(
        "To save a project, you need to first open a ROM and initialize your "
        "code path and labels file. Label resource manager can be found in "
        "the View menu. Code path is set in the Code editor after opening a "
        "folder.");

    if (Button("Close", gui::kDefaultModalSize)) {
      open_manage_project = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }

  if (show_resource_label_manager) {
    rom()->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened_ &&
        !current_project_.labels_filename_.empty()) {
      current_project_.labels_filename_ = rom()->resource_label()->filename_;
    }
  }
}

void EditorManager::LoadRom() {
  auto file_name = FileDialogWrapper::ShowOpenFileDialog();
  auto load_rom = rom()->LoadFromFile(file_name);
  if (load_rom.ok()) {
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    manager.AddFile(file_name);
    manager.Save();
  }
}

void EditorManager::SaveRom() {
  if (flags()->kSaveDungeonMaps) {
    status_ = screen_editor_.SaveDungeonMaps();
    RETURN_VOID_IF_ERROR(status_);
  }

  status_ = overworld_editor_.Save();
  RETURN_VOID_IF_ERROR(status_);

  status_ = rom()->SaveToFile(backup_rom_, save_new_auto_);
}

void EditorManager::OpenRomOrProject(const std::string &filename) {
  if (absl::StrContains(filename, ".yaze")) {
    status_ = current_project_.Open(filename);
    if (status_.ok()) {
      status_ = OpenProject();
    }
  } else {
    status_ = rom()->LoadFromFile(filename);
  }
}

absl::Status EditorManager::OpenProject() {
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

} // namespace editor
} // namespace yaze

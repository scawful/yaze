#include "editor_manager.h"

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "app/core/features.h"
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
#include "app/emu/emulator.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "editor/editor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/hex.h"
#include "util/macro.h"

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

std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

}  // namespace

void EditorManager::Initialize(const std::string &filename) {
  if (!filename.empty()) {
    PRINT_IF_ERROR(rom()->LoadFromFile(filename));
  }

  std::vector<gui::MenuItem> recent_files;
  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  if (manager.GetRecentFiles().empty()) {
    recent_files.emplace_back("No Recent Files", "", nullptr);
  } else {
    for (const auto &filePath : manager.GetRecentFiles()) {
      recent_files.emplace_back(filePath, "",
                                [&]() { OpenRomOrProject(filePath); });
    }
  }

  std::vector<gui::MenuItem> options_subitems;
  options_subitems.emplace_back(
      "Backup ROM", "", [&]() { backup_rom_ |= backup_rom_; },
      [&]() { return backup_rom_; });
  options_subitems.emplace_back(
      "Save New Auto", "", [&]() { save_new_auto_ |= save_new_auto_; },
      [&]() { return save_new_auto_; });

  std::vector<gui::MenuItem> project_menu_subitems;
  project_menu_subitems.emplace_back("New Project", "",
                                     [&]() { new_project_menu = true; });
  project_menu_subitems.emplace_back("Open Project", "",
                                     [&]() { status_ = OpenProject(); });
  project_menu_subitems.emplace_back(
      "Save Project", "", [&]() { SaveProject(); },
      [&]() { return current_project_.project_opened_; });

  context_.shortcut_manager.RegisterShortcut(
      "Open", {ImGuiKey_O, ImGuiMod_Ctrl}, [&]() { LoadRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Save", {ImGuiKey_S, ImGuiMod_Ctrl}, [&]() { SaveRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close", {ImGuiKey_W, ImGuiMod_Ctrl}, [&]() { rom()->Close(); });
  context_.shortcut_manager.RegisterShortcut(
      "Quit", {ImGuiKey_Q, ImGuiMod_Ctrl}, [&]() { quit_ = true; });

  context_.shortcut_manager.RegisterShortcut(
      "Undo", {ImGuiKey_Z, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Undo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Redo", {ImGuiKey_Y, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Redo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Cut", {ImGuiKey_X, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Cut(); });
  context_.shortcut_manager.RegisterShortcut(
      "Copy", {ImGuiKey_C, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Copy(); });
  context_.shortcut_manager.RegisterShortcut(
      "Paste", {ImGuiKey_V, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Paste(); });
  context_.shortcut_manager.RegisterShortcut(
      "Find", {ImGuiKey_F, ImGuiMod_Ctrl},
      [&]() { status_ = current_editor_->Find(); });

  context_.shortcut_manager.RegisterShortcut(
      "Load Last ROM", {ImGuiKey_R, ImGuiMod_Ctrl}, [&]() {
        manager.Load();
        if (!manager.GetRecentFiles().empty()) {
          auto front = manager.GetRecentFiles().front();
          OpenRomOrProject(front);
        }
      });

  context_.shortcut_manager.RegisterShortcut("F1", ImGuiKey_F1,
                                             [&]() { about_ = true; });

  gui::kMainMenu = {
      {"File",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_FILE_OPEN, " Open"),
            context_.shortcut_manager.GetKeys("Open"),
            context_.shortcut_manager.GetCallback("Open")},
           {"Open Recent", "", [&]() {},
            []() { return !manager.GetRecentFiles().empty(); }, recent_files},
           {absl::StrCat(ICON_MD_FILE_DOWNLOAD, " Save"),
            context_.shortcut_manager.GetKeys("Save"),
            context_.shortcut_manager.GetCallback("Save")},
           {absl::StrCat(ICON_MD_SAVE_AS, " Save As.."), "",
            [&]() { save_as_menu_ = true; }},
           {absl::StrCat(ICON_MD_BALLOT, " Project"), "", [&]() {},
            []() { return true; }, project_menu_subitems},
           {absl::StrCat(ICON_MD_CLOSE, " Close"), "",
            [&]() { rom()->Close(); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_MISCELLANEOUS_SERVICES, " Options"), "",
            [&]() {}, []() { return true; }, options_subitems},
           {absl::StrCat(ICON_MD_EXIT_TO_APP, " Quit"), "Ctrl+Q",
            [&]() { quit_ = true; }},
       }},
      {"Edit",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_CONTENT_CUT, " Cut"),
            context_.shortcut_manager.GetKeys("Cut"),
            context_.shortcut_manager.GetCallback("Cut")},
           {absl::StrCat(ICON_MD_CONTENT_COPY, " Copy"),
            context_.shortcut_manager.GetKeys("Copy"),
            context_.shortcut_manager.GetCallback("Copy")},
           {absl::StrCat(ICON_MD_CONTENT_PASTE, " Paste"),
            context_.shortcut_manager.GetKeys("Paste"),
            context_.shortcut_manager.GetCallback("Paste")},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_UNDO, " Undo"),
            context_.shortcut_manager.GetKeys("Undo"),
            context_.shortcut_manager.GetCallback("Undo")},
           {absl::StrCat(ICON_MD_REDO, " Redo"),
            context_.shortcut_manager.GetKeys("Redo"),
            context_.shortcut_manager.GetCallback("Redo")},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_SEARCH, " Find"),
            context_.shortcut_manager.GetKeys("Find"),
            context_.shortcut_manager.GetCallback("Find")},
       }},
      {"View",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_CODE, " Assembly Editor"), "",
            [&]() { show_asm_editor_ = true; },
            [&]() { return show_asm_editor_; }},
           {absl::StrCat(ICON_MD_CASTLE, " Dungeon Editor"), "",
            [&]() { dungeon_editor_.set_active(true); },
            [&]() { return *dungeon_editor_.active(); }},
           {absl::StrCat(ICON_MD_PHOTO, " Graphics Editor"), "",
            [&]() { graphics_editor_.set_active(true); },
            [&]() { return *graphics_editor_.active(); }},
           {absl::StrCat(ICON_MD_MUSIC_NOTE, " Music Editor"), "",
            [&]() { music_editor_.set_active(true); },
            [&]() { return *music_editor_.active(); }},
           {absl::StrCat(ICON_MD_LAYERS, " Overworld Editor"), "",
            [&]() { overworld_editor_.set_active(true); },
            [&]() { return *overworld_editor_.active(); }},
           {absl::StrCat(ICON_MD_PALETTE, " Palette Editor"), "",
            [&]() { palette_editor_.set_active(true); },
            [&]() { return *palette_editor_.active(); }},
           {absl::StrCat(ICON_MD_SCREENSHOT, " Screen Editor"), "",
            [&]() { screen_editor_.set_active(true); },
            [&]() { return *screen_editor_.active(); }},
           {absl::StrCat(ICON_MD_SMART_TOY, " Sprite Editor"), "",
            [&]() { sprite_editor_.set_active(true); },
            [&]() { return *sprite_editor_.active(); }},
           {absl::StrCat(ICON_MD_MESSAGE, " Message Editor"), "",
            [&]() { message_editor_.set_active(true); },
            [&]() { return *message_editor_.active(); }},
           {absl::StrCat(ICON_MD_SETTINGS, " Settings Editor"), "",
            [&]() { settings_editor_.set_active(true); },
            [&]() { return *settings_editor_.active(); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_GAMEPAD, " Emulator"), "",
            [&]() { show_emulator_ = true; }},
           {absl::StrCat(ICON_MD_MEMORY, " Memory Editor"), "",
            [&]() { show_memory_editor_ = true; }},
           {absl::StrCat(ICON_MD_SIM_CARD, " ROM Metadata"), "",
            [&]() { rom_info_ = true; }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_HELP, " ImGui Demo"), "",
            [&]() { show_imgui_demo_ = true; }},
           {absl::StrCat(ICON_MD_HELP, " ImGui Metrics"), "",
            [&]() { show_imgui_metrics_ = true; }},
       }},
      {"Workspace",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_SPACE_DASHBOARD, " Layout"), "",
            [&]() { show_workspace_layout = true; }},
       }},
      {"Help",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_HELP, " How to open a ROM"), "",
            [&]() { open_rom_help = true; }},
           {absl::StrCat(ICON_MD_HELP, " Supported Features"), "",
            [&]() { open_supported_features = true; }},
           {absl::StrCat(ICON_MD_HELP, " How to manage a project"), "",
            [&]() { open_manage_project = true; }},
           {absl::StrCat(ICON_MD_HELP, " About"), "F1",
            [&]() { about_ = true; }},
       }}};

  overworld_editor_.Initialize();
}

absl::Status EditorManager::Update() {
  DrawPopups();
  ExecuteShortcuts(context_.shortcut_manager);

  for (auto editor : active_editors_) {
    if (*editor->active()) {
      if (ImGui::Begin(GetEditorName(editor->type()).c_str(),
                       editor->active())) {
        current_editor_ = editor;
        status_ = editor->Update();
      }
      ImGui::End();
    }
  }

  static bool show_home = true;
  ImGui::Begin("Home", &show_home);
  if (!current_rom_) {
    DrawHomepage();
  } else {
    ManageActiveEditors();
  }
  ImGui::End();
  return absl::OkStatus();
}

void EditorManager::ManageActiveEditors() {
  // Show popup pane to select an editor to add
  static bool show_add_editor = false;
  if (show_add_editor) OpenPopup("AddEditor");

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

void EditorManager::DrawHomepage() {
  TextWrapped("Welcome to the Yet Another Zelda3 Editor (yaze)!");
  TextWrapped(
      "This editor is designed to be a comprehensive tool for editing the "
      "Legend of Zelda: A Link to the Past.");
  TextWrapped(
      "The editor is still in development, so please report any bugs or issues "
      "you encounter.");

  if (gui::ClickableText("Open a ROM")) {
    LoadRom();
  }
}

void EditorManager::DrawPopups() {
  static bool show_status_ = false;
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

  if (about_) OpenPopup("About");
  if (BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Yet Another Zelda3 Editor - v%s", version_.c_str());
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

  if (rom_info_) OpenPopup("ROM Information");
  if (BeginPopupModal("ROM Information", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
    Text("Title: %s", rom()->title().c_str());
    Text("ROM Size: %s", util::HexLongLong(rom()->size()).c_str());

    if (Button("Close", gui::kDefaultModalSize) ||
        IsKeyPressed(ImGuiKey_Escape)) {
      rom_info_ = false;
      CloseCurrentPopup();
    }
    EndPopup();
  }
}

void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;
  static bool save_as_menu = false;

  if (BeginMenuBar()) {
    gui::DrawMenu(gui::kMainMenu);

    SameLine(GetWindowWidth() - GetStyle().ItemSpacing.x -
             CalcTextSize(ICON_MD_DISPLAY_SETTINGS).x - 110);
    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    if (Button(ICON_MD_DISPLAY_SETTINGS)) {
      show_display_settings = !show_display_settings;
    }
    PopStyleColor();
    Text("yaze v%s", version_.c_str());
    EndMenuBar();
  }

  if (show_display_settings) {
    Begin("Display Settings", &show_display_settings, ImGuiWindowFlags_None);
    gui::DrawDisplaySettings();
    End();
  }

  if (show_imgui_demo_) ShowDemoWindow();
  if (show_imgui_metrics_) ShowMetricsWindow(&show_imgui_metrics_);
  if (show_memory_editor_) memory_editor_.Update(show_memory_editor_);
  if (show_asm_editor_) assembly_editor_.Update(show_asm_editor_);

  if (show_emulator_) {
    Begin("Emulator", &show_emulator_, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    End();
  }

  if (show_palette_editor_) {
    Begin("Palette Editor", &show_palette_editor_);
    status_ = palette_editor_.Update();
    End();
  }

  if (open_supported_features) OpenPopup("Supported Features");
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

  if (open_rom_help) OpenPopup("Open a ROM");
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

  if (open_manage_project) OpenPopup("Manage Project");
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
}

void EditorManager::LoadRom() {
  auto file_name = FileDialogWrapper::ShowOpenFileDialog();
  auto load_rom = rom()->LoadFromFile(file_name);
  if (load_rom.ok()) {
    current_rom_ = rom();
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    manager.AddFile(file_name);
    manager.Save();
    LoadAssets();
  }
}

void EditorManager::LoadAssets() {
  auto load_rom_assets = [&]() -> absl::Status {
    auto &sheet_manager = GraphicsSheetManager::GetInstance();
    ASSIGN_OR_RETURN(*sheet_manager.mutable_gfx_sheets(),
                     LoadAllGraphicsData(*rom()))
    RETURN_IF_ERROR(overworld_editor_.Load());
    RETURN_IF_ERROR(dungeon_editor_.Load());
    return absl::OkStatus();
  };
  if (!load_rom_assets().ok()) {
    status_ = load_rom_assets();
  }
}

void EditorManager::SaveRom() {
  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    status_ = screen_editor_.SaveDungeonMaps();
    RETURN_VOID_IF_ERROR(status_);
  }

  status_ = overworld_editor_.Save();
  RETURN_VOID_IF_ERROR(status_);

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    PRINT_IF_ERROR(SaveAllGraphicsData(
        *rom(), GraphicsSheetManager::GetInstance().gfx_sheets()));

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
    current_rom_ = rom();
    LoadAssets();
  }
}

absl::Status EditorManager::OpenProject() {
  RETURN_IF_ERROR(rom()->LoadFromFile(current_project_.rom_filename_));
  current_rom_ = rom();

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
  LoadAssets();
  return absl::OkStatus();
}

void EditorManager::SaveProject() {
  if (current_project_.project_opened_) {
    status_ = current_project_.Save();
  } else {
    new_project_menu = true;
  }
}

}  // namespace editor
}  // namespace yaze

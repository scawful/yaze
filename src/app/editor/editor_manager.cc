#include "editor_manager.h"

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
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
#include "app/gfx/arena.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "editor/editor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using namespace ImGui;
using core::FileDialogWrapper;

namespace {

std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

}  // namespace

constexpr const char *kOverworldEditorName = ICON_MD_LAYERS " Overworld Editor";
constexpr const char *kGraphicsEditorName = ICON_MD_PHOTO " Graphics Editor";
constexpr const char *kPaletteEditorName = ICON_MD_PALETTE " Palette Editor";
constexpr const char *kScreenEditorName = ICON_MD_SCREENSHOT " Screen Editor";
constexpr const char *kSpriteEditorName = ICON_MD_SMART_TOY " Sprite Editor";
constexpr const char *kMessageEditorName = ICON_MD_MESSAGE " Message Editor";
constexpr const char *kSettingsEditorName = ICON_MD_SETTINGS " Settings Editor";
constexpr const char *kAssemblyEditorName = ICON_MD_CODE " Assembly Editor";
constexpr const char *kDungeonEditorName = ICON_MD_CASTLE " Dungeon Editor";
constexpr const char *kMusicEditorName = ICON_MD_MUSIC_NOTE " Music Editor";

void EditorManager::Initialize(const std::string &filename) {
  // Point to a blank editor set when no ROM is loaded
  current_editor_set_ = &blank_editor_set_;

  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Initialize popup manager
  popup_manager_ = std::make_unique<PopupManager>(this);
  popup_manager_->Initialize();

  // Set the popup manager in the context
  context_.popup_manager = popup_manager_.get();

  context_.shortcut_manager.RegisterShortcut(
      "Open", {ImGuiKey_O, ImGuiMod_Ctrl}, [this]() { status_ = LoadRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Save", {ImGuiKey_S, ImGuiMod_Ctrl}, [this]() { status_ = SaveRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close", {ImGuiKey_W, ImGuiMod_Ctrl}, [this]() {
        if (current_rom_) current_rom_->Close();
      });
  context_.shortcut_manager.RegisterShortcut(
      "Quit", {ImGuiKey_Q, ImGuiMod_Ctrl}, [this]() { quit_ = true; });

  context_.shortcut_manager.RegisterShortcut(
      "Undo", {ImGuiKey_Z, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Undo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Redo", {ImGuiKey_Y, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Redo(); });
  context_.shortcut_manager.RegisterShortcut(
      "Cut", {ImGuiKey_X, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Cut(); });
  context_.shortcut_manager.RegisterShortcut(
      "Copy", {ImGuiKey_C, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Copy(); });
  context_.shortcut_manager.RegisterShortcut(
      "Paste", {ImGuiKey_V, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Paste(); });
  context_.shortcut_manager.RegisterShortcut(
      "Find", {ImGuiKey_F, ImGuiMod_Ctrl},
      [this]() { status_ = current_editor_->Find(); });

  context_.shortcut_manager.RegisterShortcut(
      "Load Last ROM", {ImGuiKey_R, ImGuiMod_Ctrl}, [this]() {
        static RecentFilesManager manager("recent_files.txt");
        manager.Load();
        if (!manager.GetRecentFiles().empty()) {
          auto front = manager.GetRecentFiles().front();
          status_ = OpenRomOrProject(front);
        }
      });

  context_.shortcut_manager.RegisterShortcut(
      "F1", ImGuiKey_F1, [this]() { popup_manager_->Show("About"); });

  // Initialize menu items
  std::vector<gui::MenuItem> recent_files;
  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  if (manager.GetRecentFiles().empty()) {
    recent_files.emplace_back("No Recent Files", "", nullptr);
  } else {
    for (const auto &filePath : manager.GetRecentFiles()) {
      recent_files.emplace_back(filePath, "", [filePath, this]() {
        status_ = OpenRomOrProject(filePath);
      });
    }
  }

  std::vector<gui::MenuItem> options_subitems;
  options_subitems.emplace_back(
      "Backup ROM", "", [this]() { backup_rom_ |= backup_rom_; },
      [this]() { return backup_rom_; });
  options_subitems.emplace_back(
      "Save New Auto", "", [this]() { save_new_auto_ |= save_new_auto_; },
      [this]() { return save_new_auto_; });

  std::vector<gui::MenuItem> project_menu_subitems;
  project_menu_subitems.emplace_back(
      "New Project", "", [this]() { popup_manager_->Show("New Project"); });
  project_menu_subitems.emplace_back("Open Project", "",
                                     [this]() { status_ = OpenProject(); });
  project_menu_subitems.emplace_back(
      "Save Project", "", [this]() { status_ = SaveProject(); },
      [this]() { return current_project_.project_opened_; });

  gui::kMainMenu = {
      {"File",
       {},
       {},
       {},
       {
           {absl::StrCat(ICON_MD_FILE_OPEN, " Open"),
            context_.shortcut_manager.GetKeys("Open"),
            context_.shortcut_manager.GetCallback("Open")},
           {"Open Recent", "", []() {},
            []() { return !manager.GetRecentFiles().empty(); }, recent_files},
           {absl::StrCat(ICON_MD_FILE_DOWNLOAD, " Save"),
            context_.shortcut_manager.GetKeys("Save"),
            context_.shortcut_manager.GetCallback("Save")},
           {absl::StrCat(ICON_MD_SAVE_AS, " Save As.."), "",
            [this]() { popup_manager_->Show("Save As.."); }},
           {absl::StrCat(ICON_MD_BALLOT, " Project"), "", []() {},
            []() { return true; }, project_menu_subitems},
           {absl::StrCat(ICON_MD_CLOSE, " Close"), "",
            [this]() {
              if (current_rom_) current_rom_->Close();
            }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_MISCELLANEOUS_SERVICES, " Options"), "",
            []() {}, []() { return true; }, options_subitems},
           {absl::StrCat(ICON_MD_EXIT_TO_APP, " Quit"), "Ctrl+Q",
            [this]() { quit_ = true; }},
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
           {absl::StrCat(ICON_MD_HOME, " Home"), "",
            [&]() { show_homepage_ = true; }},
           {kAssemblyEditorName, "", [&]() { show_asm_editor_ = true; },
            [&]() { return show_asm_editor_; }},
           {kDungeonEditorName, "",
            [&]() { current_editor_set_->dungeon_editor_.set_active(true); },
            [&]() { return *current_editor_set_->dungeon_editor_.active(); }},
           {kGraphicsEditorName, "",
            [&]() { current_editor_set_->graphics_editor_.set_active(true); },
            [&]() { return *current_editor_set_->graphics_editor_.active(); }},
           {kMusicEditorName, "",
            [&]() { current_editor_set_->music_editor_.set_active(true); },
            [&]() { return *current_editor_set_->music_editor_.active(); }},
           {kOverworldEditorName, "",
            [&]() { current_editor_set_->overworld_editor_.set_active(true); },
            [&]() { return *current_editor_set_->overworld_editor_.active(); }},
           {kPaletteEditorName, "",
            [&]() { current_editor_set_->palette_editor_.set_active(true); },
            [&]() { return *current_editor_set_->palette_editor_.active(); }},
           {kScreenEditorName, "",
            [&]() { current_editor_set_->screen_editor_.set_active(true); },
            [&]() { return *current_editor_set_->screen_editor_.active(); }},
           {kSpriteEditorName, "",
            [&]() { current_editor_set_->sprite_editor_.set_active(true); },
            [&]() { return *current_editor_set_->sprite_editor_.active(); }},
           {kMessageEditorName, "",
            [&]() { current_editor_set_->message_editor_.set_active(true); },
            [&]() { return *current_editor_set_->message_editor_.active(); }},
           {kSettingsEditorName, "",
            [&]() { current_editor_set_->settings_editor_.set_active(true); },
            [&]() { return *current_editor_set_->settings_editor_.active(); }},
           {gui::kSeparator, "", nullptr, []() { return true; }},
           {absl::StrCat(ICON_MD_GAMEPAD, " Emulator"), "",
            [&]() { show_emulator_ = true; }},
           {absl::StrCat(ICON_MD_MEMORY, " Memory Editor"), "",
            [&]() { show_memory_editor_ = true; }},
           {absl::StrCat(ICON_MD_SIM_CARD, " ROM Metadata"), "",
            [&]() { popup_manager_->Show("ROM Information"); }},
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
            [&]() { popup_manager_->Show("Open a ROM"); }},
           {absl::StrCat(ICON_MD_HELP, " Supported Features"), "",
            [&]() { popup_manager_->Show("Supported Features"); }},
           {absl::StrCat(ICON_MD_HELP, " How to manage a project"), "",
            [&]() { popup_manager_->Show("Manage Project"); }},
           {absl::StrCat(ICON_MD_HELP, " About"), "F1",
            [&]() { popup_manager_->Show("About"); }},
       }}};
}

absl::Status EditorManager::Update() {
  popup_manager_->DrawPopups();
  ExecuteShortcuts(context_.shortcut_manager);

  if (show_homepage_) {
    ImGui::Begin("Home", &show_homepage_);
    DrawHomepage();
    ImGui::End();
  }

  if (!current_editor_set_) {
    return absl::OkStatus();
  }
  for (auto editor : current_editor_set_->active_editors_) {
    if (*editor->active()) {
      if (editor->type() == EditorType::kOverworld) {
        auto &overworld_editor = static_cast<OverworldEditor &>(*editor);
        if (overworld_editor.jump_to_tab() != -1) {
          current_editor_set_->dungeon_editor_.set_active(true);
          // Set the dungeon editor to the jump to tab
          current_editor_set_->dungeon_editor_.add_room(
              overworld_editor.jump_to_tab());
          overworld_editor.jump_to_tab_ = -1;
        }
      }

      if (ImGui::Begin(GetEditorName(editor->type()).c_str(),
                       editor->active())) {
        current_editor_ = editor;
        status_ = editor->Update();
      }
      ImGui::End();
    }
  }
  return absl::OkStatus();
}

void EditorManager::DrawHomepage() {
  TextWrapped("Welcome to the Yet Another Zelda3 Editor (yaze)!");
  TextWrapped("The Legend of Zelda: A Link to the Past.");
  TextWrapped("Please report any bugs or issues you encounter.");
  ImGui::SameLine();
  if (gui::ClickableText("https://github.com/scawful/yaze")) {
    gui::OpenUrl("https://github.com/scawful/yaze");
  }

  if (!current_rom_) {
    TextWrapped("No ROM loaded.");
    if (gui::ClickableText("Open a ROM")) {
      status_ = LoadRom();
    }
    SameLine();
    Checkbox("Load custom overworld features",
             &core::FeatureFlags::get().overworld.kLoadCustomOverworld);

    ImGui::BeginChild("Recent Files", ImVec2(-1, -1), true);
    static RecentFilesManager manager("recent_files.txt");
    manager.Load();
    for (const auto &file : manager.GetRecentFiles()) {
      if (gui::ClickableText(file.c_str())) {
        status_ = OpenRomOrProject(file);
      }
    }
    ImGui::EndChild();
    return;
  }

  TextWrapped("Current ROM: %s", current_rom_->filename().c_str());
  if (Button(kOverworldEditorName)) {
    current_editor_set_->overworld_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kDungeonEditorName)) {
    current_editor_set_->dungeon_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kGraphicsEditorName)) {
    current_editor_set_->graphics_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kMessageEditorName)) {
    current_editor_set_->message_editor_.set_active(true);
  }

  if (Button(kPaletteEditorName)) {
    current_editor_set_->palette_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kScreenEditorName)) {
    current_editor_set_->screen_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kSpriteEditorName)) {
    current_editor_set_->sprite_editor_.set_active(true);
  }
  ImGui::SameLine();
  if (Button(kMusicEditorName)) {
    current_editor_set_->music_editor_.set_active(true);
  }

  if (Button(kSettingsEditorName)) {
    current_editor_set_->settings_editor_.set_active(true);
  }
}

absl::Status EditorManager::DrawRomSelector() {
  SameLine((GetWindowWidth() / 2) - 100);
  if (current_rom_ && current_rom_->is_loaded()) {
    SetNextItemWidth(GetWindowWidth() / 6);
    if (BeginCombo("##ROMSelector", current_rom_->short_name().c_str())) {
      for (const auto &session : sessions_) {
        const Rom* rom = &session.rom;
        if (MenuItem(rom->short_name().c_str())) {
          RETURN_IF_ERROR(SetCurrentRom(const_cast<Rom*>(rom)));
        }
      }
      EndCombo();
    }
  } else {
    Text("No ROM loaded");
  }
  return absl::OkStatus();
}

void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;
  static bool save_as_menu = false;

  if (BeginMenuBar()) {
    gui::DrawMenu(gui::kMainMenu);

    status_ = DrawRomSelector();

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
  if (show_memory_editor_ && current_editor_set_) {
    current_editor_set_->memory_editor_.Update(show_memory_editor_);
  }
  if (show_asm_editor_ && current_editor_set_) {
    current_editor_set_->assembly_editor_.Update(show_asm_editor_);
  }

  if (show_emulator_) {
    Begin("Emulator", &show_emulator_, ImGuiWindowFlags_MenuBar);
    emulator_.Run();
    End();
  }

  if (show_palette_editor_ && current_editor_set_) {
    Begin("Palette Editor", &show_palette_editor_);
    status_ = current_editor_set_->palette_editor_.Update();
    End();
  }

  if (show_resource_label_manager && current_rom_) {
    current_rom_->resource_label()->DisplayLabels(&show_resource_label_manager);
    if (current_project_.project_opened_ &&
        !current_project_.labels_filename_.empty()) {
      current_project_.labels_filename_ =
          current_rom_->resource_label()->filename_;
    }
  }

  if (save_as_menu) {
    static std::string save_as_filename = "";
    Begin("Save As..", &save_as_menu, ImGuiWindowFlags_AlwaysAutoResize);
    InputText("Filename", &save_as_filename);
    if (Button("Save", gui::kDefaultModalSize)) {
      status_ = SaveRom();
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

absl::Status EditorManager::LoadRom() {
  auto file_name = FileDialogWrapper::ShowOpenFileDialog();
  Rom temp_rom;
  RETURN_IF_ERROR(temp_rom.LoadFromFile(file_name));

  sessions_.emplace_back(std::move(temp_rom));
  RomSession &session = sessions_.back();
  // Wire editor contexts
  for (auto *editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(file_name);
  manager.Save();
  RETURN_IF_ERROR(LoadAssets());

  return absl::OkStatus();
}

absl::Status EditorManager::LoadAssets() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }
  current_editor_set_->overworld_editor_.Initialize();
  current_editor_set_->message_editor_.Initialize();
  ASSIGN_OR_RETURN(*gfx::Arena::Get().mutable_gfx_sheets(),
                   LoadAllGraphicsData(*current_rom_));
  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->dungeon_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->screen_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->settings_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->sprite_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->message_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->music_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->palette_editor_.Load());
  return absl::OkStatus();
}

absl::Status EditorManager::SaveRom() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(zelda3::SaveDungeonMaps(
        *current_rom_, current_editor_set_->screen_editor_.dungeon_maps_));
  }

  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(
        SaveAllGraphicsData(*current_rom_, gfx::Arena::Get().gfx_sheets()));

  Rom::SaveSettings settings;
  settings.backup = backup_rom_;
  settings.save_new = save_new_auto_;
  return current_rom_->SaveToFile(settings);
}

absl::Status EditorManager::OpenRomOrProject(const std::string &filename) {
  if (absl::StrContains(filename, ".yaze")) {
    RETURN_IF_ERROR(current_project_.Open(filename));
    RETURN_IF_ERROR(OpenProject());
  } else {
    Rom temp_rom;
    RETURN_IF_ERROR(temp_rom.LoadFromFile(filename));
    sessions_.emplace_back(std::move(temp_rom));
    RomSession &session = sessions_.back();
    for (auto *editor : session.editors.active_editors_) {
      editor->set_context(&context_);
    }
    current_rom_ = &session.rom;
    current_editor_set_ = &session.editors;
    RETURN_IF_ERROR(LoadAssets());
  }
  return absl::OkStatus();
}

absl::Status EditorManager::OpenProject() {
  Rom temp_rom;
  RETURN_IF_ERROR(temp_rom.LoadFromFile(current_project_.rom_filename_));

  if (!temp_rom.resource_label()->LoadLabels(
          current_project_.labels_filename_)) {
    return absl::InternalError(
        "Could not load labels file, update your project file.");
  }

  sessions_.emplace_back(std::move(temp_rom));
  RomSession &session = sessions_.back();
  for (auto *editor : session.editors.active_editors_) {
    editor->set_context(&context_);
  }
  current_rom_ = &session.rom;
  current_editor_set_ = &session.editors;

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(current_project_.filepath + "/" + current_project_.name +
                  ".yaze");
  manager.Save();
  if (current_editor_set_) {
    current_editor_set_->assembly_editor_.OpenFolder(
        current_project_.code_folder_);
  }
  current_project_.project_opened_ = true;
  RETURN_IF_ERROR(LoadAssets());
  return absl::OkStatus();
}

absl::Status EditorManager::SaveProject() {
  if (current_project_.project_opened_) {
    RETURN_IF_ERROR(current_project_.Save());
  } else {
    new_project_menu = true;
  }
  return absl::OkStatus();
}

absl::Status EditorManager::SetCurrentRom(Rom *rom) {
  if (!rom) {
    return absl::InvalidArgumentError("Invalid ROM pointer");
  }

  for (auto &session : sessions_) {
    if (&session.rom == rom) {
      current_rom_ = &session.rom;
      current_editor_set_ = &session.editors;
      return absl::OkStatus();
    }
  }
  // If ROM wasn't found in existing sessions, treat as new session.
  // Copying an external ROM object is avoided; instead, fail.
  return absl::NotFoundError("ROM not found in existing sessions");
}

}  // namespace editor
}  // namespace yaze

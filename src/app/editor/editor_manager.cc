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
  return std::ranges::find(active_editors, editor) != active_editors.end();
}

std::string GetEditorName(EditorType type) {
  return kEditorNames[static_cast<int>(type)];
}

void DrawRomSelector(EditorManager &editor_manager) {
  const auto &roms = editor_manager.GetRoms();
  const Rom *current_rom = editor_manager.GetCurrentRom();

  SameLine((GetWindowWidth() / 2) - 100);
  if (current_rom && current_rom->is_loaded()) {
    SetNextItemWidth(GetWindowWidth() / 4);
    if (BeginCombo("##ROMSelector", current_rom->filename().c_str())) {
      for (const auto &rom : roms) {
        if (MenuItem(rom->filename().c_str())) {
          editor_manager.SetCurrentRom(rom.get());
        }
      }
      EndCombo();
    }
  } else {
    Text("No ROM loaded");
  }
}

}  // namespace

void EditorManager::Initialize(const std::string &filename) {
  if (!filename.empty()) {
    PRINT_IF_ERROR(OpenRomOrProject(filename));
  }

  // Initialize popup manager
  popup_manager_ = std::make_unique<PopupManager>(this);
  popup_manager_->Initialize();

  // Set the popup manager in the context
  context_.popup_manager = popup_manager_.get();

  context_.shortcut_manager.RegisterShortcut(
      "Open", {ImGuiKey_O, ImGuiMod_Ctrl}, [&]() { LoadRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Save", {ImGuiKey_S, ImGuiMod_Ctrl}, [&]() { SaveRom(); });
  context_.shortcut_manager.RegisterShortcut(
      "Close", {ImGuiKey_W, ImGuiMod_Ctrl}, [&]() {
        if (current_rom_) current_rom_->Close();
      });
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
        static RecentFilesManager manager("recent_files.txt");
        manager.Load();
        if (!manager.GetRecentFiles().empty()) {
          auto front = manager.GetRecentFiles().front();
          OpenRomOrProject(front);
        }
      });

  context_.shortcut_manager.RegisterShortcut(
      "F1", ImGuiKey_F1, [&]() { popup_manager_->Show("About"); });

  // Initialize menu items
  std::vector<gui::MenuItem> recent_files;
  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  if (manager.GetRecentFiles().empty()) {
    recent_files.emplace_back("No Recent Files", "", nullptr);
  } else {
    for (const auto &filePath : manager.GetRecentFiles()) {
      recent_files.emplace_back(
          filePath, "", [filePath, this]() { OpenRomOrProject(filePath); });
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
  project_menu_subitems.emplace_back(
      "New Project", "", [&]() { popup_manager_->Show("New Project"); });
  project_menu_subitems.emplace_back("Open Project", "",
                                     [&]() { status_ = OpenProject(); });
  project_menu_subitems.emplace_back(
      "Save Project", "", [&]() { SaveProject(); },
      [&]() { return current_project_.project_opened_; });

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
            [&]() { popup_manager_->Show("Save As.."); }},
           {absl::StrCat(ICON_MD_BALLOT, " Project"), "", [&]() {},
            []() { return true; }, project_menu_subitems},
           {absl::StrCat(ICON_MD_CLOSE, " Close"), "",
            [&]() {
              if (current_rom_) current_rom_->Close();
            }},
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
           {absl::StrCat(ICON_MD_HOME, " Home"), "",
            [&]() { show_homepage_ = true; }},
           {absl::StrCat(ICON_MD_CODE, " Assembly Editor"), "",
            [&]() { show_asm_editor_ = true; },
            [&]() { return show_asm_editor_; }},
           {absl::StrCat(ICON_MD_CASTLE, " Dungeon Editor"), "",
            [&]() { current_editor_set_->dungeon_editor_.set_active(true); },
            [&]() { return *current_editor_set_->dungeon_editor_.active(); }},
           {absl::StrCat(ICON_MD_PHOTO, " Graphics Editor"), "",
            [&]() { current_editor_set_->graphics_editor_.set_active(true); },
            [&]() { return *current_editor_set_->graphics_editor_.active(); }},
           {absl::StrCat(ICON_MD_MUSIC_NOTE, " Music Editor"), "",
            [&]() { current_editor_set_->music_editor_.set_active(true); },
            [&]() { return *current_editor_set_->music_editor_.active(); }},
           {absl::StrCat(ICON_MD_LAYERS, " Overworld Editor"), "",
            [&]() { current_editor_set_->overworld_editor_.set_active(true); },
            [&]() { return *current_editor_set_->overworld_editor_.active(); }},
           {absl::StrCat(ICON_MD_PALETTE, " Palette Editor"), "",
            [&]() { current_editor_set_->palette_editor_.set_active(true); },
            [&]() { return *current_editor_set_->palette_editor_.active(); }},
           {absl::StrCat(ICON_MD_SCREENSHOT, " Screen Editor"), "",
            [&]() { current_editor_set_->screen_editor_.set_active(true); },
            [&]() { return *current_editor_set_->screen_editor_.active(); }},
           {absl::StrCat(ICON_MD_SMART_TOY, " Sprite Editor"), "",
            [&]() { current_editor_set_->sprite_editor_.set_active(true); },
            [&]() { return *current_editor_set_->sprite_editor_.active(); }},
           {absl::StrCat(ICON_MD_MESSAGE, " Message Editor"), "",
            [&]() { current_editor_set_->message_editor_.set_active(true); },
            [&]() { return *current_editor_set_->message_editor_.active(); }},
           {absl::StrCat(ICON_MD_SETTINGS, " Settings Editor"), "",
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

  if (current_editor_set_) {
    for (auto editor : current_editor_set_->active_editors_) {
      if (*editor->active()) {
        if (ImGui::Begin(GetEditorName(editor->type()).c_str(),
                         editor->active())) {
          current_editor_ = editor;
          status_ = editor->Update();
        }
        ImGui::End();
      }
    }

    static uint64_t last_cleanup_time = 0;
    uint64_t current_time = SDL_GetTicks64();

    // Clean up unused textures every 5 seconds
    if (current_time - last_cleanup_time > 5000) {
      current_editor_set_->CleanupUnusedTextures(current_time, 5000);
      last_cleanup_time = current_time;
    }
  }

  if (show_homepage_) {
    ImGui::Begin("Home", &show_homepage_);
    DrawHomepage();
    ImGui::End();
  }
  return absl::OkStatus();
}

void EditorManager::DrawHomepage() {
  TextWrapped("Welcome to the Yet Another Zelda3 Editor (yaze)!");
  TextWrapped(
      "This editor is designed to be a comprehensive tool for editing the "
      "Legend of Zelda: A Link to the Past.");
  TextWrapped(
      "The editor is still in development, so please report any bugs or issues "
      "you encounter.");

  if (current_rom_) {
    TextWrapped("Current ROM: %s", current_rom_->filename().c_str());
  } else {
    TextWrapped("No ROM loaded.");
    if (gui::ClickableText("Open a ROM")) {
      status_ = LoadRom();
    }
  }
}

void EditorManager::DrawPopups() {
  // This function is now handled by the PopupManager
  // We'll keep it as a placeholder in case we need to add more popup-related
  // functionality
}

void EditorManager::DrawMenuBar() {
  static bool show_display_settings = false;
  static bool save_as_menu = false;

  if (BeginMenuBar()) {
    gui::DrawMenu(gui::kMainMenu);

    DrawRomSelector(*this);

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
  auto new_rom = std::make_unique<Rom>();
  RETURN_IF_ERROR(new_rom->LoadFromFile(file_name));

  current_rom_ = new_rom.get();
  roms_.push_back(std::move(new_rom));

  // Create new editor set for this ROM
  auto editor_set = std::make_unique<EditorSet>(current_rom_);
  for (auto *editor : editor_set->active_editors_) {
    editor->set_context(&context_);
  }
  current_editor_set_ = editor_set.get();
  editor_sets_[current_rom_] = std::move(editor_set);

  static RecentFilesManager manager("recent_files.txt");
  manager.Load();
  manager.AddFile(file_name);
  manager.Save();
  RETURN_IF_ERROR(LoadAssets());

  // Show ROM info popup after loading
  popup_manager_->Show("ROM Information");

  return absl::OkStatus();
}

absl::Status EditorManager::LoadAssets() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }
  current_editor_set_->overworld_editor_.Initialize();
  current_editor_set_->message_editor_.Initialize();

  auto &sheet_manager = GraphicsSheetManager::GetInstance();
  ASSIGN_OR_RETURN(*sheet_manager.mutable_gfx_sheets(),
                   LoadAllGraphicsData(*current_rom_));
  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Load());
  RETURN_IF_ERROR(current_editor_set_->dungeon_editor_.Load());
  return absl::OkStatus();
}

absl::Status EditorManager::SaveRom() {
  if (!current_rom_ || !current_editor_set_) {
    return absl::FailedPreconditionError("No ROM or editor set loaded");
  }

  if (core::FeatureFlags::get().kSaveDungeonMaps) {
    RETURN_IF_ERROR(current_editor_set_->screen_editor_.SaveDungeonMaps());
  }

  RETURN_IF_ERROR(current_editor_set_->overworld_editor_.Save());

  if (core::FeatureFlags::get().kSaveGraphicsSheet)
    RETURN_IF_ERROR(SaveAllGraphicsData(
        *current_rom_, GraphicsSheetManager::GetInstance().gfx_sheets()));

  return current_rom_->SaveToFile(backup_rom_, save_new_auto_);
}

absl::Status EditorManager::OpenRomOrProject(const std::string &filename) {
  if (absl::StrContains(filename, ".yaze")) {
    RETURN_IF_ERROR(current_project_.Open(filename));
    RETURN_IF_ERROR(OpenProject());
  } else {
    auto new_rom = std::make_unique<Rom>();
    RETURN_IF_ERROR(new_rom->LoadFromFile(filename));
    current_rom_ = new_rom.get();
    SharedRom::set_rom(current_rom_);
    roms_.push_back(std::move(new_rom));

    // Create new editor set for this ROM
    auto editor_set = std::make_unique<EditorSet>(current_rom_);
    for (auto *editor : editor_set->active_editors_) {
      editor->set_context(&context_);
    }
    current_editor_set_ = editor_set.get();
    editor_sets_[current_rom_] = std::move(editor_set);
    RETURN_IF_ERROR(LoadAssets());
  }
  return absl::OkStatus();
}

absl::Status EditorManager::OpenProject() {
  auto new_rom = std::make_unique<Rom>();
  RETURN_IF_ERROR(new_rom->LoadFromFile(current_project_.rom_filename_));
  current_rom_ = new_rom.get();
  roms_.push_back(std::move(new_rom));

  if (!current_rom_->resource_label()->LoadLabels(
          current_project_.labels_filename_)) {
    return absl::InternalError(
        "Could not load labels file, update your project file.");
  }

  // Create new editor set for this ROM
  auto editor_set = std::make_unique<EditorSet>(current_rom_);
  for (auto *editor : editor_set->active_editors_) {
    editor->set_context(&context_);
  }
  current_editor_set_ = editor_set.get();
  editor_sets_[current_rom_] = std::move(editor_set);

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

  auto it = editor_sets_.find(rom);
  if (it == editor_sets_.end()) {
    // Create new editor set if it doesn't exist
    auto editor_set = std::make_unique<EditorSet>(rom);
    for (auto *editor : editor_set->active_editors_) {
      editor->set_context(&context_);
    }
    current_editor_set_ = editor_set.get();

    editor_sets_[rom] = std::move(editor_set);
    current_rom_ = rom;
    RETURN_IF_ERROR(LoadAssets());
  } else {
    current_editor_set_ = it->second.get();
    current_rom_ = rom;
    SharedRom::set_rom(rom);
  }

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze

#include "popup_manager.h"

#include "app/editor/editor_manager.h"
#include "app/gui/style.h"
#include "app/gui/icons.h"
#include "util/hex.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {

using namespace ImGui;

PopupManager::PopupManager(EditorManager* editor_manager)
    : editor_manager_(editor_manager), status_(absl::OkStatus()) {}

void PopupManager::Initialize() {
  // Register all popups
  popups_["About"] = {"About", false, [this]() { DrawAboutPopup(); }};
  popups_["ROM Information"] = {"ROM Information", false, [this]() { DrawRomInfoPopup(); }};
  popups_["Save As.."] = {"Save As..", false, [this]() { DrawSaveAsPopup(); }};
  popups_["New Project"] = {"New Project", false, [this]() { DrawNewProjectPopup(); }};
  popups_["Supported Features"] = {"Supported Features", false, [this]() { DrawSupportedFeaturesPopup(); }};
  popups_["Open a ROM"] = {"Open a ROM", false, [this]() { DrawOpenRomHelpPopup(); }};
  popups_["Manage Project"] = {"Manage Project", false, [this]() { DrawManageProjectPopup(); }};
}

void PopupManager::DrawPopups() {
  // Draw status popup if needed
  DrawStatusPopup();
  
  // Draw all registered popups
  for (auto& [name, params] : popups_) {
    if (params.is_visible) {
      OpenPopup(name.c_str());
      if (BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        params.draw_function();
        EndPopup();
      }
    }
  }
}

void PopupManager::Show(const char* name) {
  auto it = popups_.find(name);
  if (it != popups_.end()) {
    it->second.is_visible = true;
  }
}

void PopupManager::Hide(const char* name) {
  auto it = popups_.find(name);
  if (it != popups_.end()) {
    it->second.is_visible = false;
    CloseCurrentPopup();
  }
}

bool PopupManager::IsVisible(const char* name) const {
  auto it = popups_.find(name);
  if (it != popups_.end()) {
    return it->second.is_visible;
  }
  return false;
}

void PopupManager::SetStatus(const absl::Status& status) {
  if (!status.ok()) {
    show_status_ = true;
    prev_status_ = status;
    status_ = status;
  }
}

bool PopupManager::BeginCentered(const char* name) {
  ImGuiIO const& io = GetIO();
  ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  return Begin(name, nullptr, flags);
}

void PopupManager::DrawStatusPopup() {
  if (show_status_ && BeginCentered("StatusWindow")) {
    Text("%s", ICON_MD_ERROR);
    Text("%s", prev_status_.ToString().c_str());
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
      SetClipboardText(prev_status_.ToString().c_str());
    }
    End();
  }
}

void PopupManager::DrawAboutPopup() {
  Text("Yet Another Zelda3 Editor - v%s", editor_manager_->version().c_str());
  Text("Written by: scawful");
  Spacing();
  Text("Special Thanks: Zarby89, JaredBrian");
  Separator();

  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("About");
  }
}

void PopupManager::DrawRomInfoPopup() {
  auto* current_rom = editor_manager_->GetCurrentRom();
  if (!current_rom) return;
  
  Text("Title: %s", current_rom->title().c_str());
  Text("ROM Size: %s", util::HexLongLong(current_rom->size()).c_str());

  if (Button("Close", gui::kDefaultModalSize) || IsKeyPressed(ImGuiKey_Escape)) {
    Hide("ROM Information");
  }
}

void PopupManager::DrawSaveAsPopup() {
  static std::string save_as_filename = "";
  InputText("Filename", &save_as_filename);
  if (Button("Save", gui::kDefaultModalSize)) {
    // Call the save function from editor manager
    // This will need to be implemented in the editor manager
    Hide("Save As..");
  }
  SameLine();
  if (Button("Cancel", gui::kDefaultModalSize)) {
    Hide("Save As..");
  }
}

void PopupManager::DrawNewProjectPopup() {
  static std::string save_as_filename = "";
  InputText("Project Name", &save_as_filename);
  
  // These would need to be implemented in the editor manager
  if (Button("Destination Filepath", gui::kDefaultModalSize)) {
    // Call file dialog
  }
  SameLine();
  Text("%s", "filepath"); // This would be from the editor manager
  
  if (Button("ROM File", gui::kDefaultModalSize)) {
    // Call file dialog
  }
  SameLine();
  Text("%s", "rom_filename"); // This would be from the editor manager
  
  if (Button("Labels File", gui::kDefaultModalSize)) {
    // Call file dialog
  }
  SameLine();
  Text("%s", "labels_filename"); // This would be from the editor manager
  
  if (Button("Code Folder", gui::kDefaultModalSize)) {
    // Call file dialog
  }
  SameLine();
  Text("%s", "code_folder"); // This would be from the editor manager

  Separator();
  if (Button("Create", gui::kDefaultModalSize)) {
    // Create project
    Hide("New Project");
  }
  SameLine();
  if (Button("Cancel", gui::kDefaultModalSize)) {
    Hide("New Project");
  }
}

void PopupManager::DrawSupportedFeaturesPopup() {
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
    Hide("Supported Features");
  }
}

void PopupManager::DrawOpenRomHelpPopup() {
  Text("File -> Open");
  Text("Select a ROM file to open");
  Text("Supported ROMs (headered or unheadered):");
  Text("The Legend of Zelda: A Link to the Past");
  Text("US Version 1.0");
  Text("JP Version 1.0");

  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Open a ROM");
  }
}

void PopupManager::DrawManageProjectPopup() {
  Text("Project Menu");
  Text("Create a new project or open an existing one.");
  Text("Save the project to save the current state of the project.");
  TextWrapped(
      "To save a project, you need to first open a ROM and initialize your "
      "code path and labels file. Label resource manager can be found in "
      "the View menu. Code path is set in the Code editor after opening a "
      "folder.");

  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Manage Project");
  }
}

}  // namespace editor
}  // namespace yaze

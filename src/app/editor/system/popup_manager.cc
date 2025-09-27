#include "popup_manager.h"

#include "absl/strings/str_format.h"
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
  
  // v0.3 Help Documentation popups
  popups_["Getting Started"] = {"Getting Started", false, [this]() { DrawGettingStartedPopup(); }};
  popups_["Asar Integration"] = {"Asar Integration", false, [this]() { DrawAsarIntegrationPopup(); }};
  popups_["Build Instructions"] = {"Build Instructions", false, [this]() { DrawBuildInstructionsPopup(); }};
  popups_["CLI Usage"] = {"CLI Usage", false, [this]() { DrawCLIUsagePopup(); }};
  popups_["Troubleshooting"] = {"Troubleshooting", false, [this]() { DrawTroubleshootingPopup(); }};
  popups_["Contributing"] = {"Contributing", false, [this]() { DrawContributingPopup(); }};
  popups_["Whats New v03"] = {"What's New in v0.3", false, [this]() { DrawWhatsNewPopup(); }};
  
  // Workspace-related popups
  popups_["Workspace Help"] = {"Workspace Help", false, [this]() { DrawWorkspaceHelpPopup(); }};
  popups_["Session Limit Warning"] = {"Session Limit Warning", false, [this]() { DrawSessionLimitWarningPopup(); }};
  popups_["Layout Reset Confirm"] = {"Reset Layout Confirmation", false, [this]() { DrawLayoutResetConfirmPopup(); }};
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
  if (CollapsingHeader(absl::StrFormat("%s Overworld Editor", ICON_MD_LAYERS).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("LW/DW/SW Tilemap Editing");
    BulletText("LW/DW/SW Map Properties");
    BulletText("Create/Delete/Update Entrances");
    BulletText("Create/Delete/Update Exits");
    BulletText("Create/Delete/Update Sprites");
    BulletText("Create/Delete/Update Items");
    BulletText("Multi-session map editing support");
  }

  if (CollapsingHeader(absl::StrFormat("%s Dungeon Editor", ICON_MD_CASTLE).c_str())) {
    BulletText("View Room Header Properties");
    BulletText("View Entrance Properties");
    BulletText("Enhanced room navigation");
  }

  if (CollapsingHeader(absl::StrFormat("%s Graphics & Themes", ICON_MD_PALETTE).c_str())) {
    BulletText("View Decompressed Graphics Sheets");
    BulletText("View/Update Graphics Groups");
    BulletText("5+ Built-in themes (Classic, Cyberpunk, Sunset, Forest, Midnight)");
    BulletText("Custom theme creation and editing");
    BulletText("Theme import/export functionality");
    BulletText("Animated background grid effects");
  }

  if (CollapsingHeader(absl::StrFormat("%s Palettes", ICON_MD_COLOR_LENS).c_str())) {
    BulletText("View Palette Groups");
    BulletText("Enhanced palette editing tools");
    BulletText("Color conversion utilities");
  }
  
  if (CollapsingHeader(absl::StrFormat("%s Project Management", ICON_MD_FOLDER).c_str())) {
    BulletText("Multi-session workspace support");
    BulletText("Enhanced project creation and management");
    BulletText("ZScream project format compatibility");
    BulletText("Workspace settings and feature flags");
  }
  
  if (CollapsingHeader(absl::StrFormat("%s Development Tools", ICON_MD_BUILD).c_str())) {
    BulletText("Asar 65816 assembler integration");
    BulletText("Enhanced CLI tools with TUI interface");
    BulletText("Memory editor with advanced features");
    BulletText("Hex editor with search and navigation");
    BulletText("Assembly validation and symbol extraction");
  }

  if (CollapsingHeader(absl::StrFormat("%s Save Capabilities", ICON_MD_SAVE).c_str())) {
    BulletText("All Overworld editing features");
    BulletText("Hex Editor changes");
    BulletText("Theme configurations");
    BulletText("Project settings and workspace layouts");
    BulletText("Custom assembly patches");
  }

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

void PopupManager::DrawGettingStartedPopup() {
  TextWrapped("Welcome to YAZE v0.3!");
  TextWrapped("This software allows you to modify 'The Legend of Zelda: A Link to the Past' (US or JP) ROMs.");
  Spacing();
  TextWrapped("General Tips:");
  BulletText("Experiment flags determine whether certain features are enabled");
  BulletText("Backup files are enabled by default for safety");
  BulletText("Use File > Options to configure settings");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Getting Started");
  }
}

void PopupManager::DrawAsarIntegrationPopup() {
  TextWrapped("Asar 65816 Assembly Integration");
  TextWrapped("YAZE v0.3 includes full Asar assembler support for ROM patching.");
  Spacing();
  TextWrapped("Features:");
  BulletText("Cross-platform ROM patching with assembly code");
  BulletText("Symbol extraction with addresses and opcodes");
  BulletText("Assembly validation with error reporting");
  BulletText("Memory-safe operations with automatic ROM size management");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Asar Integration");
  }
}

void PopupManager::DrawBuildInstructionsPopup() {
  TextWrapped("Build Instructions");
  TextWrapped("YAZE uses modern CMake for cross-platform builds.");
  Spacing();
  TextWrapped("Quick Start:");
  BulletText("cmake -B build");
  BulletText("cmake --build build --target yaze");
  Spacing();
  TextWrapped("Development:");
  BulletText("cmake --preset dev");
  BulletText("cmake --build --preset dev");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Build Instructions");
  }
}

void PopupManager::DrawCLIUsagePopup() {
  TextWrapped("Command Line Interface (z3ed)");
  TextWrapped("Enhanced CLI tool with Asar integration.");
  Spacing();
  TextWrapped("Commands:");
  BulletText("z3ed asar patch.asm --rom=file.sfc");
  BulletText("z3ed extract symbols.asm");
  BulletText("z3ed validate assembly.asm");
  BulletText("z3ed patch file.bps --rom=file.sfc");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("CLI Usage");
  }
}

void PopupManager::DrawTroubleshootingPopup() {
  TextWrapped("Troubleshooting");
  TextWrapped("Common issues and solutions:");
  Spacing();
  BulletText("ROM won't load: Check file format (SFC/SMC supported)");
  BulletText("Graphics issues: Try disabling experimental features");
  BulletText("Performance: Enable hardware acceleration in display settings");
  BulletText("Crashes: Check ROM file integrity and available memory");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Troubleshooting");
  }
}

void PopupManager::DrawContributingPopup() {
  TextWrapped("Contributing to YAZE");
  TextWrapped("YAZE is open source and welcomes contributions!");
  Spacing();
  TextWrapped("How to contribute:");
  BulletText("Fork the repository on GitHub");
  BulletText("Create feature branches for new work");
  BulletText("Follow C++ coding standards");
  BulletText("Include tests for new features");
  BulletText("Submit pull requests for review");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Contributing");
  }
}

void PopupManager::DrawWhatsNewPopup() {
  TextWrapped("What's New in YAZE v0.3");
  Spacing();
  
  if (CollapsingHeader(absl::StrFormat("%s User Interface & Theming", ICON_MD_PALETTE).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Complete theme management system with 5+ built-in themes");
    BulletText("Custom theme editor with save-to-file functionality");
    BulletText("Animated background grid with breathing effects (optional)");
    BulletText("Enhanced welcome screen with themed elements");
    BulletText("Multi-session workspace support with docking");
    BulletText("Improved editor organization and navigation");
  }
  
  if (CollapsingHeader(absl::StrFormat("%s Development & Build System", ICON_MD_BUILD).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Asar 65816 assembler integration for ROM patching");
    BulletText("Enhanced CLI tools with TUI (Terminal User Interface)");
    BulletText("Modernized CMake build system with presets");
    BulletText("Cross-platform CI/CD pipeline (Windows, macOS, Linux)");
    BulletText("Comprehensive testing framework with 46+ core tests");
    BulletText("Professional packaging for all platforms (DMG, MSI, DEB)");
  }
  
  if (CollapsingHeader(absl::StrFormat("%s Core Improvements", ICON_MD_SETTINGS).c_str())) {
    BulletText("Enhanced project management with YazeProject structure");
    BulletText("Improved ROM loading and validation");
    BulletText("Better error handling and status reporting");
    BulletText("Memory safety improvements with sanitizers");
    BulletText("Enhanced file dialog integration");
    BulletText("Improved logging and debugging capabilities");
  }
  
  if (CollapsingHeader(absl::StrFormat("%s Editor Features", ICON_MD_EDIT).c_str())) {
    BulletText("Enhanced overworld editing capabilities");
    BulletText("Improved graphics sheet viewing and editing");
    BulletText("Better palette management and editing");
    BulletText("Enhanced memory and hex editing tools");
    BulletText("Improved sprite and item management");
    BulletText("Better entrance and exit editing");
  }
  
  Spacing();
  if (Button(absl::StrFormat("%s View Theme Editor", ICON_MD_PALETTE).c_str(), ImVec2(-1, 30))) {
    // Close this popup and show theme settings
    Hide("Whats New v03");
    // Could trigger theme editor opening here
  }
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Whats New v03");
  }
}

void PopupManager::DrawWorkspaceHelpPopup() {
  TextWrapped("Workspace Management");
  TextWrapped("YAZE supports multiple ROM sessions and flexible workspace layouts.");
  Spacing();
  
  TextWrapped("Session Management:");
  BulletText("Ctrl+Shift+N: Create new session");
  BulletText("Ctrl+Shift+W: Close current session");
  BulletText("Ctrl+Tab: Quick session switcher");
  BulletText("Each session maintains its own ROM and editor state");
  
  Spacing();
  TextWrapped("Layout Management:");
  BulletText("Drag window tabs to dock/undock");
  BulletText("Ctrl+Shift+S: Save current layout");
  BulletText("Ctrl+Shift+O: Load saved layout");
  BulletText("F11: Maximize current window");
  
  Spacing();
  TextWrapped("Preset Layouts:");
  BulletText("Developer: Code, memory, testing tools");
  BulletText("Designer: Graphics, palettes, sprites");
  BulletText("Modder: All gameplay editing tools");
  
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Workspace Help");
  }
}

void PopupManager::DrawSessionLimitWarningPopup() {
  TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Warning", ICON_MD_WARNING);
  TextWrapped("You have reached the recommended session limit.");
  TextWrapped("Having too many sessions open may impact performance.");
  Spacing();
  TextWrapped("Consider closing unused sessions or saving your work.");
  
  if (Button("Understood", gui::kDefaultModalSize)) {
    Hide("Session Limit Warning");
  }
  SameLine();
  if (Button("Open Session Manager", gui::kDefaultModalSize)) {
    Hide("Session Limit Warning");
    // This would trigger the session manager to open
  }
}

void PopupManager::DrawLayoutResetConfirmPopup() {
  TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Confirm Reset", ICON_MD_WARNING);
  TextWrapped("This will reset your current workspace layout to default.");
  TextWrapped("Any custom window arrangements will be lost.");
  Spacing();
  TextWrapped("Do you want to continue?");
  
  if (Button("Reset Layout", gui::kDefaultModalSize)) {
    Hide("Layout Reset Confirm");
    // This would trigger the actual reset
  }
  SameLine();
  if (Button("Cancel", gui::kDefaultModalSize)) {
    Hide("Layout Reset Confirm");
  }
}

}  // namespace editor
}  // namespace yaze

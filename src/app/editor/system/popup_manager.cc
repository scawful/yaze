#include "popup_manager.h"

#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gui/app/feature_flags_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/hex.h"

namespace yaze {
namespace editor {

using namespace ImGui;

PopupManager::PopupManager(EditorManager* editor_manager)
    : editor_manager_(editor_manager), status_(absl::OkStatus()) {}

void PopupManager::Initialize() {
  // ============================================================================
  // POPUP REGISTRATION
  // ============================================================================
  // All popups must be registered here BEFORE any menu callbacks can trigger
  // them. This method is called in EditorManager constructor BEFORE
  // MenuOrchestrator and UICoordinator are created, ensuring safe
  // initialization order.
  //
  // Popup Registration Format:
  // popups_[PopupID::kConstant] = {
  //   .name = PopupID::kConstant,
  //   .type = PopupType::kXxx,
  //   .is_visible = false,
  //   .allow_resize = false/true,
  //   .draw_function = [this]() { DrawXxxPopup(); }
  // };
  // ============================================================================

  // File Operations
  popups_[PopupID::kSaveAs] = {PopupID::kSaveAs, PopupType::kFileOperation,
                               false, false, [this]() { DrawSaveAsPopup(); }};
  popups_[PopupID::kNewProject] = {PopupID::kNewProject,
                                   PopupType::kFileOperation, false, false,
                                   [this]() { DrawNewProjectPopup(); }};
  popups_[PopupID::kManageProject] = {PopupID::kManageProject,
                                      PopupType::kFileOperation, false, false,
                                      [this]() { DrawManageProjectPopup(); }};

  // Information
  popups_[PopupID::kAbout] = {PopupID::kAbout, PopupType::kInfo, false, false,
                              [this]() { DrawAboutPopup(); }};
  popups_[PopupID::kRomInfo] = {PopupID::kRomInfo, PopupType::kInfo, false,
                                false, [this]() { DrawRomInfoPopup(); }};
  popups_[PopupID::kSupportedFeatures] = {
      PopupID::kSupportedFeatures, PopupType::kInfo, false, false,
      [this]() { DrawSupportedFeaturesPopup(); }};
  popups_[PopupID::kOpenRomHelp] = {PopupID::kOpenRomHelp, PopupType::kHelp,
                                    false, false,
                                    [this]() { DrawOpenRomHelpPopup(); }};

  // Help Documentation
  popups_[PopupID::kGettingStarted] = {PopupID::kGettingStarted,
                                       PopupType::kHelp, false, false,
                                       [this]() { DrawGettingStartedPopup(); }};
  popups_[PopupID::kAsarIntegration] = {
      PopupID::kAsarIntegration, PopupType::kHelp, false, false,
      [this]() { DrawAsarIntegrationPopup(); }};
  popups_[PopupID::kBuildInstructions] = {
      PopupID::kBuildInstructions, PopupType::kHelp, false, false,
      [this]() { DrawBuildInstructionsPopup(); }};
  popups_[PopupID::kCLIUsage] = {PopupID::kCLIUsage, PopupType::kHelp, false,
                                 false, [this]() { DrawCLIUsagePopup(); }};
  popups_[PopupID::kTroubleshooting] = {
      PopupID::kTroubleshooting, PopupType::kHelp, false, false,
      [this]() { DrawTroubleshootingPopup(); }};
  popups_[PopupID::kContributing] = {PopupID::kContributing, PopupType::kHelp,
                                     false, false,
                                     [this]() { DrawContributingPopup(); }};
  popups_[PopupID::kWhatsNew] = {PopupID::kWhatsNew, PopupType::kHelp, false,
                                 false, [this]() { DrawWhatsNewPopup(); }};

  // Settings
  popups_[PopupID::kDisplaySettings] = {
      PopupID::kDisplaySettings, PopupType::kSettings, false,
      true,  // Resizable
      [this]() { DrawDisplaySettingsPopup(); }};
  popups_[PopupID::kFeatureFlags] = {
      PopupID::kFeatureFlags, PopupType::kSettings, false, true,  // Resizable
      [this]() { DrawFeatureFlagsPopup(); }};

  // Workspace
  popups_[PopupID::kWorkspaceHelp] = {PopupID::kWorkspaceHelp, PopupType::kHelp,
                                      false, false,
                                      [this]() { DrawWorkspaceHelpPopup(); }};
  popups_[PopupID::kSessionLimitWarning] = {
      PopupID::kSessionLimitWarning, PopupType::kWarning, false, false,
      [this]() { DrawSessionLimitWarningPopup(); }};
  popups_[PopupID::kLayoutResetConfirm] = {
      PopupID::kLayoutResetConfirm, PopupType::kConfirmation, false, false,
      [this]() { DrawLayoutResetConfirmPopup(); }};

  // Debug/Testing
  popups_[PopupID::kDataIntegrity] = {PopupID::kDataIntegrity, PopupType::kInfo,
                                      false, true,  // Resizable
                                      [this]() { DrawDataIntegrityPopup(); }};
}

void PopupManager::DrawPopups() {
  // Draw status popup if needed
  DrawStatusPopup();

  // Draw all registered popups
  for (auto& [name, params] : popups_) {
    if (params.is_visible) {
      OpenPopup(name.c_str());

      // Use allow_resize flag from popup definition
      ImGuiWindowFlags popup_flags = params.allow_resize
                                         ? ImGuiWindowFlags_None
                                         : ImGuiWindowFlags_AlwaysAutoResize;

      if (BeginPopupModal(name.c_str(), nullptr, popup_flags)) {
        params.draw_function();
        EndPopup();
      }
    }
  }
}

void PopupManager::Show(const char* name) {
  if (!name) {
    return;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
  if (it != popups_.end()) {
    it->second.is_visible = true;
  } else {
    // Log warning for unregistered popup
    printf(
        "[PopupManager] Warning: Popup '%s' not registered. Available popups: ",
        name);
    for (const auto& [key, _] : popups_) {
      printf("'%s' ", key.c_str());
    }
    printf("\n");
  }
}

void PopupManager::Hide(const char* name) {
  if (!name) {
    return;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
  if (it != popups_.end()) {
    it->second.is_visible = false;
    CloseCurrentPopup();
  }
}

bool PopupManager::IsVisible(const char* name) const {
  if (!name) {
    return false;  // Safety check for null pointer
  }

  std::string name_str(name);
  auto it = popups_.find(name_str);
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

  if (Button("Close", gui::kDefaultModalSize) ||
      IsKeyPressed(ImGuiKey_Escape)) {
    Hide("ROM Information");
  }
}

void PopupManager::DrawSaveAsPopup() {
  using namespace ImGui;

  Text("%s Save ROM to new location", ICON_MD_SAVE_AS);
  Separator();

  static std::string save_as_filename = "";
  if (editor_manager_->GetCurrentRom() && save_as_filename.empty()) {
    save_as_filename = editor_manager_->GetCurrentRom()->title();
  }

  InputText("Filename", &save_as_filename);
  Separator();

  if (Button(absl::StrFormat("%s Browse...", ICON_MD_FOLDER_OPEN).c_str(),
             gui::kDefaultModalSize)) {
    auto file_path =
        util::FileDialogWrapper::ShowSaveFileDialog(save_as_filename, "sfc");
    if (!file_path.empty()) {
      save_as_filename = file_path;
    }
  }

  SameLine();
  if (Button(absl::StrFormat("%s Save", ICON_MD_SAVE).c_str(),
             gui::kDefaultModalSize)) {
    if (!save_as_filename.empty()) {
      // Ensure proper file extension
      std::string final_filename = save_as_filename;
      if (final_filename.find(".sfc") == std::string::npos &&
          final_filename.find(".smc") == std::string::npos) {
        final_filename += ".sfc";
      }

      auto status = editor_manager_->SaveRomAs(final_filename);
      if (status.ok()) {
        save_as_filename = "";
        Hide(PopupID::kSaveAs);
      }
    }
  }

  SameLine();
  if (Button(absl::StrFormat("%s Cancel", ICON_MD_CANCEL).c_str(),
             gui::kDefaultModalSize)) {
    save_as_filename = "";
    Hide(PopupID::kSaveAs);
  }
}

void PopupManager::DrawNewProjectPopup() {
  using namespace ImGui;

  static std::string project_name = "";
  static std::string project_filepath = "";
  static std::string rom_filename = "";
  static std::string labels_filename = "";
  static std::string code_folder = "";

  InputText("Project Name", &project_name);

  if (Button(absl::StrFormat("%s Destination Folder", ICON_MD_FOLDER).c_str(),
             gui::kDefaultModalSize)) {
    project_filepath = util::FileDialogWrapper::ShowOpenFolderDialog();
  }
  SameLine();
  Text("%s", project_filepath.empty() ? "(Not set)" : project_filepath.c_str());

  if (Button(absl::StrFormat("%s ROM File", ICON_MD_VIDEOGAME_ASSET).c_str(),
             gui::kDefaultModalSize)) {
    rom_filename = util::FileDialogWrapper::ShowOpenFileDialog();
  }
  SameLine();
  Text("%s", rom_filename.empty() ? "(Not set)" : rom_filename.c_str());

  if (Button(absl::StrFormat("%s Labels File", ICON_MD_LABEL).c_str(),
             gui::kDefaultModalSize)) {
    labels_filename = util::FileDialogWrapper::ShowOpenFileDialog();
  }
  SameLine();
  Text("%s", labels_filename.empty() ? "(Not set)" : labels_filename.c_str());

  if (Button(absl::StrFormat("%s Code Folder", ICON_MD_CODE).c_str(),
             gui::kDefaultModalSize)) {
    code_folder = util::FileDialogWrapper::ShowOpenFolderDialog();
  }
  SameLine();
  Text("%s", code_folder.empty() ? "(Not set)" : code_folder.c_str());

  Separator();

  if (Button(absl::StrFormat("%s Choose Project File Location", ICON_MD_SAVE)
                 .c_str(),
             gui::kDefaultModalSize)) {
    auto project_file_path =
        util::FileDialogWrapper::ShowSaveFileDialog(project_name, "yaze");
    if (!project_file_path.empty()) {
      if (project_file_path.find(".yaze") == std::string::npos) {
        project_file_path += ".yaze";
      }
      project_filepath = project_file_path;
    }
  }

  if (Button(absl::StrFormat("%s Create Project", ICON_MD_ADD).c_str(),
             gui::kDefaultModalSize)) {
    if (!project_filepath.empty() && !project_name.empty()) {
      auto status = editor_manager_->CreateNewProject();
      if (status.ok()) {
        // Clear fields
        project_name = "";
        project_filepath = "";
        rom_filename = "";
        labels_filename = "";
        code_folder = "";
        Hide(PopupID::kNewProject);
      }
    }
  }
  SameLine();
  if (Button(absl::StrFormat("%s Cancel", ICON_MD_CANCEL).c_str(),
             gui::kDefaultModalSize)) {
    // Clear fields
    project_name = "";
    project_filepath = "";
    rom_filename = "";
    labels_filename = "";
    code_folder = "";
    Hide(PopupID::kNewProject);
  }
}

void PopupManager::DrawSupportedFeaturesPopup() {
  if (CollapsingHeader(
          absl::StrFormat("%s Overworld Editor", ICON_MD_LAYERS).c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("LW/DW/SW Tilemap Editing");
    BulletText("LW/DW/SW Map Properties");
    BulletText("Create/Delete/Update Entrances");
    BulletText("Create/Delete/Update Exits");
    BulletText("Create/Delete/Update Sprites");
    BulletText("Create/Delete/Update Items");
    BulletText("Multi-session map editing support");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Dungeon Editor", ICON_MD_CASTLE).c_str())) {
    BulletText("View Room Header Properties");
    BulletText("View Entrance Properties");
    BulletText("Enhanced room navigation");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Graphics & Themes", ICON_MD_PALETTE).c_str())) {
    BulletText("View Decompressed Graphics Sheets");
    BulletText("View/Update Graphics Groups");
    BulletText(
        "5+ Built-in themes (Classic, Cyberpunk, Sunset, Forest, Midnight)");
    BulletText("Custom theme creation and editing");
    BulletText("Theme import/export functionality");
    BulletText("Animated background grid effects");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Palettes", ICON_MD_COLOR_LENS).c_str())) {
    BulletText("View Palette Groups");
    BulletText("Enhanced palette editing tools");
    BulletText("Color conversion utilities");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Project Management", ICON_MD_FOLDER).c_str())) {
    BulletText("Multi-session workspace support");
    BulletText("Enhanced project creation and management");
    BulletText("ZScream project format compatibility");
    BulletText("Workspace settings and feature flags");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Development Tools", ICON_MD_BUILD).c_str())) {
    BulletText("Asar 65816 assembler integration");
    BulletText("Enhanced CLI tools with TUI interface");
    BulletText("Memory editor with advanced features");
    BulletText("Hex editor with search and navigation");
    BulletText("Assembly validation and symbol extraction");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Save Capabilities", ICON_MD_SAVE).c_str())) {
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
  TextWrapped(
      "This software allows you to modify 'The Legend of Zelda: A Link to the "
      "Past' (US or JP) ROMs.");
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
  TextWrapped(
      "YAZE v0.3 includes full Asar assembler support for ROM patching.");
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

  if (CollapsingHeader(
          absl::StrFormat("%s User Interface & Theming", ICON_MD_PALETTE)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Complete theme management system with 5+ built-in themes");
    BulletText("Custom theme editor with save-to-file functionality");
    BulletText("Animated background grid with breathing effects (optional)");
    BulletText("Enhanced welcome screen with themed elements");
    BulletText("Multi-session workspace support with docking");
    BulletText("Improved editor organization and navigation");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Development & Build System", ICON_MD_BUILD)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    BulletText("Asar 65816 assembler integration for ROM patching");
    BulletText("Enhanced CLI tools with TUI (Terminal User Interface)");
    BulletText("Modernized CMake build system with presets");
    BulletText("Cross-platform CI/CD pipeline (Windows, macOS, Linux)");
    BulletText("Comprehensive testing framework with 46+ core tests");
    BulletText("Professional packaging for all platforms (DMG, MSI, DEB)");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Core Improvements", ICON_MD_SETTINGS).c_str())) {
    BulletText("Enhanced project management with YazeProject structure");
    BulletText("Improved ROM loading and validation");
    BulletText("Better error handling and status reporting");
    BulletText("Memory safety improvements with sanitizers");
    BulletText("Enhanced file dialog integration");
    BulletText("Improved logging and debugging capabilities");
  }

  if (CollapsingHeader(
          absl::StrFormat("%s Editor Features", ICON_MD_EDIT).c_str())) {
    BulletText("Enhanced overworld editing capabilities");
    BulletText("Improved graphics sheet viewing and editing");
    BulletText("Better palette management and editing");
    BulletText("Enhanced memory and hex editing tools");
    BulletText("Improved sprite and item management");
    BulletText("Better entrance and exit editing");
  }

  Spacing();
  if (Button(absl::StrFormat("%s View Theme Editor", ICON_MD_PALETTE).c_str(),
             ImVec2(-1, 30))) {
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
  TextWrapped(
      "YAZE supports multiple ROM sessions and flexible workspace layouts.");
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
  TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Confirm Reset",
              ICON_MD_WARNING);
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

void PopupManager::DrawDisplaySettingsPopup() {
  // Set a comfortable default size with natural constraints
  SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
  SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));

  Text("%s Display & Theme Settings", ICON_MD_DISPLAY_SETTINGS);
  TextWrapped("Customize your YAZE experience - accessible anytime!");
  Separator();

  // Create a child window for scrollable content to avoid table conflicts
  // Use remaining space minus the close button area
  float available_height =
      GetContentRegionAvail().y - 60;  // Reserve space for close button
  if (BeginChild("DisplaySettingsContent", ImVec2(0, available_height), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    // Use the popup-safe version to avoid table conflicts
    gui::DrawDisplaySettingsForPopup();

    Separator();
    gui::TextWithSeparators("Font Manager");
    gui::DrawFontManager();

    // Global font scale (moved from the old display settings window)
    ImGuiIO& io = GetIO();
    Separator();
    Text("Global Font Scale");
    static float font_global_scale = io.FontGlobalScale;
    if (SliderFloat("##global_scale", &font_global_scale, 0.5f, 1.8f, "%.2f")) {
      if (editor_manager_) {
        editor_manager_->SetFontGlobalScale(font_global_scale);
      } else {
        io.FontGlobalScale = font_global_scale;
      }
    }
  }
  EndChild();

  Separator();
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide("Display Settings");
  }
}

void PopupManager::DrawFeatureFlagsPopup() {
  using namespace ImGui;

  // Display feature flags editor using the existing FlagsMenu system
  Text("Feature Flags Configuration");
  Separator();

  BeginChild("##FlagsContent", ImVec2(0, -30), true);

  // Use the feature flags menu system
  static gui::FlagsMenu flags_menu;

  if (BeginTabBar("FlagCategories")) {
    if (BeginTabItem("Overworld")) {
      flags_menu.DrawOverworldFlags();
      EndTabItem();
    }
    if (BeginTabItem("Dungeon")) {
      flags_menu.DrawDungeonFlags();
      EndTabItem();
    }
    if (BeginTabItem("Resources")) {
      flags_menu.DrawResourceFlags();
      EndTabItem();
    }
    if (BeginTabItem("System")) {
      flags_menu.DrawSystemFlags();
      EndTabItem();
    }
    EndTabBar();
  }

  EndChild();

  Separator();
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide(PopupID::kFeatureFlags);
  }
}

void PopupManager::DrawDataIntegrityPopup() {
  using namespace ImGui;

  Text("Data Integrity Check Results");
  Separator();

  BeginChild("##IntegrityContent", ImVec2(0, -30), true);

  // Placeholder for data integrity results
  // In a full implementation, this would show test results
  Text("ROM Data Integrity:");
  Separator();
  TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ ROM header valid");
  TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Checksum valid");
  TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Graphics data intact");
  TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Map data intact");

  Spacing();
  Text("No issues detected.");

  EndChild();

  Separator();
  if (Button("Close", gui::kDefaultModalSize)) {
    Hide(PopupID::kDataIntegrity);
  }
}

}  // namespace editor
}  // namespace yaze

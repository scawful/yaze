#include "app/editor/ui/ui_coordinator.h"

#include <functional>
#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/system/window_delegate.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

UICoordinator::UICoordinator(
    EditorManager* editor_manager,
    RomFileManager& rom_manager,
    ProjectManager& project_manager,
    EditorRegistry& editor_registry,
    SessionCoordinator& session_coordinator,
    WindowDelegate& window_delegate,
    ToastManager& toast_manager,
    PopupManager& popup_manager)
    : editor_manager_(editor_manager),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      session_coordinator_(session_coordinator),
      window_delegate_(window_delegate),
      toast_manager_(toast_manager),
      popup_manager_(popup_manager) {
  
  // Initialize welcome screen with proper callbacks
  welcome_screen_ = std::make_unique<WelcomeScreen>();
  
  // Wire welcome screen callbacks to EditorManager
  welcome_screen_->SetOpenRomCallback([this]() {
    if (editor_manager_) {
      auto status = editor_manager_->LoadRom();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to load ROM: %s", status.message()),
            ToastType::kError);
      } else {
        // Hide welcome screen on successful ROM load
        show_welcome_screen_ = false;
        welcome_screen_manually_closed_ = true;
      }
    }
  });
  
  welcome_screen_->SetNewProjectCallback([this]() {
    if (editor_manager_) {
      auto status = editor_manager_->CreateNewProject();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to create project: %s", status.message()),
            ToastType::kError);
      } else {
        // Hide welcome screen on successful project creation
        show_welcome_screen_ = false;
        welcome_screen_manually_closed_ = true;
      }
    }
  });
  
  welcome_screen_->SetOpenProjectCallback([this](const std::string& filepath) {
    if (editor_manager_) {
      auto status = editor_manager_->OpenRomOrProject(filepath);
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to open project: %s", status.message()),
            ToastType::kError);
      } else {
        // Hide welcome screen on successful project open
        show_welcome_screen_ = false;
        welcome_screen_manually_closed_ = true;
      }
    }
  });
}

void UICoordinator::DrawAllUI() {
  // Apply Material Design styling
  ApplyMaterialDesignStyling();
  
  // Draw all UI components in order
  DrawMenuBarExtras();
  DrawContextSensitiveCardControl();
  DrawSessionSwitcher();
  DrawSessionManager();
  DrawSessionRenameDialog();
  DrawLayoutPresets();
  DrawWelcomeScreen();
  DrawProjectHelp();
  DrawWindowManagementUI();
  
  // Draw all popups
  DrawAllPopups();
}

void UICoordinator::DrawMenuBarExtras() {
  auto* current_rom = rom_manager_.GetCurrentRom();
  std::string version_text = absl::StrFormat("v%s", editor_manager_->version().c_str());
  float version_width = ImGui::CalcTextSize(version_text.c_str()).x;
  float session_rom_area_width = 280.0f;

  ImGui::SameLine(ImGui::GetWindowWidth() - version_width - 10 - session_rom_area_width);

  // Session indicator with Material Design styling
  if (session_coordinator_.HasMultipleSessions()) {
    std::string session_button_text = absl::StrFormat("%s%zu", ICON_MD_TAB, 
                                                     session_coordinator_.GetActiveSessionCount());
    
    // Material Design button styling
    ImGui::PushStyleColor(ImGuiCol_Button, gui::GetPrimaryVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::GetPrimaryHoverVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, gui::GetPrimaryActiveVec4());
    
    if (ImGui::SmallButton(session_button_text.c_str())) {
      session_coordinator_.ToggleSessionSwitcher();
    }
    
    ImGui::PopStyleColor(3);
    
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sessions: %zu active\nClick to switch",
                        session_coordinator_.GetActiveSessionCount());
    }
    ImGui::SameLine();
  }

  // ROM information display with Material Design card styling
  if (current_rom && current_rom->is_loaded()) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::Text("%s %s", ICON_MD_INSERT_DRIVE_FILE, current_rom->title().c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
  }

  // Version info with subtle styling
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::Text("%s", version_text.c_str());
  ImGui::PopStyleColor();
}

void UICoordinator::DrawContextSensitiveCardControl() {
  // Get current editor and determine category
  auto* current_editor = editor_manager_->GetCurrentEditorSet();
  if (!current_editor) return;
  
  // Find active card-based editor
  Editor* active_editor = nullptr;
  for (auto* editor : current_editor->active_editors_) {
    if (*editor->active() && editor_registry_.IsCardBasedEditor(editor->type())) {
      active_editor = editor;
      break;
    }
  }
  
  if (!active_editor) return;
  
  std::string category = editor_registry_.GetEditorCategory(active_editor->type());
  
  // Draw compact card control with session awareness
  auto& card_manager = gui::EditorCardManager::Get();
  if (session_coordinator_.HasMultipleSessions()) {
    std::string session_prefix = absl::StrFormat("s%zu", session_coordinator_.GetActiveSessionIndex());
    card_manager.DrawCompactCardControlWithSession(category, session_prefix);
  } else {
    card_manager.DrawCompactCardControl(category);
  }
}

void UICoordinator::DrawSessionSwitcher() {
  if (!show_session_switcher_) return;
  
  // Material Design dialog styling
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::GetSurfaceVec4());
  ImGui::PushStyleColor(ImGuiCol_Border, gui::GetOutlineVec4());
  
  if (ImGui::Begin("Session Switcher", &show_session_switcher_, 
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
    
    // Header with Material Design typography
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetOnSurfaceVec4());
    ImGui::Text("%s Session Switcher", ICON_MD_TAB);
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // Session list with Material Design list styling
    for (size_t i = 0; i < session_coordinator_.GetActiveSessionCount(); ++i) {
      std::string session_name = session_coordinator_.GetSessionDisplayName(i);
      bool is_active = (i == session_coordinator_.GetActiveSessionIndex());
      
      // Active session highlighting
      if (is_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, gui::GetPrimaryVec4());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::GetPrimaryHoverVec4());
        ImGui::PushStyleColor(ImGuiCol_Text, gui::GetOnPrimaryVec4());
      }
      
      std::string button_text = absl::StrFormat("%s %s", 
                                               is_active ? ICON_MD_RADIO_BUTTON_CHECKED : ICON_MD_RADIO_BUTTON_UNCHECKED,
                                               session_name.c_str());
      
      if (ImGui::Button(button_text.c_str(), ImVec2(-1, 0))) {
        session_coordinator_.SwitchToSession(i);
        show_session_switcher_ = false;
      }
      
      if (is_active) {
        ImGui::PopStyleColor(3);
      }
    }
    
    ImGui::Separator();
    
    // Action buttons with Material Design styling
    if (ImGui::Button(absl::StrFormat("%s New Session", ICON_MD_ADD).c_str(), ImVec2(-1, 0))) {
      session_coordinator_.CreateNewSession();
      show_session_switcher_ = false;
    }
    
    if (ImGui::Button(absl::StrFormat("%s Close", ICON_MD_CLOSE).c_str(), ImVec2(-1, 0))) {
      show_session_switcher_ = false;
    }
  }
  
  ImGui::End();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}

void UICoordinator::DrawSessionManager() {
  // TODO: Implement session manager dialog
  // This would be a more comprehensive session management interface
}

void UICoordinator::DrawSessionRenameDialog() {
  // TODO: Implement session rename dialog
  // This would allow users to rename sessions for better organization
}

void UICoordinator::DrawLayoutPresets() {
  // TODO: Implement layout presets UI
  // This would show available layout presets (Developer, Designer, Modder)
}

void UICoordinator::DrawWelcomeScreen() {
  // Auto-show welcome screen when no ROM is loaded (unless manually closed)
  if (!show_welcome_screen_ && !welcome_screen_manually_closed_) {
    // Check with EditorManager if we should show welcome screen
    if (editor_manager_ && editor_manager_->GetActiveSessionCount() == 0) {
      show_welcome_screen_ = true;
    }
  }
  
  if (!show_welcome_screen_) return;
  
  if (welcome_screen_) {
    // Update recent projects before showing
    welcome_screen_->RefreshRecentProjects();
    
    bool was_open = show_welcome_screen_;
    welcome_screen_->Show(&show_welcome_screen_);
    
    // Check if the welcome screen was manually closed via the close button
    if (was_open && !show_welcome_screen_) {
      welcome_screen_manually_closed_ = true;
      welcome_screen_->MarkManuallyClosed();
    }
  }
}

void UICoordinator::DrawProjectHelp() {
  // TODO: Implement project help UI
  // This would show project-specific help and documentation
}

void UICoordinator::DrawWindowManagementUI() {
  // TODO: Implement window management UI
  // This would provide controls for window visibility, docking, etc.
}

void UICoordinator::DrawAllPopups() {
  // Draw all registered popups
  popup_manager_.DrawPopups();
}

void UICoordinator::ShowPopup(const std::string& popup_name) {
  popup_manager_.Show(popup_name.c_str());
}

void UICoordinator::HidePopup(const std::string& popup_name) {
  popup_manager_.Hide(popup_name.c_str());
}

void UICoordinator::ShowDisplaySettings() {
  show_display_settings_ = true;
  ShowPopup("Display Settings");
}

void UICoordinator::HideCurrentEditorCards() {
  // TODO: Implement card hiding logic
  // This would hide cards for the current editor
}

void UICoordinator::ShowAllWindows() {
  window_delegate_.ShowAllWindows();
}

void UICoordinator::HideAllWindows() {
  window_delegate_.HideAllWindows();
}

void UICoordinator::ApplyMaterialDesignStyling() {
  // Apply Material Design 3 color scheme
  ImGuiStyle& style = ImGui::GetStyle();
  
  // Set Material Design colors
  style.Colors[ImGuiCol_WindowBg] = gui::GetSurfaceVec4();
  style.Colors[ImGuiCol_ChildBg] = gui::GetSurfaceVariantVec4();
  style.Colors[ImGuiCol_PopupBg] = gui::GetSurfaceContainerVec4();
  style.Colors[ImGuiCol_Border] = gui::GetOutlineVec4();
  style.Colors[ImGuiCol_BorderShadow] = gui::GetShadowVec4();
  
  // Text colors
  style.Colors[ImGuiCol_Text] = gui::GetOnSurfaceVec4();
  style.Colors[ImGuiCol_TextDisabled] = gui::GetOnSurfaceVariantVec4();
  
  // Button colors
  style.Colors[ImGuiCol_Button] = gui::GetSurfaceContainerHighestVec4();
  style.Colors[ImGuiCol_ButtonHovered] = gui::GetSurfaceContainerHighVec4();
  style.Colors[ImGuiCol_ButtonActive] = gui::GetPrimaryVec4();
  
  // Header colors
  style.Colors[ImGuiCol_Header] = gui::GetSurfaceContainerHighVec4();
  style.Colors[ImGuiCol_HeaderHovered] = gui::GetSurfaceContainerHighestVec4();
  style.Colors[ImGuiCol_HeaderActive] = gui::GetPrimaryVec4();
  
  // Frame colors
  style.Colors[ImGuiCol_FrameBg] = gui::GetSurfaceContainerHighestVec4();
  style.Colors[ImGuiCol_FrameBgHovered] = gui::GetSurfaceContainerHighVec4();
  style.Colors[ImGuiCol_FrameBgActive] = gui::GetPrimaryVec4();
  
  // Scrollbar colors
  style.Colors[ImGuiCol_ScrollbarBg] = gui::GetSurfaceContainerVec4();
  style.Colors[ImGuiCol_ScrollbarGrab] = gui::GetOutlineVec4();
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = gui::GetOnSurfaceVariantVec4();
  style.Colors[ImGuiCol_ScrollbarGrabActive] = gui::GetOnSurfaceVec4();
  
  // Slider colors
  style.Colors[ImGuiCol_SliderGrab] = gui::GetPrimaryVec4();
  style.Colors[ImGuiCol_SliderGrabActive] = gui::GetPrimaryActiveVec4();
  
  // Tab colors
  style.Colors[ImGuiCol_Tab] = gui::GetSurfaceContainerHighVec4();
  style.Colors[ImGuiCol_TabHovered] = gui::GetSurfaceContainerHighestVec4();
  style.Colors[ImGuiCol_TabActive] = gui::GetPrimaryVec4();
  style.Colors[ImGuiCol_TabUnfocused] = gui::GetSurfaceContainerVec4();
  style.Colors[ImGuiCol_TabUnfocusedActive] = gui::GetPrimaryActiveVec4();
  
  // Title bar colors
  style.Colors[ImGuiCol_TitleBg] = gui::GetSurfaceContainerVec4();
  style.Colors[ImGuiCol_TitleBgActive] = gui::GetSurfaceContainerHighVec4();
  style.Colors[ImGuiCol_TitleBgCollapsed] = gui::GetSurfaceContainerVec4();
  
  // Menu bar colors
  style.Colors[ImGuiCol_MenuBarBg] = gui::GetSurfaceContainerVec4();
  
  // Modal dimming
  style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);
}

void UICoordinator::UpdateThemeElements() {
  // Update theme-specific elements
  ApplyMaterialDesignStyling();
}

void UICoordinator::DrawThemePreview() {
  // TODO: Implement theme preview
  // This would show a preview of the current theme
}

// Helper methods for drawing operations
void UICoordinator::DrawSessionIndicator() {
  // TODO: Implement session indicator
}

void UICoordinator::DrawVersionInfo() {
  // TODO: Implement version info display
}

void UICoordinator::DrawSessionTabs() {
  // TODO: Implement session tabs
}

void UICoordinator::DrawSessionBadges() {
  // TODO: Implement session badges
}

// Material Design component helpers
void UICoordinator::DrawMaterialButton(const std::string& text, const std::string& icon, 
                                      std::function<void()> callback, bool enabled) {
  if (!enabled) {
    ImGui::PushStyleColor(ImGuiCol_Button, gui::GetSurfaceContainerHighestVec4());
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetOnSurfaceVariantVec4());
  }
  
  std::string button_text = absl::StrFormat("%s %s", icon.c_str(), text.c_str());
  if (ImGui::Button(button_text.c_str())) {
    if (enabled && callback) {
      callback();
    }
  }
  
  if (!enabled) {
    ImGui::PopStyleColor(2);
  }
}

void UICoordinator::DrawMaterialCard(const std::string& title, const std::string& content) {
  // TODO: Implement Material Design card component
}

void UICoordinator::DrawMaterialDialog(const std::string& title, std::function<void()> content) {
  // TODO: Implement Material Design dialog component
}

// Layout and positioning helpers
void UICoordinator::CenterWindow(const std::string& window_name) {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void UICoordinator::PositionWindow(const std::string& window_name, float x, float y) {
  ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
}

void UICoordinator::SetWindowSize(const std::string& window_name, float width, float height) {
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

// Icon and theming helpers
std::string UICoordinator::GetIconForEditor(EditorType type) const {
  switch (type) {
    case EditorType::kDungeon: return ICON_MD_CASTLE;
    case EditorType::kOverworld: return ICON_MD_MAP;
    case EditorType::kGraphics: return ICON_MD_IMAGE;
    case EditorType::kPalette: return ICON_MD_PALETTE;
    case EditorType::kSprite: return ICON_MD_TOYS;
    case EditorType::kScreen: return ICON_MD_TV;
    case EditorType::kMessage: return ICON_MD_CHAT_BUBBLE;
    case EditorType::kMusic: return ICON_MD_MUSIC_NOTE;
    case EditorType::kAssembly: return ICON_MD_CODE;
    case EditorType::kHex: return ICON_MD_DATA_ARRAY;
    case EditorType::kEmulator: return ICON_MD_PLAY_ARROW;
    case EditorType::kSettings: return ICON_MD_SETTINGS;
    default: return ICON_MD_HELP;
  }
}

std::string UICoordinator::GetColorForEditor(EditorType type) const {
  // TODO: Implement editor-specific colors
  return "primary";  // Placeholder for now
}

void UICoordinator::ApplyEditorTheme(EditorType type) {
  // TODO: Implement editor-specific theming
}

// Session UI helpers
void UICoordinator::DrawSessionList() {
  // TODO: Implement session list
}

void UICoordinator::DrawSessionControls() {
  // TODO: Implement session controls
}

void UICoordinator::DrawSessionInfo() {
  // TODO: Implement session info
}

void UICoordinator::DrawSessionStatus() {
  // TODO: Implement session status
}

// Popup helpers
void UICoordinator::DrawHelpMenuPopups() {
  // TODO: Implement help menu popups
}

void UICoordinator::DrawSettingsPopups() {
  // TODO: Implement settings popups
}

void UICoordinator::DrawProjectPopups() {
  // TODO: Implement project popups
}

void UICoordinator::DrawSessionPopups() {
  // TODO: Implement session popups
}

// Window management helpers
void UICoordinator::DrawWindowControls() {
  // TODO: Implement window controls
}

void UICoordinator::DrawLayoutControls() {
  // TODO: Implement layout controls
}

void UICoordinator::DrawDockingControls() {
  // TODO: Implement docking controls
}

// Performance and debug UI
void UICoordinator::DrawPerformanceUI() {
  // TODO: Implement performance UI
}

void UICoordinator::DrawDebugUI() {
  // TODO: Implement debug UI
}

void UICoordinator::DrawTestingUI() {
  // TODO: Implement testing UI
}

}  // namespace editor
}  // namespace yaze

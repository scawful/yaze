#include "app/editor/ui/ui_coordinator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/core/project.h"
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
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

UICoordinator::UICoordinator(
    EditorManager* editor_manager,
    RomFileManager& rom_manager,
    ProjectManager& project_manager,
    EditorRegistry& editor_registry,
    EditorCardRegistry& card_registry,
    SessionCoordinator& session_coordinator,
    WindowDelegate& window_delegate,
    ToastManager& toast_manager,
    PopupManager& popup_manager,
    ShortcutManager& shortcut_manager)
    : editor_manager_(editor_manager),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      card_registry_(card_registry),
      session_coordinator_(session_coordinator),
      window_delegate_(window_delegate),
      toast_manager_(toast_manager),
      popup_manager_(popup_manager),
      shortcut_manager_(shortcut_manager) {
  
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
  // Note: Theme styling is applied by ThemeManager, not here
  // This is called from EditorManager::Update() - don't call menu bar stuff here
  
  // Draw UI windows and dialogs
  // Session dialogs are drawn by SessionCoordinator separately to avoid duplication
  DrawCommandPalette();          // Ctrl+Shift+P
  DrawLayoutPresets();           // Layout preset dialogs
  DrawWelcomeScreen();           // Welcome screen
  DrawProjectHelp();             // Project help
  DrawWindowManagementUI();      // Window management
}

void UICoordinator::DrawMenuBarExtras() {
  // Get current ROM from EditorManager (RomFileManager doesn't track "current")
  auto* current_rom = editor_manager_->GetCurrentRom();
  
  // Calculate version width for right alignment
  std::string version_text = absl::StrFormat("v%s", editor_manager_->version().c_str());
  float version_width = ImGui::CalcTextSize(version_text.c_str()).x;

  // Session indicator with Material Design styling
  if (session_coordinator_.HasMultipleSessions()) {
    ImGui::SameLine();
    std::string session_button_text = absl::StrFormat("%s %zu", ICON_MD_TAB, 
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
      ImGui::SetTooltip("Switch Sessions (Ctrl+Tab)");
    }
  }

  // ROM information display with Material Design card styling
  ImGui::SameLine();
  if (current_rom && current_rom->is_loaded()) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    std::string rom_title = current_rom->title();
    if (current_rom->dirty()) {
      ImGui::Text("%s %s*", ICON_MD_CIRCLE, rom_title.c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Unsaved changes");
      }
    } else {
      ImGui::Text("%s %s", ICON_MD_INSERT_DRIVE_FILE, rom_title.c_str());
    }
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
    ImGui::Text("%s No ROM", ICON_MD_WARNING);
    ImGui::PopStyleColor();
  }

  // Version info aligned to far right
  ImGui::SameLine(ImGui::GetWindowWidth() - version_width - 15.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::Text("%s", version_text.c_str());
  ImGui::PopStyleColor();
}

void UICoordinator::DrawContextSensitiveCardControl() {
  // Get the currently active editor directly from EditorManager
  // This ensures we show cards for the correct editor that has focus
  auto* active_editor = editor_manager_->GetCurrentEditor();
  if (!active_editor) return;
  
  // Only show card control for card-based editors (not palette, not assembly in legacy mode, etc.)
  if (!editor_registry_.IsCardBasedEditor(active_editor->type())) {
    return;
  }
  
  // Get the category and session for the active editor
  std::string category = editor_registry_.GetEditorCategory(active_editor->type());
  size_t session_id = editor_manager_->GetCurrentSessionId();
  
  // Draw compact card control in menu bar (mini dropdown for cards)
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, gui::GetSurfaceContainerHighestVec4());
  
  if (ImGui::SmallButton(absl::StrFormat("%s %s", ICON_MD_LAYERS, category.c_str()).c_str())) {
    ImGui::OpenPopup("##CardQuickAccess");
  }
  
  ImGui::PopStyleColor(2);
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Quick access to %s cards", category.c_str());
  }
  
  // Quick access popup for toggling cards
  if (ImGui::BeginPopup("##CardQuickAccess")) {
    auto cards = card_registry_.GetCardsInCategory(session_id, category);
    
    for (const auto& card : cards) {
      bool visible = card.visibility_flag ? *card.visibility_flag : false;
      if (ImGui::MenuItem(card.display_name.c_str(), nullptr, visible)) {
        if (visible) {
          card_registry_.HideCard(session_id, card.card_id);
        } else {
          card_registry_.ShowCard(session_id, card.card_id);
        }
      }
    }
    ImGui::EndPopup();
  }
}

// ============================================================================
// Session UI Delegation
// ============================================================================
// All session-related UI is now managed by SessionCoordinator to eliminate
// duplication. UICoordinator methods delegate to SessionCoordinator.

void UICoordinator::ShowSessionSwitcher() {
  session_coordinator_.ShowSessionSwitcher();
}

bool UICoordinator::IsSessionSwitcherVisible() const {
  return session_coordinator_.IsSessionSwitcherVisible();
}

void UICoordinator::SetSessionSwitcherVisible(bool visible) {
  if (visible) {
    session_coordinator_.ShowSessionSwitcher();
  } else {
    session_coordinator_.HideSessionSwitcher();
  }
}

// ============================================================================
// Layout and Window Management UI
// ============================================================================

void UICoordinator::DrawLayoutPresets() {
  // TODO: Implement layout presets UI
  // This would show available layout presets (Developer, Designer, Modder)
}

void UICoordinator::DrawWelcomeScreen() {
  // ============================================================================
  // WELCOME SCREEN VISIBILITY LOGIC - Redesigned for clarity
  // ============================================================================
  // 
  // SHOW WELCOME SCREEN WHEN:
  // 1. No ROM is loaded AND
  // 2. User hasn't manually closed it this session
  //
  // HIDE WELCOME SCREEN WHEN:
  // 1. ROM is loaded OR
  // 2. User closes it manually
  // ============================================================================
  
  if (!editor_manager_) {
    LOG_ERROR("UICoordinator", "EditorManager is null - cannot check ROM state");
    return;
  }
  
  if (!welcome_screen_) {
    LOG_ERROR("UICoordinator", "WelcomeScreen object is null - cannot render");
    return;
  }
  
  // Get current ROM state
  auto* current_rom = editor_manager_->GetCurrentRom();
  bool rom_is_loaded = current_rom && current_rom->is_loaded();
  
  // Determine if welcome screen should be visible
  bool should_show = !rom_is_loaded && !welcome_screen_manually_closed_;
  
  // Log state changes for debugging
  static bool last_should_show = false;
  static bool first_run = true;
  if (first_run || should_show != last_should_show) {
    LOG_INFO("UICoordinator", 
             "Welcome screen state: should_show=%s, rom_loaded=%s, manually_closed=%s",
             should_show ? "true" : "false",
             rom_is_loaded ? "true" : "false", 
             welcome_screen_manually_closed_ ? "true" : "false");
    last_should_show = should_show;
    first_run = false;
  }
  
  // Update visibility flag
  show_welcome_screen_ = should_show;
  
  // Early exit if shouldn't show
  if (!should_show) {
    return;
  }
  
  // Draw the welcome screen
  LOG_INFO("UICoordinator", "Rendering welcome screen window");
  
  // Update recent projects before showing
  welcome_screen_->RefreshRecentProjects();
  
  // Show the welcome screen (it manages its own ImGui window)
  bool is_open = true;
  bool action_taken = welcome_screen_->Show(&is_open);
  
  // If user closed the window via X button, mark as manually closed
  if (!is_open) {
    welcome_screen_manually_closed_ = true;
    show_welcome_screen_ = false;
    welcome_screen_->MarkManuallyClosed();
    LOG_INFO("UICoordinator", "Welcome screen manually closed by user (X button)");
  }
  
  // If an action was taken (ROM loaded, project opened), the welcome screen will auto-hide
  // next frame when rom_is_loaded becomes true
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
  // Display Settings is now a popup managed by PopupManager
  // Delegate directly to PopupManager instead of UICoordinator
  popup_manager_.Show(PopupID::kDisplaySettings);
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

void UICoordinator::DrawCommandPalette() {
  if (!show_command_palette_) return;
  
  LOG_INFO("UICoordinator", "DrawCommandPalette() - rendering command palette");
  
  using namespace ImGui;
  auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  
  SetNextWindowPos(GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  
  bool show_palette = true;
  if (Begin(absl::StrFormat("%s Command Palette", ICON_MD_SEARCH).c_str(),
            &show_palette, ImGuiWindowFlags_NoCollapse)) {
    
    // Search input with focus management
    SetNextItemWidth(-100);
    if (IsWindowAppearing()) {
      SetKeyboardFocusHere();
      command_palette_selected_idx_ = 0;
    }
    
    bool input_changed = InputTextWithHint(
        "##cmd_query",
        absl::StrFormat("%s Search commands (fuzzy matching enabled)...", ICON_MD_SEARCH).c_str(),
        command_palette_query_, IM_ARRAYSIZE(command_palette_query_));
    
    SameLine();
    if (Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
      command_palette_query_[0] = '\0';
      input_changed = true;
      command_palette_selected_idx_ = 0;
    }
    
    Separator();
    
    // Fuzzy filter commands with scoring
    std::vector<std::pair<int, std::pair<std::string, std::string>>> scored_commands;
    std::string query_lower = command_palette_query_;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    
    for (const auto& entry : shortcut_manager_.GetShortcuts()) {
      const auto& name = entry.first;
      const auto& shortcut = entry.second;
      
      std::string name_lower = name;
      std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
      
      int score = 0;
      if (command_palette_query_[0] == '\0') {
        score = 1;  // Show all when no query
      } else if (name_lower.find(query_lower) == 0) {
        score = 1000;  // Starts with
      } else if (name_lower.find(query_lower) != std::string::npos) {
        score = 500;  // Contains
      } else {
        // Fuzzy match - characters in order
        size_t text_idx = 0, query_idx = 0;
        while (text_idx < name_lower.length() && query_idx < query_lower.length()) {
          if (name_lower[text_idx] == query_lower[query_idx]) {
            score += 10;
            query_idx++;
          }
          text_idx++;
        }
        if (query_idx != query_lower.length()) score = 0;
      }
      
      if (score > 0) {
        std::string shortcut_text = shortcut.keys.empty()
            ? ""
            : absl::StrFormat("(%s)", PrintShortcut(shortcut.keys).c_str());
        scored_commands.push_back({score, {name, shortcut_text}});
      }
    }
    
    std::sort(scored_commands.begin(), scored_commands.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Display results with categories
    if (BeginTabBar("CommandCategories")) {
      if (BeginTabItem(absl::StrFormat("%s All Commands", ICON_MD_LIST).c_str())) {
        if (gui::LayoutHelpers::BeginTableWithTheming("CommandPaletteTable", 3,
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, -30))) {
          
          TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch, 0.5f);
          TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, 0.3f);
          TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch, 0.2f);
          TableHeadersRow();
          
          for (size_t i = 0; i < scored_commands.size(); ++i) {
            const auto& [score, cmd_pair] = scored_commands[i];
            const auto& [command_name, shortcut_text] = cmd_pair;
            
            TableNextRow();
            TableNextColumn();
            
            PushID(static_cast<int>(i));
            bool is_selected = (static_cast<int>(i) == command_palette_selected_idx_);
            if (Selectable(command_name.c_str(), is_selected,
                          ImGuiSelectableFlags_SpanAllColumns)) {
              command_palette_selected_idx_ = i;
              const auto& shortcuts = shortcut_manager_.GetShortcuts();
              auto it = shortcuts.find(command_name);
              if (it != shortcuts.end() && it->second.callback) {
                it->second.callback();
                show_command_palette_ = false;
              }
            }
            PopID();
            
            TableNextColumn();
            PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.text_secondary));
            Text("%s", shortcut_text.c_str());
            PopStyleColor();
            
            TableNextColumn();
            if (score > 0) {
              PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.text_disabled));
              Text("%d", score);
              PopStyleColor();
            }
          }
          
          EndTable();
        }
        EndTabItem();
      }
      
      if (BeginTabItem(absl::StrFormat("%s Recent", ICON_MD_HISTORY).c_str())) {
        Text("Recent commands coming soon...");
        EndTabItem();
      }
      
      if (BeginTabItem(absl::StrFormat("%s Frequent", ICON_MD_STAR).c_str())) {
        Text("Frequent commands coming soon...");
        EndTabItem();
      }
      
      EndTabBar();
    }
    
    // Status bar with tips
    Separator();
    Text("%s %zu commands | Score: fuzzy match", ICON_MD_INFO, scored_commands.size());
    SameLine();
    PushStyleColor(ImGuiCol_Text, gui::ConvertColorToImVec4(theme.text_disabled));
    Text("| ↑↓=Navigate | Enter=Execute | Esc=Close");
    PopStyleColor();
  }
  End();
  
  // Update visibility state
  if (!show_palette) {
    show_command_palette_ = false;
  }
}

}  // namespace editor
}  // namespace yaze


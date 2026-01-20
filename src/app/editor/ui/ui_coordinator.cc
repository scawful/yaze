#include "app/editor/ui/ui_coordinator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IOS == 1
#include "app/platform/ios/ios_platform_state.h"
#endif
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/editor/layout/window_delegate.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/project_manager.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/welcome_screen.h"
#include "app/gui/core/background_renderer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

UICoordinator::UICoordinator(
    EditorManager* editor_manager, RomFileManager& rom_manager,
    ProjectManager& project_manager, EditorRegistry& editor_registry,
    PanelManager& panel_manager, SessionCoordinator& session_coordinator,
    WindowDelegate& window_delegate, ToastManager& toast_manager,
    PopupManager& popup_manager, ShortcutManager& shortcut_manager)
    : editor_manager_(editor_manager),
      rom_manager_(rom_manager),
      project_manager_(project_manager),
      editor_registry_(editor_registry),
      panel_manager_(panel_manager),
      session_coordinator_(session_coordinator),
      window_delegate_(window_delegate),
      toast_manager_(toast_manager),
      popup_manager_(popup_manager),
      shortcut_manager_(shortcut_manager) {
  // Initialize welcome screen with proper callbacks
  welcome_screen_ = std::make_unique<WelcomeScreen>();

  // Wire welcome screen callbacks to EditorManager
  welcome_screen_->SetOpenRomCallback([this]() {
#ifdef __EMSCRIPTEN__
    // In web builds, trigger the file input element directly
    // The file input handler in app.js will handle the file selection
    // and call LoadRomFromWeb, which will update the ROM
    EM_ASM({
      var romInput = document.getElementById('rom-input');
      if (romInput) {
        romInput.click();
      }
    });
    // Don't hide welcome screen yet - it will be hidden when ROM loads
    // (DrawWelcomeScreen auto-transitions to Dashboard on ROM load)
#else
    if (editor_manager_) {
      auto status = editor_manager_->LoadRom();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to load ROM: %s", status.message()),
            ToastType::kError);
      }
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
      else {
        // Transition to Dashboard on successful ROM load
        SetStartupSurface(StartupSurface::kDashboard);
      }
#endif
    }
#endif
  });

  welcome_screen_->SetNewProjectCallback([this]() {
    if (editor_manager_) {
      auto status = editor_manager_->CreateNewProject();
      if (!status.ok()) {
        toast_manager_.Show(
            absl::StrFormat("Failed to create project: %s", status.message()),
            ToastType::kError);
      } else {
        // Transition to Dashboard on successful project creation
        SetStartupSurface(StartupSurface::kDashboard);
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
        // Transition to Dashboard on successful project open
        SetStartupSurface(StartupSurface::kDashboard);
      }
    }
  });

  welcome_screen_->SetOpenAgentCallback([this]() {
    if (editor_manager_) {
#ifdef YAZE_BUILD_AGENT_UI
      editor_manager_->ShowAIAgent();
#endif
      // Keep welcome screen visible - user may want to do other things
    }
  });
}

void UICoordinator::SetWelcomeScreenBehavior(StartupVisibility mode) {
  welcome_behavior_override_ = mode;
  if (mode == StartupVisibility::kHide) {
    welcome_screen_manually_closed_ = true;
    // If hiding welcome, transition to appropriate state
    if (current_startup_surface_ == StartupSurface::kWelcome) {
      SetStartupSurface(StartupSurface::kDashboard);
    }
  } else if (mode == StartupVisibility::kShow) {
    welcome_screen_manually_closed_ = false;
    SetStartupSurface(StartupSurface::kWelcome);
  }
}

void UICoordinator::SetDashboardBehavior(StartupVisibility mode) {
  if (dashboard_behavior_override_ == mode) {
    return;
  }
  dashboard_behavior_override_ = mode;
  if (mode == StartupVisibility::kShow) {
    // Only transition to dashboard if we're not in welcome
    if (current_startup_surface_ != StartupSurface::kWelcome) {
      SetStartupSurface(StartupSurface::kDashboard);
    }
  } else if (mode == StartupVisibility::kHide) {
    // If hiding dashboard, transition to editor state
    if (current_startup_surface_ == StartupSurface::kDashboard) {
      SetStartupSurface(StartupSurface::kEditor);
    }
  }
}

void UICoordinator::DrawBackground() {
  if (ImGui::GetCurrentContext()) {
    ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    auto& theme_manager = gui::ThemeManager::Get();
    auto current_theme = theme_manager.GetCurrentTheme();
    auto& bg_renderer = gui::BackgroundRenderer::Get();

    // Draw grid covering the entire main viewport
    ImVec2 grid_pos = viewport->WorkPos;
    ImVec2 grid_size = viewport->WorkSize;
    bg_renderer.RenderDockingBackground(bg_draw_list, grid_pos, grid_size,
                                        current_theme.primary);
  }
}

void UICoordinator::DrawAllUI() {
  // Note: Theme styling is applied by ThemeManager, not here
  // This is called from EditorManager::Update() - don't call menu bar stuff
  // here

  // Draw UI windows and dialogs
  // Session dialogs are drawn by SessionCoordinator separately to avoid
  // duplication
  DrawCommandPalette();          // Ctrl+Shift+P
  DrawGlobalSearch();            // Ctrl+Shift+K
  DrawWorkspacePresetDialogs();  // Save/Load workspace dialogs
  DrawLayoutPresets();           // Layout preset dialogs
  DrawWelcomeScreen();           // Welcome screen
  DrawProjectHelp();             // Project help
  DrawWindowManagementUI();      // Window management

  // Draw popups and toasts
  DrawAllPopups();
  toast_manager_.Draw();
  DrawMobileNavigation();
}

bool UICoordinator::IsCompactLayout() const {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return false;
  }
  const float width = viewport->WorkSize.x;
#if defined(__APPLE__) && TARGET_OS_IOS == 1
  return true;
#else
  return width < 900.0f;
#endif
}

void UICoordinator::DrawMobileNavigation() {
  if (!IsCompactLayout()) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  ImVec2 safe = style.DisplaySafeAreaPadding;
#if defined(__APPLE__) && TARGET_OS_IOS == 1
  const auto safe_area = ::yaze::platform::ios::GetSafeAreaInsets();
  if (safe_area.left != 0.0f || safe_area.right != 0.0f ||
      safe_area.top != 0.0f || safe_area.bottom != 0.0f) {
    safe = ImVec2(safe_area.right, safe_area.bottom);
  }
#endif
  const float button_size = std::max(44.0f, ImGui::GetFontSize() * 2.1f);
  const float padding = style.WindowPadding.x;
  const ImVec2 pos(viewport->WorkPos.x + viewport->WorkSize.x - safe.x -
                       padding - button_size,
                   viewport->WorkPos.y + viewport->WorkSize.y - safe.y -
                       padding - button_size);

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 button_color = gui::ConvertColorToImVec4(theme.button);
  const ImVec4 button_hovered = gui::ConvertColorToImVec4(theme.button_hovered);
  const ImVec4 button_active = gui::ConvertColorToImVec4(theme.button_active);
  const ImVec4 button_text = gui::ConvertColorToImVec4(theme.accent);

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(button_size, button_size), ImGuiCond_Always);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (ImGui::Begin("##MobileNavButton", nullptr, flags)) {
    ImGui::PushStyleColor(ImGuiCol_Button, button_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active);
    ImGui::PushStyleColor(ImGuiCol_Text, button_text);

    if (ImGui::Button(ICON_MD_APPS, ImVec2(button_size, button_size))) {
      ImGui::OpenPopup("##MobileNavPopup");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Navigation");
    }

    ImGui::PopStyleColor(4);
  }
  ImGui::End();
  ImGui::PopStyleVar();

  ImGui::PushStyleColor(ImGuiCol_PopupBg,
                        gui::ConvertColorToImVec4(theme.surface));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

  if (ImGui::BeginPopup("##MobileNavPopup")) {
    bool has_rom = false;
    if (editor_manager_) {
      auto* current_rom = editor_manager_->GetCurrentRom();
      has_rom = current_rom && current_rom->is_loaded();
    }

    if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Open ROM")) {
      if (editor_manager_) {
        auto status = editor_manager_->LoadRom();
        if (!status.ok()) {
          toast_manager_.Show(
              absl::StrFormat("Failed to load ROM: %s", status.message()),
              ToastType::kError);
        }
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
        else {
          SetStartupSurface(StartupSurface::kDashboard);
        }
#endif
      }
    }

    if (has_rom) {
      if (ImGui::MenuItem(
              ICON_MD_DASHBOARD " Dashboard", nullptr,
              current_startup_surface_ == StartupSurface::kDashboard)) {
        SetStartupSurface(StartupSurface::kDashboard);
      }
      if (ImGui::MenuItem(
              ICON_MD_EDIT " Editor", nullptr,
              current_startup_surface_ == StartupSurface::kEditor)) {
        SetStartupSurface(StartupSurface::kEditor);
      }
    }

    const bool sidebar_visible = panel_manager_.IsSidebarVisible();
    if (ImGui::MenuItem(ICON_MD_VIEW_SIDEBAR " Toggle Sidebar", nullptr,
                        sidebar_visible)) {
      TogglePanelSidebar();
    }

    if (editor_manager_) {
      auto* right_panel = editor_manager_->right_panel_manager();
      if (right_panel) {
        const bool settings_active =
            right_panel->IsPanelActive(RightPanelManager::PanelType::kSettings);
        if (ImGui::MenuItem(ICON_MD_SETTINGS " Settings", nullptr,
                            settings_active)) {
          right_panel->TogglePanel(RightPanelManager::PanelType::kSettings);
        }
      }
    }

    ImGui::EndPopup();
  }

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

// =============================================================================
// Menu Bar Helpers
// =============================================================================

bool UICoordinator::DrawMenuBarIconButton(const char* icon, const char* tooltip,
                                          bool is_active) {
  // Push consistent button styling
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        gui::GetSurfaceContainerHighestVec4());

  // Active state uses primary color, inactive uses secondary text
  if (is_active) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  }

  bool clicked = ImGui::SmallButton(icon);

  ImGui::PopStyleColor(4);

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

float UICoordinator::GetMenuBarIconButtonWidth() {
  // SmallButton width = text width + frame padding * 2
  const float frame_padding = ImGui::GetStyle().FramePadding.x;
  // Use a standard icon width (Material Design icons are uniform)
  const float icon_width = ImGui::CalcTextSize(ICON_MD_SETTINGS).x;
  return icon_width + frame_padding * 2.0f;
}

void UICoordinator::DrawMenuBarExtras() {
  // Right-aligned status cluster: Version, dirty indicator, session, bell, panel toggles
  // Panel toggles are positioned using SCREEN coordinates (from viewport) so they
  // stay fixed even when the dockspace resizes due to panel open/close.
  //
  // Layout: [v0.x.x][●][📄▾][🔔] [panels][⬆]
  //         ^^^ shifts with dockspace ^^^  ^^^ fixed screen position ^^^

  auto* current_rom = editor_manager_->GetCurrentRom();
  const std::string full_version =
      absl::StrFormat("v%s", editor_manager_->version().c_str());

  const float item_spacing = 6.0f;
  const float button_width = GetMenuBarIconButtonWidth();
  const float padding = 8.0f;

  // Get TRUE viewport dimensions (not affected by dockspace resize)
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float true_viewport_right = viewport->WorkPos.x + viewport->WorkSize.x;

  // Calculate panel toggle region width
  // Buttons: Project, Agent (GRPC only), Help, Settings, Properties
  int panel_button_count = 0;
  if (editor_manager_->right_panel_manager()) {
#ifdef YAZE_WITH_GRPC
    panel_button_count = 5;  // Project, Agent, Help, Settings, Properties
#else
    panel_button_count = 4;  // Project, Help, Settings, Properties
#endif
  }

  float panel_region_width = 0.0f;
  if (panel_button_count > 0) {
    panel_region_width = (button_width * panel_button_count) +
                         (item_spacing * (panel_button_count - 1)) + padding;
  }
#ifdef __EMSCRIPTEN__
  panel_region_width += button_width + item_spacing;  // WASM toggle
#endif

  // Calculate screen X position for panel toggles (fixed at viewport right edge)
  float panel_screen_x = true_viewport_right - panel_region_width;
  if (editor_manager_->right_panel_manager() &&
      editor_manager_->right_panel_manager()->IsPanelExpanded()) {
    panel_screen_x -= editor_manager_->right_panel_manager()->GetPanelWidth();
  }

  // Calculate available space for status cluster (version, dirty, session, bell)
  // This ends where the panel toggle region begins
  const float window_width = ImGui::GetWindowWidth();
  const float window_screen_x = ImGui::GetWindowPos().x;
  const float menu_items_end = ImGui::GetCursorPosX() + 16.0f;

  // Convert panel screen X to window-local coordinates for space calculation
  float panel_local_x = panel_screen_x - window_screen_x;
  float region_end =
      std::min(window_width - padding, panel_local_x - item_spacing);

  // Calculate what elements to show - progressive hiding when space is tight
  bool has_dirty_rom =
      current_rom && current_rom->is_loaded() && current_rom->dirty();
  bool has_multiple_sessions = session_coordinator_.HasMultipleSessions();

  float version_width = ImGui::CalcTextSize(full_version.c_str()).x;
  float dirty_width =
      ImGui::CalcTextSize(ICON_MD_FIBER_MANUAL_RECORD).x + item_spacing;
  float session_width = button_width;

  const float available_width = region_end - menu_items_end - padding;

  // Minimum required width: just the bell (always visible)
  float required_width = button_width;

  // Progressive show/hide based on available space
  // Priority (highest to lowest): Bell > Dirty > Session > Version

  // Try to fit version (lowest priority - hide first when tight)
  bool show_version =
      (required_width + version_width + item_spacing) <= available_width;
  if (show_version) {
    required_width += version_width + item_spacing;
  }

  // Try to fit session button (medium priority)
  bool show_session =
      has_multiple_sessions &&
      (required_width + session_width + item_spacing) <= available_width;
  if (show_session) {
    required_width += session_width + item_spacing;
  }

  // Try to fit dirty indicator (high priority - only hide if extremely tight)
  bool show_dirty =
      has_dirty_rom && (required_width + dirty_width) <= available_width;
  if (show_dirty) {
    required_width += dirty_width;
  }

  // Calculate start position (right-align within available space)
  float start_pos = std::max(menu_items_end, region_end - required_width);

  // =========================================================================
  // DRAW STATUS CLUSTER (shifts with dockspace)
  // =========================================================================
  ImGui::SameLine(start_pos);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(item_spacing, 0.0f));

  // 1. Version - subdued gray text
  if (show_version) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
    ImGui::Text("%s", full_version.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
  }

  // 2. Dirty badge - warning color dot
  if (show_dirty) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::PushStyleColor(ImGuiCol_Text,
                          gui::ConvertColorToImVec4(theme.warning));
    ImGui::Text(ICON_MD_FIBER_MANUAL_RECORD);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Unsaved changes: %s",
                        current_rom->short_name().c_str());
    }
    ImGui::SameLine();
  }

  // 3. Session button - layers icon
  if (show_session) {
    DrawSessionButton();
    ImGui::SameLine();
  }

  // 4. Notification bell (pass visibility flags for enhanced tooltip)
  DrawNotificationBell(show_dirty, has_dirty_rom, show_session,
                       has_multiple_sessions);

  // =========================================================================
  // DRAW PANEL TOGGLES (fixed screen position, unaffected by dockspace resize)
  // =========================================================================
  if (panel_button_count > 0) {
    // Get current Y position within menu bar
    float menu_bar_y = ImGui::GetCursorScreenPos().y;

    // Position at fixed screen coordinates
    ImGui::SetCursorScreenPos(ImVec2(panel_screen_x, menu_bar_y));

    // Draw panel toggle buttons
    editor_manager_->right_panel_manager()->DrawPanelToggleButtons();
  }

#ifdef __EMSCRIPTEN__
  // WASM toggle button - also at fixed position
  ImGui::SameLine();
  if (DrawMenuBarIconButton(ICON_MD_EXPAND_LESS,
                            "Hide menu bar (Alt to restore)")) {
    show_menu_bar_ = false;
  }
#endif

  ImGui::PopStyleVar();  // ItemSpacing
}

void UICoordinator::DrawMenuBarRestoreButton() {
  // Only draw when menu bar is hidden (primarily for WASM builds)
  if (show_menu_bar_) {
    return;
  }

  // Small floating button in top-left corner to restore menu bar
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;

  ImGui::SetNextWindowPos(ImVec2(8, 8));
  ImGui::SetNextWindowBgAlpha(0.7f);

  if (ImGui::Begin("##MenuBarRestore", nullptr, flags)) {
    ImGui::PushStyleColor(ImGuiCol_Button, gui::GetSurfaceContainerVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          gui::GetSurfaceContainerHighestVec4());
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());

    if (ImGui::Button(ICON_MD_FULLSCREEN_EXIT, ImVec2(32, 32))) {
      show_menu_bar_ = true;
    }

    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show menu bar (Alt)");
    }
  }
  ImGui::End();

  // Also check for Alt key to restore menu bar
  if (ImGui::IsKeyPressed(ImGuiKey_LeftAlt) ||
      ImGui::IsKeyPressed(ImGuiKey_RightAlt)) {
    show_menu_bar_ = true;
  }
}

void UICoordinator::DrawNotificationBell(bool show_dirty, bool has_dirty_rom,
                                         bool show_session,
                                         bool has_multiple_sessions) {
  size_t unread = toast_manager_.GetUnreadCount();
  auto* current_rom = editor_manager_->GetCurrentRom();
  auto* right_panel = editor_manager_->right_panel_manager();

  // Check if notifications panel is active
  bool is_active =
      right_panel &&
      right_panel->IsPanelActive(RightPanelManager::PanelType::kNotifications);

  // Bell icon with accent color when there are unread notifications or panel is active
  if (unread > 0 || is_active) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          gui::GetSurfaceContainerHighestVec4());
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          gui::GetSurfaceContainerHighVec4());
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          gui::GetSurfaceContainerHighestVec4());
  }

  // Bell button - opens notifications panel in right sidebar
  if (ImGui::SmallButton(ICON_MD_NOTIFICATIONS)) {
    if (right_panel) {
      right_panel->TogglePanel(RightPanelManager::PanelType::kNotifications);
      toast_manager_.MarkAllRead();
    }
  }

  ImGui::PopStyleColor(4);

  // Enhanced tooltip showing notifications + hidden status items
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();

    // Notifications
    if (unread > 0) {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
      ImGui::Text("%s %zu new notification%s", ICON_MD_NOTIFICATIONS, unread,
                  unread == 1 ? "" : "s");
      ImGui::PopStyleColor();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
      ImGui::Text(ICON_MD_NOTIFICATIONS " No new notifications");
      ImGui::PopStyleColor();
    }

    ImGui::TextDisabled("Click to open Notifications panel");

    // Show hidden status items if any
    if (!show_dirty && has_dirty_rom) {
      ImGui::Separator();
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          gui::ConvertColorToImVec4(
              gui::ThemeManager::Get().GetCurrentTheme().warning));
      ImGui::Text(ICON_MD_FIBER_MANUAL_RECORD " Unsaved changes: %s",
                  current_rom->short_name().c_str());
      ImGui::PopStyleColor();
    }

    if (!show_session && has_multiple_sessions) {
      if (!show_dirty && has_dirty_rom) {
        // Already had a separator
      } else {
        ImGui::Separator();
      }
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
      ImGui::Text(ICON_MD_LAYERS " %zu sessions active",
                  session_coordinator_.GetActiveSessionCount());
      ImGui::PopStyleColor();
    }

    ImGui::EndTooltip();
  }
}

void UICoordinator::DrawSessionButton() {
  auto* current_rom = editor_manager_->GetCurrentRom();

  // Consistent button styling with other menubar buttons
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        gui::GetSurfaceContainerHighVec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        gui::GetSurfaceContainerHighestVec4());
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());

  // Store button position for popup anchoring
  ImVec2 button_min = ImGui::GetCursorScreenPos();

  if (ImGui::SmallButton(ICON_MD_LAYERS)) {
    ImGui::OpenPopup("##SessionSwitcherPopup");
  }

  ImVec2 button_max = ImGui::GetItemRectMax();

  ImGui::PopStyleColor(4);

  if (ImGui::IsItemHovered()) {
    std::string tooltip = current_rom && current_rom->is_loaded()
                              ? current_rom->short_name()
                              : "No ROM loaded";
    ImGui::SetTooltip("%s\n%zu sessions open (Ctrl+Tab)", tooltip.c_str(),
                      session_coordinator_.GetActiveSessionCount());
  }

  // Anchor popup to right edge - position so right edge aligns with button
  const float popup_width = 250.0f;
  const float screen_width = ImGui::GetIO().DisplaySize.x;
  const float popup_x =
      std::min(button_min.x, screen_width - popup_width - 10.0f);

  ImGui::SetNextWindowPos(ImVec2(popup_x, button_max.y + 2.0f),
                          ImGuiCond_Appearing);

  // Session switcher popup
  if (ImGui::BeginPopup("##SessionSwitcherPopup")) {
    ImGui::Text(ICON_MD_LAYERS " Sessions");
    ImGui::Separator();

    for (size_t i = 0; i < session_coordinator_.GetTotalSessionCount(); ++i) {
      if (session_coordinator_.IsSessionClosed(i))
        continue;

      auto* session =
          static_cast<RomSession*>(session_coordinator_.GetSession(i));
      if (!session)
        continue;

      Rom* rom = &session->rom;
      ImGui::PushID(static_cast<int>(i));

      bool is_current = (rom == current_rom);
      if (is_current) {
        ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
      }

      std::string label =
          rom->is_loaded()
              ? absl::StrFormat("%s %s", ICON_MD_DESCRIPTION,
                                rom->short_name().c_str())
              : absl::StrFormat("%s Session %zu", ICON_MD_DESCRIPTION, i + 1);

      if (ImGui::Selectable(label.c_str(), is_current)) {
        editor_manager_->SwitchToSession(i);
      }

      if (is_current) {
        ImGui::PopStyleColor();
      }

      ImGui::PopID();
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

// Emulator visibility delegates to PanelManager (single source of truth)
bool UICoordinator::IsEmulatorVisible() const {
  size_t session_id = session_coordinator_.GetActiveSessionIndex();
  return panel_manager_.IsPanelVisible(session_id, "emulator.cpu_debugger");
}

void UICoordinator::SetEmulatorVisible(bool visible) {
  size_t session_id = session_coordinator_.GetActiveSessionIndex();
  if (visible) {
    panel_manager_.ShowPanel(session_id, "emulator.cpu_debugger");
  } else {
    panel_manager_.HidePanel(session_id, "emulator.cpu_debugger");
  }
}

// ============================================================================
// Layout and Window Management UI
// ============================================================================

void UICoordinator::DrawLayoutPresets() {
  // TODO: [EditorManagerRefactor] Implement full layout preset UI with
  // save/load For now, this is accessed via Window menu items that call
  // workspace_manager directly
}

void UICoordinator::DrawWelcomeScreen() {
  // ============================================================================
  // CENTRALIZED WELCOME SCREEN LOGIC (using StartupSurface state)
  // ============================================================================
  // Uses ShouldShowWelcome() as single source of truth
  // Auto-transitions to Dashboard on ROM load
  // Activity Bar hidden when welcome is visible
  // ============================================================================

  if (!editor_manager_) {
    LOG_ERROR("UICoordinator",
              "EditorManager is null - cannot check ROM state");
    return;
  }

  if (!welcome_screen_) {
    LOG_ERROR("UICoordinator", "WelcomeScreen object is null - cannot render");
    return;
  }

  // Check ROM state and update startup surface accordingly
  auto* current_rom = editor_manager_->GetCurrentRom();
  bool rom_is_loaded = current_rom && current_rom->is_loaded();

  // Auto-transition: ROM loaded -> Dashboard (unless manually closed)
  if (rom_is_loaded && current_startup_surface_ == StartupSurface::kWelcome &&
      !welcome_screen_manually_closed_) {
    SetStartupSurface(StartupSurface::kDashboard);
  }

  // Auto-transition: ROM unloaded -> Welcome (reset to welcome state)
  if (!rom_is_loaded && current_startup_surface_ != StartupSurface::kWelcome &&
      !welcome_screen_manually_closed_) {
    SetStartupSurface(StartupSurface::kWelcome);
  }

  // Use centralized visibility check
  if (!ShouldShowWelcome()) {
    return;
  }

  // Reset first show flag to override ImGui ini state
  welcome_screen_->ResetFirstShow();

  // Update recent projects before showing
  welcome_screen_->RefreshRecentProjects();

  // Pass layout offsets so welcome screen centers within dockspace region
  // Note: Activity Bar is hidden when welcome is shown, so left_offset = 0
  float left_offset =
      ShouldShowActivityBar() ? editor_manager_->GetLeftLayoutOffset() : 0.0f;
  float right_offset = editor_manager_->GetRightLayoutOffset();
  welcome_screen_->SetLayoutOffsets(left_offset, right_offset);

  // Show the welcome screen window
  bool is_open = true;
  welcome_screen_->Show(&is_open);

  // If user closed it via X button, respect that and transition to appropriate state
  if (!is_open) {
    welcome_screen_manually_closed_ = true;
    // Transition to Dashboard if ROM loaded, stay in Editor state otherwise
    if (rom_is_loaded) {
      SetStartupSurface(StartupSurface::kDashboard);
    }
  }
}

void UICoordinator::DrawProjectHelp() {
  // TODO: [EditorManagerRefactor] Implement project help dialog
  // Show context-sensitive help based on current editor and ROM state
}

void UICoordinator::DrawWorkspacePresetDialogs() {
  if (show_save_workspace_preset_) {
    ImGui::Begin("Save Workspace Preset", &show_save_workspace_preset_,
                 ImGuiWindowFlags_AlwaysAutoResize);
    static char preset_name[128] = "";
    ImGui::InputText("Name", preset_name, IM_ARRAYSIZE(preset_name));
    if (ImGui::Button("Save", gui::kDefaultModalSize)) {
      if (strlen(preset_name) > 0) {
        editor_manager_->SaveWorkspacePreset(preset_name);
        toast_manager_.Show("Preset saved", editor::ToastType::kSuccess);
        show_save_workspace_preset_ = false;
        preset_name[0] = '\0';
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", gui::kDefaultModalSize)) {
      show_save_workspace_preset_ = false;
      preset_name[0] = '\0';
    }
    ImGui::End();
  }

  if (show_load_workspace_preset_) {
    ImGui::Begin("Load Workspace Preset", &show_load_workspace_preset_,
                 ImGuiWindowFlags_AlwaysAutoResize);

    // Lazy load workspace presets when UI is accessed
    editor_manager_->RefreshWorkspacePresets();

    if (auto* workspace_manager = editor_manager_->workspace_manager()) {
      for (const auto& name : workspace_manager->workspace_presets()) {
        if (ImGui::Selectable(name.c_str())) {
          editor_manager_->LoadWorkspacePreset(name);
          toast_manager_.Show("Preset loaded", editor::ToastType::kSuccess);
          show_load_workspace_preset_ = false;
        }
      }
      if (workspace_manager->workspace_presets().empty())
        ImGui::Text("No presets found");
    }
    ImGui::End();
  }
}

void UICoordinator::DrawWindowManagementUI() {
  // TODO: [EditorManagerRefactor] Implement window management dialog
  // Provide UI for toggling window visibility, managing docking, etc.
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

void UICoordinator::HideCurrentEditorPanels() {
  if (!editor_manager_)
    return;

  auto* current_editor = editor_manager_->GetCurrentEditor();
  if (!current_editor)
    return;

  std::string category =
      editor_registry_.GetEditorCategory(current_editor->type());
  size_t session_id = session_coordinator_.GetActiveSessionIndex();
  panel_manager_.HideAllPanelsInCategory(session_id, category);

  LOG_INFO("UICoordinator", "Hid all panels in category: %s", category.c_str());
}

// ============================================================================
// Sidebar Visibility (delegates to PanelManager)
// ============================================================================

void UICoordinator::TogglePanelSidebar() {
  panel_manager_.ToggleSidebarVisibility();
}

bool UICoordinator::IsPanelSidebarVisible() const {
  return panel_manager_.IsSidebarVisible();
}

void UICoordinator::SetPanelSidebarVisible(bool visible) {
  panel_manager_.SetSidebarVisible(visible);
}

void UICoordinator::ShowAllWindows() {
  window_delegate_.ShowAllWindows();
}

void UICoordinator::HideAllWindows() {
  window_delegate_.HideAllWindows();
}

// Material Design component helpers
void UICoordinator::DrawMaterialButton(const std::string& text,
                                       const std::string& icon,
                                       const ImVec4& color,
                                       std::function<void()> callback,
                                       bool enabled) {
  if (!enabled) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          gui::GetSurfaceContainerHighestVec4());
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetOnSurfaceVariantVec4());
  }

  std::string button_text =
      absl::StrFormat("%s %s", icon.c_str(), text.c_str());
  if (ImGui::Button(button_text.c_str())) {
    if (enabled && callback) {
      callback();
    }
  }

  if (!enabled) {
    ImGui::PopStyleColor(2);
  }
}

// Layout and positioning helpers
void UICoordinator::CenterWindow(const std::string& window_name) {
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

void UICoordinator::PositionWindow(const std::string& window_name, float x,
                                   float y) {
  ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Appearing);
}

void UICoordinator::SetWindowSize(const std::string& window_name, float width,
                                  float height) {
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

void UICoordinator::DrawCommandPalette() {
  if (!show_command_palette_)
    return;

  // Initialize command palette on first use
  if (!command_palette_initialized_) {
    InitializeCommandPalette(0);  // Default session
  }

  using namespace ImGui;
  auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  SetNextWindowPos(GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                   ImVec2(0.5f, 0.5f));
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
        absl::StrFormat("%s Search commands (fuzzy matching enabled)...",
                        ICON_MD_SEARCH)
            .c_str(),
        command_palette_query_, IM_ARRAYSIZE(command_palette_query_));

    SameLine();
    if (Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
      command_palette_query_[0] = '\0';
      input_changed = true;
      command_palette_selected_idx_ = 0;
    }

    Separator();

    // Unified command list structure
    struct ScoredCommand {
      int score;
      std::string name;
      std::string category;
      std::string shortcut;
      std::function<void()> callback;
    };
    std::vector<ScoredCommand> scored_commands;

    std::string query_lower = command_palette_query_;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   ::tolower);

    auto score_text = [&query_lower](const std::string& text) -> int {
      std::string text_lower = text;
      std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(),
                     ::tolower);

      if (query_lower.empty()) return 1;
      if (text_lower.find(query_lower) == 0) return 1000;
      if (text_lower.find(query_lower) != std::string::npos) return 500;

      // Fuzzy match
      size_t text_idx = 0, query_idx = 0;
      int score = 0;
      while (text_idx < text_lower.length() && query_idx < query_lower.length()) {
        if (text_lower[text_idx] == query_lower[query_idx]) {
          score += 10;
          query_idx++;
        }
        text_idx++;
      }
      return (query_idx == query_lower.length()) ? score : 0;
    };

    // Add shortcuts from ShortcutManager
    for (const auto& [name, shortcut] : shortcut_manager_.GetShortcuts()) {
      int score = score_text(name);
      if (score > 0) {
        std::string shortcut_text =
            shortcut.keys.empty()
                ? ""
                : absl::StrFormat("(%s)", PrintShortcut(shortcut.keys).c_str());
        scored_commands.push_back({score, name, "Shortcuts", shortcut_text,
                                   shortcut.callback});
      }
    }

    // Add commands from CommandPalette
    for (const auto& entry : command_palette_.GetAllCommands()) {
      int score = score_text(entry.name);
      // Also search category and description
      score += score_text(entry.category) / 2;
      score += score_text(entry.description) / 4;

      if (score > 0) {
        scored_commands.push_back({score, entry.name, entry.category,
                                   entry.shortcut, entry.callback});
      }
    }

    // Sort by score descending
    std::sort(scored_commands.begin(), scored_commands.end(),
              [](const auto& a, const auto& b) { return a.score > b.score; });

    // Display results with categories
    if (BeginTabBar("CommandCategories")) {
      if (BeginTabItem(
              absl::StrFormat("%s All Commands", ICON_MD_LIST).c_str())) {
        if (gui::LayoutHelpers::BeginTableWithTheming(
                "CommandPaletteTable", 4,
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp,
                ImVec2(0, -30))) {
          TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch, 0.45f);
          TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 0.2f);
          TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, 0.2f);
          TableSetupColumn("Score", ImGuiTableColumnFlags_WidthStretch, 0.15f);
          TableHeadersRow();

          for (size_t i = 0; i < scored_commands.size(); ++i) {
            const auto& cmd = scored_commands[i];

            TableNextRow();
            TableNextColumn();

            PushID(static_cast<int>(i));
            bool is_selected =
                (static_cast<int>(i) == command_palette_selected_idx_);
            if (Selectable(cmd.name.c_str(), is_selected,
                           ImGuiSelectableFlags_SpanAllColumns)) {
              command_palette_selected_idx_ = i;
              if (cmd.callback) {
                cmd.callback();
                show_command_palette_ = false;
                // Record usage for frecency
                command_palette_.RecordUsage(cmd.name);
              }
            }
            PopID();

            TableNextColumn();
            PushStyleColor(ImGuiCol_Text,
                           gui::ConvertColorToImVec4(theme.text_secondary));
            Text("%s", cmd.category.c_str());
            PopStyleColor();

            TableNextColumn();
            PushStyleColor(ImGuiCol_Text,
                           gui::ConvertColorToImVec4(theme.text_secondary));
            Text("%s", cmd.shortcut.c_str());
            PopStyleColor();

            TableNextColumn();
            PushStyleColor(ImGuiCol_Text,
                             gui::ConvertColorToImVec4(theme.text_disabled));
            Text("%d", cmd.score);
            PopStyleColor();
          }

          gui::LayoutHelpers::EndTableWithTheming();
        }
        EndTabItem();
      }

      if (BeginTabItem(absl::StrFormat("%s Recent", ICON_MD_HISTORY).c_str())) {
        auto recent = command_palette_.GetRecentCommands(10);
        if (recent.empty()) {
          Text("No recent commands yet.");
        } else {
          for (const auto& entry : recent) {
            if (Selectable(entry.name.c_str())) {
              if (entry.callback) {
                entry.callback();
                show_command_palette_ = false;
                command_palette_.RecordUsage(entry.name);
              }
            }
          }
        }
        EndTabItem();
      }

      if (BeginTabItem(absl::StrFormat("%s Frequent", ICON_MD_STAR).c_str())) {
        auto frequent = command_palette_.GetFrequentCommands(10);
        if (frequent.empty()) {
          Text("No frequently used commands yet.");
        } else {
          for (const auto& entry : frequent) {
            if (Selectable(absl::StrFormat("%s (%d uses)", entry.name,
                                           entry.usage_count).c_str())) {
              if (entry.callback) {
                entry.callback();
                show_command_palette_ = false;
                command_palette_.RecordUsage(entry.name);
              }
            }
          }
        }
        EndTabItem();
      }

      EndTabBar();
    }

    // Status bar with tips
    Separator();
    Text("%s %zu commands | Score: fuzzy match", ICON_MD_INFO,
         scored_commands.size());
    SameLine();
    PushStyleColor(ImGuiCol_Text,
                   gui::ConvertColorToImVec4(theme.text_disabled));
    Text("| ↑↓=Navigate | Enter=Execute | Esc=Close");
    PopStyleColor();
  }
  End();

  // Update visibility state
  if (!show_palette) {
    show_command_palette_ = false;
  }
}

void UICoordinator::InitializeCommandPalette(size_t session_id) {
  command_palette_.Clear();

  // Register panel commands
  command_palette_.RegisterPanelCommands(&panel_manager_, session_id);

  // Register editor switch commands
  command_palette_.RegisterEditorCommands(
      [this](const std::string& category) {
        auto type = EditorRegistry::GetEditorTypeFromCategory(category);
        if (type != EditorType::kSettings) {  // kSettings is used as "unknown"
          editor_registry_.SwitchToEditor(type);
        }
      });

  // Register layout preset commands
  command_palette_.RegisterLayoutCommands([this](const std::string& preset) {
    // TODO: Implement layout preset application via LayoutManager
    toast_manager_.Show(
        absl::StrFormat("Layout preset '%s' (coming soon)", preset),
        ToastType::kInfo);
  });

  command_palette_initialized_ = true;
}

void UICoordinator::RefreshCommandPalette(size_t session_id) {
  InitializeCommandPalette(session_id);
}

void UICoordinator::DrawGlobalSearch() {
  if (!show_global_search_)
    return;

  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  bool show_search = true;
  if (ImGui::Begin(
          absl::StrFormat("%s Global Search", ICON_MD_MANAGE_SEARCH).c_str(),
          &show_search, ImGuiWindowFlags_NoCollapse)) {
    // Enhanced search input with focus management
    ImGui::SetNextItemWidth(-100);
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }

    bool input_changed = ImGui::InputTextWithHint(
        "##global_query",
        absl::StrFormat("%s Search everything...", ICON_MD_SEARCH).c_str(),
        global_search_query_, IM_ARRAYSIZE(global_search_query_));

    ImGui::SameLine();
    if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
      global_search_query_[0] = '\0';
      input_changed = true;
    }

    ImGui::Separator();

    // Tabbed search results for better organization
    if (ImGui::BeginTabBar("SearchResultTabs")) {
      // Recent Files Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Recent Files", ICON_MD_HISTORY).c_str())) {
        auto& manager = project::RecentFilesManager::GetInstance();
        auto recent_files = manager.GetRecentFiles();

        if (ImGui::BeginTable("RecentFilesTable", 3,
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                  80.0f);
          ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                  100.0f);
          ImGui::TableHeadersRow();

          for (const auto& file : recent_files) {
            if (global_search_query_[0] != '\0' &&
                file.find(global_search_query_) == std::string::npos)
              continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", util::GetFileName(file).c_str());

            ImGui::TableNextColumn();
            std::string ext = util::GetFileExtension(file);
            if (ext == "sfc" || ext == "smc") {
              ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s ROM",
                                 ICON_MD_VIDEOGAME_ASSET);
            } else if (ext == "yaze") {
              ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.8f, 1.0f), "%s Project",
                                 ICON_MD_FOLDER);
            } else {
              ImGui::Text("%s File", ICON_MD_DESCRIPTION);
            }

            ImGui::TableNextColumn();
            ImGui::PushID(file.c_str());
            if (ImGui::Button("Open")) {
              auto status = editor_manager_->OpenRomOrProject(file);
              if (!status.ok()) {
                toast_manager_.Show(
                    absl::StrCat("Failed to open: ", status.message()),
                    ToastType::kError);
              }
              SetGlobalSearchVisible(false);
            }
            ImGui::PopID();
          }

          ImGui::EndTable();
        }
        ImGui::EndTabItem();
      }

      // Labels Tab (only if ROM is loaded)
      auto* current_rom = editor_manager_->GetCurrentRom();
      if (current_rom && current_rom->resource_label()) {
        if (ImGui::BeginTabItem(
                absl::StrFormat("%s Labels", ICON_MD_LABEL).c_str())) {
          auto& labels = current_rom->resource_label()->labels_;

          if (ImGui::BeginTable("LabelsTable", 3,
                                ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                    100.0f);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch,
                                    0.4f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                                    0.6f);
            ImGui::TableHeadersRow();

            for (const auto& type_pair : labels) {
              for (const auto& kv : type_pair.second) {
                if (global_search_query_[0] != '\0' &&
                    kv.first.find(global_search_query_) == std::string::npos &&
                    kv.second.find(global_search_query_) == std::string::npos)
                  continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", type_pair.first.c_str());

                ImGui::TableNextColumn();
                if (ImGui::Selectable(kv.first.c_str(), false,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                  // Future: navigate to related editor/location
                }

                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", kv.second.c_str());
              }
            }

            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }
      }

      // Sessions Tab
      if (session_coordinator_.GetActiveSessionCount() > 1) {
        if (ImGui::BeginTabItem(
                absl::StrFormat("%s Sessions", ICON_MD_TAB).c_str())) {
          ImGui::Text("Search and switch between active sessions:");

          for (size_t i = 0; i < session_coordinator_.GetTotalSessionCount();
               ++i) {
            std::string session_info =
                session_coordinator_.GetSessionDisplayName(i);
            if (session_info == "[CLOSED SESSION]")
              continue;

            if (global_search_query_[0] != '\0' &&
                session_info.find(global_search_query_) == std::string::npos)
              continue;

            bool is_current =
                (i == session_coordinator_.GetActiveSessionIndex());
            if (is_current) {
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            }

            if (ImGui::Selectable(absl::StrFormat("%s %s %s", ICON_MD_TAB,
                                                  session_info.c_str(),
                                                  is_current ? "(Current)" : "")
                                      .c_str())) {
              if (!is_current) {
                editor_manager_->SwitchToSession(i);
                SetGlobalSearchVisible(false);
              }
            }

            if (is_current) {
              ImGui::PopStyleColor();
            }
          }
          ImGui::EndTabItem();
        }
      }

      ImGui::EndTabBar();
    }

    // Status bar
    ImGui::Separator();
    ImGui::Text("%s Global search across all YAZE data", ICON_MD_INFO);
  }
  ImGui::End();

  // Update visibility state
  if (!show_search) {
    SetGlobalSearchVisible(false);
  }
}

// ============================================================================
// Startup Surface Management (Single Source of Truth)
// ============================================================================

void UICoordinator::SetStartupSurface(StartupSurface surface) {
  StartupSurface old_surface = current_startup_surface_;
  current_startup_surface_ = surface;

  // Log state transitions for debugging
  const char* surface_names[] = {"Welcome", "Dashboard", "Editor"};
  LOG_INFO("UICoordinator", "Startup surface: %s -> %s",
           surface_names[static_cast<int>(old_surface)],
           surface_names[static_cast<int>(surface)]);

  // Update dependent visibility flags
  switch (surface) {
    case StartupSurface::kWelcome:
      show_welcome_screen_ = true;
      show_editor_selection_ = false;  // Dashboard hidden
      // Activity Bar will be hidden (checked via ShouldShowActivityBar)
      break;
    case StartupSurface::kDashboard:
      show_welcome_screen_ = false;
      show_editor_selection_ = true;  // Dashboard shown
      break;
    case StartupSurface::kEditor:
      show_welcome_screen_ = false;
      show_editor_selection_ = false;  // Dashboard hidden
      break;
  }
}

bool UICoordinator::ShouldShowWelcome() const {
  // Respect CLI overrides
  if (welcome_behavior_override_ == StartupVisibility::kHide) {
    return false;
  }
  if (welcome_behavior_override_ == StartupVisibility::kShow) {
    return true;
  }

  // Default: show welcome only when in welcome state and not manually closed
  return current_startup_surface_ == StartupSurface::kWelcome &&
         !welcome_screen_manually_closed_;
}

bool UICoordinator::ShouldShowDashboard() const {
  // Respect CLI overrides
  if (dashboard_behavior_override_ == StartupVisibility::kHide) {
    return false;
  }
  if (dashboard_behavior_override_ == StartupVisibility::kShow) {
    return true;
  }

  // Default: show dashboard only when in dashboard state
  return current_startup_surface_ == StartupSurface::kDashboard;
}

bool UICoordinator::ShouldShowActivityBar() const {
  // Activity Bar hidden on cold start (welcome screen)
  // Only show after ROM is loaded
  if (current_startup_surface_ == StartupSurface::kWelcome) {
    return false;
  }

  // Check if ROM is actually loaded
  if (editor_manager_) {
    auto* current_rom = editor_manager_->GetCurrentRom();
    if (!current_rom || !current_rom->is_loaded()) {
      return false;
    }
  }

  return true;
}

}  // namespace editor
}  // namespace yaze

#ifndef YAZE_APP_EDITOR_MENU_RIGHT_DRAWER_MANAGER_H_
#define YAZE_APP_EDITOR_MENU_RIGHT_DRAWER_MANAGER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "app/editor/editor.h"
#include "app/gui/core/ui_config.h"
#include "imgui/imgui.h"

namespace yaze {

class Rom;

namespace project {
struct YazeProject;
}

namespace core {
class VersionManager;
}

namespace editor {

// Forward declarations
class ProposalDrawer;
class ToastManager;
class AgentChat;
class SettingsPanel;
class SelectionPropertiesPanel;
class ProjectManagementPanel;
class ShortcutManager;

/**
 * @class RightDrawerManager
 * @brief Manages right-side sliding drawers for agent chat, proposals, settings
 *
 * Provides a unified drawer system on the right side of the application that:
 * - Slides in/out from the right edge
 * - Shifts the main docking space when expanded
 * - Supports multiple drawer types (agent chat, proposals, settings)
 * - Only one drawer can be active at a time
 *
 * Usage:
 * ```cpp
 * RightDrawerManager drawer_manager;
 * drawer_manager.SetAgentChat(&agent_chat);
 * drawer_manager.SetProposalDrawer(&proposal_drawer);
 * drawer_manager.ToggleDrawer(DrawerType::kAgentChat);
 * drawer_manager.Draw();
 * ```
 */
class RightDrawerManager {
 public:
  struct ToolOutputActions {
    std::function<void(const std::string&)> on_open_reference;
    std::function<void(uint32_t)> on_open_address;
    std::function<void(uint32_t)> on_open_lookup;
  };

  enum class DrawerType {
    kNone = 0,
    kAgentChat,
    kProposals,
    kSettings,
    kHelp,
    kNotifications,  // Full notification history panel
    kProperties,     // Full-editing properties panel
    kProject,        // Project management panel
    kToolOutput      // Tool/query results panel
  };
  using PanelType = DrawerType;

  RightDrawerManager() = default;
  ~RightDrawerManager() = default;

  // Non-copyable
  RightDrawerManager(const RightDrawerManager&) = delete;
  RightDrawerManager& operator=(const RightDrawerManager&) = delete;

  // ============================================================================
  // Configuration
  // ============================================================================

  void SetAgentChat(AgentChat* chat) { agent_chat_ = chat; }
  void SetProposalDrawer(ProposalDrawer* drawer) { proposal_drawer_ = drawer; }
  void SetSettingsPanel(SettingsPanel* panel) { settings_panel_ = panel; }
  void SetShortcutManager(ShortcutManager* manager) {
    shortcut_manager_ = manager;
  }
  void SetPropertiesPanel(SelectionPropertiesPanel* panel) {
    properties_panel_ = panel;
  }
  void SetProjectManagementPanel(ProjectManagementPanel* panel) {
    project_panel_ = panel;
  }
  void SetToastManager(ToastManager* manager) { toast_manager_ = manager; }
  void SetRom(Rom* rom) { rom_ = rom; }
  void SetToolOutput(std::string title, std::string query, std::string content,
                     ToolOutputActions actions = {});
  const std::string& tool_output_title() const { return tool_output_title_; }
  const std::string& tool_output_query() const { return tool_output_query_; }
  const std::string& tool_output_content() const {
    return tool_output_content_;
  }

  /**
   * @brief Set the active editor for context-aware help content
   * @param type The currently active editor type
   */
  void SetActiveEditor(EditorType type) { active_editor_type_ = type; }

  // ============================================================================
  // Panel Control
  // ============================================================================

  /**
   * @brief Toggle a specific drawer on/off
   * @param type Drawer type to toggle
   *
   * If the drawer is already active, it will be closed.
   * If another drawer is active, it will be closed and this one opened.
   */
  void ToggleDrawer(DrawerType type);
  [[deprecated("Use ToggleDrawer() instead.")]]
  void TogglePanel(PanelType type) {
    ToggleDrawer(type);
  }

  /**
   * @brief Open a specific drawer
   * @param type Drawer type to open
   */
  void OpenDrawer(DrawerType type);
  [[deprecated("Use OpenDrawer() instead.")]]
  void OpenPanel(PanelType type) {
    OpenDrawer(type);
  }

  /**
   * @brief Close the currently active drawer
   */
  void CloseDrawer();
  [[deprecated("Use CloseDrawer() instead.")]]
  void ClosePanel() {
    CloseDrawer();
  }

  /**
   * @brief Cycle to the next/previous right drawer in header order.
   * @param direction Positive for next, negative for previous.
   */
  void CycleDrawer(int direction);
  void CycleToNextDrawer() { CycleDrawer(1); }
  void CycleToPreviousDrawer() { CycleDrawer(-1); }
  [[deprecated("Use CycleDrawer() instead.")]]
  void CyclePanel(int direction) {
    CycleDrawer(direction);
  }
  [[deprecated("Use CycleToNextDrawer() instead.")]]
  void CycleToNextPanel() {
    CycleToNextDrawer();
  }
  [[deprecated("Use CycleToPreviousDrawer() instead.")]]
  void CycleToPreviousPanel() {
    CycleToPreviousDrawer();
  }

  /**
   * @brief Snap transient animations when host visibility changes.
   *
   * This is used for OS space switches / focus loss so partially animated
   * panel bitmaps do not linger when returning to the app.
   */
  void OnHostVisibilityChanged(bool visible);

  /**
   * @brief Check if any drawer is currently expanded (or animating closed)
   */
  bool IsDrawerExpanded() const;
  [[deprecated("Use IsDrawerExpanded() instead.")]]
  bool IsPanelExpanded() const {
    return IsDrawerExpanded();
  }

  /**
   * @brief Get the currently active drawer type
   */
  DrawerType GetActiveDrawer() const { return active_panel_; }
  [[deprecated("Use GetActiveDrawer() instead.")]]
  PanelType GetActivePanel() const {
    return GetActiveDrawer();
  }

  /**
   * @brief Check if a specific drawer is active
   */
  bool IsDrawerActive(DrawerType type) const { return active_panel_ == type; }
  [[deprecated("Use IsDrawerActive() instead.")]]
  bool IsPanelActive(PanelType type) const {
    return IsDrawerActive(type);
  }

  // ============================================================================
  // Dimensions
  // ============================================================================

  /**
   * @brief Get the width of the drawer when expanded
   */
  float GetDrawerWidth() const;
  [[deprecated("Use GetDrawerWidth() instead.")]]
  float GetPanelWidth() const {
    return GetDrawerWidth();
  }

  /**
   * @brief Get the width of the collapsed panel strip (toggle buttons)
   */
  static constexpr float GetCollapsedWidth() { return 0.0f; }

  /**
   * @brief Set drawer width for a specific drawer type
   */
  void SetDrawerWidth(DrawerType type, float width);
  [[deprecated("Use SetDrawerWidth() instead.")]]
  void SetPanelWidth(PanelType type, float width) {
    SetDrawerWidth(type, width);
  }

  /**
   * @brief Reset all drawer widths to their defaults
   */
  void ResetDrawerWidths();
  [[deprecated("Use ResetDrawerWidths() instead.")]]
  void ResetPanelWidths() {
    ResetDrawerWidths();
  }

  /**
   * @brief Get the default width for a specific drawer type
   * @param type The drawer type
   * @param editor The optional editor type for context-aware sizing
   * @return Default width in logical pixels
   */
  static float GetDefaultDrawerWidth(DrawerType type,
                                     EditorType editor = EditorType::kUnknown);
  [[deprecated("Use GetDefaultDrawerWidth() instead.")]]
  static float GetDefaultPanelWidth(PanelType type,
                                    EditorType editor = EditorType::kUnknown) {
    return GetDefaultDrawerWidth(type, editor);
  }

  struct PanelSizeLimits {
    float min_width = 280.0f;
    float max_width_ratio = gui::UIConfig::kMaxPanelWidthRatio;
  };

  /**
   * @brief Set sizing constraints for an individual right panel.
   */
  void SetPanelSizeLimits(PanelType type, const PanelSizeLimits& limits);
  PanelSizeLimits GetPanelSizeLimits(PanelType type) const;
  void SetDrawerSizeLimits(DrawerType type, const PanelSizeLimits& limits) {
    SetPanelSizeLimits(type, limits);
  }
  PanelSizeLimits GetDrawerSizeLimits(DrawerType type) const {
    return GetPanelSizeLimits(type);
  }

  /**
   * @brief Persist/restore per-drawer widths for user settings.
   */
  std::unordered_map<std::string, float> SerializeDrawerWidths() const;
  void RestoreDrawerWidths(
      const std::unordered_map<std::string, float>& widths);
  [[deprecated("Use SerializeDrawerWidths() instead.")]]
  std::unordered_map<std::string, float> SerializePanelWidths() const {
    return SerializeDrawerWidths();
  }
  [[deprecated("Use RestoreDrawerWidths() instead.")]]
  void RestorePanelWidths(
      const std::unordered_map<std::string, float>& widths) {
    RestoreDrawerWidths(widths);
  }

  void SetDrawerWidthChangedCallback(
      std::function<void(DrawerType, float)> callback) {
    on_panel_width_changed_ = std::move(callback);
  }
  [[deprecated("Use SetDrawerWidthChangedCallback() instead.")]]
  void SetPanelWidthChangedCallback(
      std::function<void(PanelType, float)> callback) {
    on_panel_width_changed_ = std::move(callback);
  }

  // ============================================================================
  // Rendering
  // ============================================================================

  /**
   * @brief Draw the drawer and its contents
   *
   * Should be called after the main docking space is drawn.
   * The drawer will position itself on the right edge.
   */
  void Draw();

  /**
   * @brief Draw drawer toggle buttons for the status cluster
   *
   * Returns true if any button was clicked.
   */
  bool DrawDrawerToggleButtons();
  [[deprecated("Use DrawDrawerToggleButtons() instead.")]]
  bool DrawPanelToggleButtons() {
    return DrawDrawerToggleButtons();
  }

  // ============================================================================
  // Panel-specific accessors
  // ============================================================================

  AgentChat* agent_chat() const { return agent_chat_; }
  ProposalDrawer* proposal_drawer() const { return proposal_drawer_; }
  SettingsPanel* settings_panel() const { return settings_panel_; }
  SelectionPropertiesPanel* properties_panel() const {
    return properties_panel_;
  }
  ProjectManagementPanel* project_panel() const { return project_panel_; }

 private:
  void DrawPanelHeader(const char* title, const char* icon);
  void DrawAgentChatPanel();
  void DrawProposalsPanel();
  void DrawSettingsPanel();
  void DrawHelpPanel();
  void DrawNotificationsPanel();
  void DrawPropertiesPanel();
  void DrawProjectPanel();
  void DrawToolOutputPanel();
  bool DrawAgentQuickActions();

  // Help panel helpers for context-aware content
  void DrawEditorContextHeader();
  void DrawGlobalShortcuts();
  void DrawEditorSpecificShortcuts();
  void DrawEditorSpecificHelp();
  void DrawQuickActionButtons();
  void DrawAboutSection();

  // Styling helpers for consistent panel UI
  bool BeginPanelSection(const char* label, const char* icon = nullptr,
                         bool default_open = true);
  void EndPanelSection();
  void DrawPanelDivider();
  void DrawPanelLabel(const char* label);
  void DrawPanelValue(const char* label, const char* value);
  void DrawPanelDescription(const char* text);
  std::string GetShortcutLabel(const std::string& action,
                               const std::string& fallback) const;
  void DrawShortcutRow(const std::string& action, const char* description,
                       const std::string& fallback);
  float GetConfiguredPanelWidth(PanelType type) const;
  float GetClampedPanelWidth(PanelType type, float viewport_width) const;
  void NotifyPanelWidthChanged(PanelType type, float width);
  static std::string PanelTypeKey(PanelType type);

  // Active panel
  PanelType active_panel_ = PanelType::kNone;

  // Active editor for context-aware help
  EditorType active_editor_type_ = EditorType::kUnknown;

  // Panel widths (customizable per panel type) - consistent sizing
  float agent_chat_width_ = gui::UIConfig::kPanelWidthAgentChat;
  float proposals_width_ = gui::UIConfig::kPanelWidthProposals;
  float settings_width_ = gui::UIConfig::kPanelWidthSettings;
  float help_width_ = gui::UIConfig::kPanelWidthHelp;
  float notifications_width_ = gui::UIConfig::kPanelWidthNotifications;
  float properties_width_ = gui::UIConfig::kPanelWidthProperties;
  float project_width_ = gui::UIConfig::kPanelWidthProject;
  float tool_output_width_ = 460.0f;

  // Component references (not owned)
  AgentChat* agent_chat_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  SettingsPanel* settings_panel_ = nullptr;
  ShortcutManager* shortcut_manager_ = nullptr;
  SelectionPropertiesPanel* properties_panel_ = nullptr;
  ProjectManagementPanel* project_panel_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  Rom* rom_ = nullptr;
  std::string tool_output_title_;
  std::string tool_output_query_;
  std::string tool_output_content_;
  ToolOutputActions tool_output_actions_;

  // Properties panel lock state
  bool properties_locked_ = false;

  // Animation state
  float panel_animation_ = 0.0f;   // 0.0 = fully closed, 1.0 = fully open
  float animation_target_ = 0.0f;  // Target value for lerp
  bool animating_ = false;
  bool closing_ = false;                        // True during close animation
  PanelType closing_panel_ = PanelType::kNone;  // Panel being animated closed
  std::unordered_map<std::string, PanelSizeLimits> panel_size_limits_;
  std::function<void(PanelType, float)> on_panel_width_changed_;
};

/**
 * @brief Get the name of a panel type
 */
const char* GetPanelTypeName(RightDrawerManager::PanelType type);

/**
 * @brief Get the icon for a panel type
 */
const char* GetPanelTypeIcon(RightDrawerManager::PanelType type);

inline const char* GetDrawerTypeName(RightDrawerManager::DrawerType type) {
  return GetPanelTypeName(type);
}

inline const char* GetDrawerTypeIcon(RightDrawerManager::DrawerType type) {
  return GetPanelTypeIcon(type);
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_RIGHT_DRAWER_MANAGER_H_

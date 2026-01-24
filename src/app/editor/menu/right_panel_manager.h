#ifndef YAZE_APP_EDITOR_MENU_RIGHT_PANEL_MANAGER_H_
#define YAZE_APP_EDITOR_MENU_RIGHT_PANEL_MANAGER_H_

#include <functional>
#include <string>

#include "app/editor/editor.h"
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
 * @class RightPanelManager
 * @brief Manages right-side sliding panels for agent chat, proposals, settings
 *
 * Provides a unified panel system on the right side of the application that:
 * - Slides in/out from the right edge
 * - Shifts the main docking space when expanded
 * - Supports multiple panel types (agent chat, proposals, settings)
 * - Only one panel can be active at a time
 *
 * Usage:
 * ```cpp
 * RightPanelManager panel_manager;
 * panel_manager.SetAgentChat(&agent_chat);
 * panel_manager.SetProposalDrawer(&proposal_drawer);
 * panel_manager.TogglePanel(PanelType::kAgentChat);
 * panel_manager.Draw();
 * ```
 */
class RightPanelManager {
 public:
  enum class PanelType {
    kNone = 0,
    kAgentChat,
    kProposals,
    kSettings,
    kHelp,
    kNotifications,  // Full notification history panel
    kProperties,     // Full-editing properties panel
    kProject         // Project management panel
  };

  RightPanelManager() = default;
  ~RightPanelManager() = default;

  // Non-copyable
  RightPanelManager(const RightPanelManager&) = delete;
  RightPanelManager& operator=(const RightPanelManager&) = delete;

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

  /**
   * @brief Set the active editor for context-aware help content
   * @param type The currently active editor type
   */
  void SetActiveEditor(EditorType type) { active_editor_type_ = type; }

  // ============================================================================
  // Panel Control
  // ============================================================================

  /**
   * @brief Toggle a specific panel on/off
   * @param type Panel type to toggle
   *
   * If the panel is already active, it will be closed.
   * If another panel is active, it will be closed and this one opened.
   */
  void TogglePanel(PanelType type);

  /**
   * @brief Open a specific panel
   * @param type Panel type to open
   */
  void OpenPanel(PanelType type);

  /**
   * @brief Close the currently active panel
   */
  void ClosePanel();

  /**
   * @brief Check if any panel is currently expanded
   */
  bool IsPanelExpanded() const { return active_panel_ != PanelType::kNone; }

  /**
   * @brief Get the currently active panel type
   */
  PanelType GetActivePanel() const { return active_panel_; }

  /**
   * @brief Check if a specific panel is active
   */
  bool IsPanelActive(PanelType type) const { return active_panel_ == type; }

  // ============================================================================
  // Dimensions
  // ============================================================================

  /**
   * @brief Get the width of the panel when expanded
   */
  float GetPanelWidth() const;

  /**
   * @brief Get the width of the collapsed panel strip (toggle buttons)
   */
  static constexpr float GetCollapsedWidth() { return 0.0f; }

  /**
   * @brief Set panel width for a specific panel type
   */
  void SetPanelWidth(PanelType type, float width);

  // ============================================================================
  // Rendering
  // ============================================================================

  /**
   * @brief Draw the panel and its contents
   *
   * Should be called after the main docking space is drawn.
   * The panel will position itself on the right edge.
   */
  void Draw();

  /**
   * @brief Draw toggle buttons for the status cluster
   *
   * Returns true if any button was clicked.
   */
  bool DrawPanelToggleButtons();

  // ============================================================================
  // Panel-specific accessors
  // ============================================================================

  AgentChat* agent_chat() const { return agent_chat_; }
  ProposalDrawer* proposal_drawer() const { return proposal_drawer_; }
  SettingsPanel* settings_panel() const { return settings_panel_; }
  SelectionPropertiesPanel* properties_panel() const { return properties_panel_; }
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

  // Active panel
  PanelType active_panel_ = PanelType::kNone;

  // Active editor for context-aware help
  EditorType active_editor_type_ = EditorType::kUnknown;

  // Panel widths (customizable per panel type) - consistent sizing
  float agent_chat_width_ = 420.0f;  // Match proposals for consistency
  float proposals_width_ = 420.0f;
  float settings_width_ = 420.0f;    // Same width for unified look
  float help_width_ = 380.0f;        // Wider for better readability
  float notifications_width_ = 420.0f;
  float properties_width_ = 320.0f;  // Narrower for properties
  float project_width_ = 380.0f;     // Project management panel

  // Component references (not owned)
  AgentChat* agent_chat_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  SettingsPanel* settings_panel_ = nullptr;
  ShortcutManager* shortcut_manager_ = nullptr;
  SelectionPropertiesPanel* properties_panel_ = nullptr;
  ProjectManagementPanel* project_panel_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  Rom* rom_ = nullptr;

  // Animation state
  float panel_animation_ = 0.0f;
  bool animating_ = false;
};

/**
 * @brief Get the name of a panel type
 */
const char* GetPanelTypeName(RightPanelManager::PanelType type);

/**
 * @brief Get the icon for a panel type
 */
const char* GetPanelTypeIcon(RightPanelManager::PanelType type);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_RIGHT_PANEL_MANAGER_H_

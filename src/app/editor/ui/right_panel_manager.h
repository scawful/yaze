#ifndef YAZE_APP_EDITOR_UI_RIGHT_PANEL_MANAGER_H_
#define YAZE_APP_EDITOR_UI_RIGHT_PANEL_MANAGER_H_

#include <functional>
#include <string>

#include "imgui/imgui.h"

namespace yaze {

class Rom;

namespace editor {

// Forward declarations
class ProposalDrawer;
class ToastManager;
class AgentChatWidget;
class SettingsEditor;

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
 * panel_manager.SetAgentChatWidget(&agent_chat);
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
    kHelp
  };

  RightPanelManager() = default;
  ~RightPanelManager() = default;

  // Non-copyable
  RightPanelManager(const RightPanelManager&) = delete;
  RightPanelManager& operator=(const RightPanelManager&) = delete;

  // ============================================================================
  // Configuration
  // ============================================================================

  void SetAgentChatWidget(AgentChatWidget* widget) { agent_chat_widget_ = widget; }
  void SetProposalDrawer(ProposalDrawer* drawer) { proposal_drawer_ = drawer; }
  void SetSettingsEditor(SettingsEditor* editor) { settings_editor_ = editor; }
  void SetToastManager(ToastManager* manager) { toast_manager_ = manager; }
  void SetRom(Rom* rom) { rom_ = rom; }

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

  AgentChatWidget* agent_chat_widget() const { return agent_chat_widget_; }
  ProposalDrawer* proposal_drawer() const { return proposal_drawer_; }
  SettingsEditor* settings_editor() const { return settings_editor_; }

 private:
  void DrawPanelHeader(const char* title, const char* icon);
  void DrawAgentChatPanel();
  void DrawProposalsPanel();
  void DrawSettingsPanel();
  void DrawHelpPanel();

  // Active panel
  PanelType active_panel_ = PanelType::kNone;

  // Panel widths (customizable per panel type)
  float agent_chat_width_ = 380.0f;
  float proposals_width_ = 420.0f;
  float settings_width_ = 480.0f;
  float help_width_ = 350.0f;

  // Component references (not owned)
  AgentChatWidget* agent_chat_widget_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  SettingsEditor* settings_editor_ = nullptr;
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

#endif  // YAZE_APP_EDITOR_UI_RIGHT_PANEL_MANAGER_H_


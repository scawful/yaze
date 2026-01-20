#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_

#include <string>

namespace yaze {
namespace editor {

/**
 * @enum PanelCategory
 * @brief Defines lifecycle behavior for editor panels
 *
 * Panels are categorized by how they behave when switching between editors:
 * - EditorBound: Hidden when switching away from parent editor (default)
 * - Persistent: Remains visible across all editor switches
 * - CrossEditor: User can "pin" to make persistent (opt-in by user)
 */
enum class PanelCategory {
  EditorBound,  ///< Hidden when switching editors (default)
  Persistent,   ///< Always visible once shown
  CrossEditor   ///< User can pin to persist across editors
};

/**
 * @enum PanelScope
 * @brief Defines whether a panel is session-scoped or global
 *
 * Session-scoped panels are registered per session (with optional prefixing).
 * Global panels share a single descriptor across all sessions.
 */
enum class PanelScope {
  kSession,
  kGlobal
};

/**
 * @class EditorPanel
 * @brief Base interface for all logical panel components
 *
 * EditorPanel represents a logical UI component that draws content within
 * a panel window. This is distinct from PanelWindow (the ImGui wrapper that
 * draws the window chrome/title bar).
 *
 * The separation allows:
 * - PanelManager to handle window creation/visibility centrally
 * - Panel components to focus purely on content drawing
 * - Consistent window behavior across all editors
 *
 * @section Usage Example
 * ```cpp
 * class UsageStatisticsPanel : public EditorPanel {
 *  public:
 *   std::string GetId() const override { return "overworld.usage_stats"; }
 *   std::string GetDisplayName() const override { return "Usage Statistics"; }
 *   std::string GetIcon() const override { return ICON_MD_ANALYTICS; }
 *   std::string GetEditorCategory() const override { return "Overworld"; }
 *
 *   void Draw(bool* p_open) override {
 *     // Draw your content here - no ImGui::Begin/End needed
 *     // PanelManager handles the window wrapper
 *     DrawUsageGrid();
 *     DrawUsageStates();
 *   }
 * };
 * ```
 *
 * @see PanelWindow - The ImGui window wrapper (draws chrome)
 * @see PanelManager - Central registry that manages panel lifecycle
 * @see PanelDescriptor - Metadata struct for panel registration
 */
class EditorPanel {
 public:
  virtual ~EditorPanel() = default;

  // ==========================================================================
  // Identity (Required)
  // ==========================================================================

  /**
   * @brief Unique identifier for this panel
   * @return Panel ID in format "{category}.{name}" (e.g., "dungeon.room_selector")
   *
   * IDs should be:
   * - Lowercase with underscores
   * - Prefixed with editor category
   * - Unique across all panels
   */
  virtual std::string GetId() const = 0;

  /**
   * @brief Human-readable name shown in menus and title bars
   * @return Display name (e.g., "Room Selector")
   */
  virtual std::string GetDisplayName() const = 0;

  /**
   * @brief Material Design icon for this panel
   * @return Icon constant (e.g., ICON_MD_LIST)
   */
  virtual std::string GetIcon() const = 0;

  /**
   * @brief Editor category this panel belongs to
   * @return Category name matching EditorType (e.g., "Dungeon", "Overworld")
   */
  virtual std::string GetEditorCategory() const = 0;

  // ==========================================================================
  // Drawing (Required)
  // ==========================================================================

  /**
   * @brief Draw the panel content
   * @param p_open Pointer to visibility flag (nullptr if not closable)
   *
   * Called by PanelManager when the panel is visible.
   * Do NOT call ImGui::Begin/End - the PanelWindow wrapper handles that.
   * Just draw your content directly.
   */
  virtual void Draw(bool* p_open) = 0;

  // ==========================================================================
  // Lifecycle Hooks (Optional)
  // ==========================================================================

  /**
   * @brief Called when panel becomes visible
   *
   * Use this to initialize state, load resources, or start animations.
   * Called after the panel is shown but before first Draw().
   */
  virtual void OnOpen() {}

  /**
   * @brief Called when panel is hidden
   *
   * Use this to cleanup state, release resources, or save state.
   * Called after the panel is hidden.
   */
  virtual void OnClose() {}

  /**
   * @brief Called when panel receives focus
   *
   * Use this to update state based on becoming the active panel.
   */
  virtual void OnFocus() {}

  // ==========================================================================
  // Behavior (Optional)
  // ==========================================================================

  /**
   * @brief Get the lifecycle category for this panel
   * @return PanelCategory determining visibility behavior on editor switch
   *
   * Default is EditorBound (hidden when switching editors).
   */
  virtual PanelCategory GetPanelCategory() const {
    return PanelCategory::EditorBound;
  }

  /**
   * @brief Get the registration scope for this panel
   * @return PanelScope indicating session or global scope
   *
   * Default is session-scoped.
   */
  virtual PanelScope GetScope() const { return PanelScope::kSession; }

  /**
   * @brief Check if this panel is currently enabled
   * @return true if panel can be shown, false if disabled
   *
   * Disabled panels appear grayed out in menus.
   * Override to implement conditional availability (e.g., requires ROM loaded).
   */
  virtual bool IsEnabled() const { return true; }

  /**
   * @brief Get tooltip text when panel is disabled
   * @return Explanation of why panel is disabled (e.g., "Requires ROM to be loaded")
   */
  virtual std::string GetDisabledTooltip() const { return ""; }

  /**
   * @brief Get keyboard shortcut hint for display
   * @return Shortcut string (e.g., "Ctrl+Shift+R")
   */
  virtual std::string GetShortcutHint() const { return ""; }

  /**
   * @brief Get display priority for menu ordering
   * @return Priority value (lower = higher in list, default 50)
   */
  virtual int GetPriority() const { return 50; }

  /**
   * @brief Get preferred width for this panel (optional)
   * @return Preferred width in pixels, or 0 to use default (250px)
   *
   * Override this to specify content-based sizing. For example, a tile
   * selector with 8 tiles at 16px × 2.0 scale would return ~276px.
   */
  virtual float GetPreferredWidth() const { return 0.0f; }

  /**
   * @brief Whether this panel should be visible by default
   * @return true if panel should be visible when editor first opens
   *
   * Override this to set panels as visible by default.
   * Most panels default to hidden to reduce UI clutter.
   */
  virtual bool IsVisibleByDefault() const { return false; }

  // ==========================================================================
  // Relationships (Optional)
  // ==========================================================================

  /**
   * @brief Get parent panel ID for cascade behavior
   * @return Parent panel ID, or empty string if no parent
   *
   * If set, this panel may be automatically closed when parent closes
   * (depending on parent's CascadeCloseChildren() setting).
   */
  virtual std::string GetParentPanelId() const { return ""; }

  /**
   * @brief Whether closing this panel should close child panels
   * @return true to cascade close to children, false to leave children open
   *
   * Only affects panels that have this panel as their parent.
   */
  virtual bool CascadeCloseChildren() const { return false; }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_

#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_COORDINATOR_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_COORDINATOR_H_

#include <functional>
#include <memory>
#include <string>

#include "app/editor/editor.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

// Forward declarations
class PanelManager;
class UICoordinator;
class ToastManager;
class StatusBar;
class RightPanelManager;

/**
 * @class LayoutCoordinator
 * @brief Facade class that coordinates all layout-related operations
 *
 * This class extracts layout logic from EditorManager to reduce cognitive
 * complexity. It provides a unified interface for:
 * - Layout offset calculations (for dockspace margins)
 * - Layout preset application
 * - Layout rebuild handling
 * - Editor layout initialization
 *
 * Dependencies are injected to avoid circular includes.
 */
class LayoutCoordinator {
 public:
  /**
   * @struct Dependencies
   * @brief All dependencies required by LayoutCoordinator
   */
  struct Dependencies {
    LayoutManager* layout_manager = nullptr;
    PanelManager* panel_manager = nullptr;
    UICoordinator* ui_coordinator = nullptr;
    ToastManager* toast_manager = nullptr;
    StatusBar* status_bar = nullptr;
    RightPanelManager* right_panel_manager = nullptr;
  };

  LayoutCoordinator() = default;
  ~LayoutCoordinator() = default;

  /**
   * @brief Initialize with all dependencies
   * @param deps The dependency struct containing all required pointers
   */
  void Initialize(const Dependencies& deps);

  // ==========================================================================
  // Layout Offset Calculations
  // ==========================================================================

  /**
   * @brief Get the left margin needed for sidebar (Activity Bar + Side Panel)
   * @return Float representing pixel offset
   */
  float GetLeftLayoutOffset() const;

  /**
   * @brief Get the right margin needed for panels
   * @return Float representing pixel offset
   */
  float GetRightLayoutOffset() const;

  /**
   * @brief Get the bottom margin needed for status bar
   * @return Float representing pixel offset
   */
  float GetBottomLayoutOffset() const;

  // ==========================================================================
  // Layout Operations
  // ==========================================================================

  /**
   * @brief Reset the workspace layout to defaults
   *
   * Clears all layout initialization flags and requests rebuild.
   * Uses the current editor context to determine which layout to rebuild.
   */
  void ResetWorkspaceLayout();

  /**
   * @brief Apply a named layout preset
   * @param preset_name Name of the preset (e.g., "Minimal", "Developer")
   * @param session_id Current session ID for panel management
   */
  void ApplyLayoutPreset(const std::string& preset_name, size_t session_id);

  /**
   * @brief Reset current editor layout to its default configuration
   * @param editor_type The current editor type
   * @param session_id Current session ID
   */
  void ResetCurrentEditorLayout(EditorType editor_type, size_t session_id);

  // ==========================================================================
  // Layout Rebuild Handling
  // ==========================================================================

  /**
   * @brief Process pending layout rebuild requests
   *
   * Called from the main update loop. Checks if a rebuild was requested
   * and executes it if we're in a valid ImGui frame scope.
   *
   * @param current_editor_type The currently active editor type
   * @param is_emulator_visible Whether the emulator is currently visible
   */
  void ProcessLayoutRebuild(EditorType current_editor_type,
                            bool is_emulator_visible);

  /**
   * @brief Initialize layout for an editor type on first activation
   * @param type The editor type to initialize
   *
   * This is called when switching to an editor for the first time.
   * Uses deferred action to ensure ImGui context is valid.
   */
  void InitializeEditorLayout(EditorType type);

  /**
   * @brief Queue an action to be executed on the next frame
   * @param action The action to queue
   *
   * Used for operations that must be deferred (e.g., layout changes
   * during menu rendering).
   */
  void QueueDeferredAction(std::function<void()> action);

  /**
   * @brief Process all queued deferred actions
   *
   * Should be called at the start of each frame before other updates.
   */
  void ProcessDeferredActions();

  // ==========================================================================
  // Accessors
  // ==========================================================================

  LayoutManager* layout_manager() { return layout_manager_; }
  const LayoutManager* layout_manager() const { return layout_manager_; }

  bool IsInitialized() const { return layout_manager_ != nullptr; }

 private:
  // Dependencies (injected)
  LayoutManager* layout_manager_ = nullptr;
  PanelManager* panel_manager_ = nullptr;
  UICoordinator* ui_coordinator_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  StatusBar* status_bar_ = nullptr;
  RightPanelManager* right_panel_manager_ = nullptr;

  // Deferred action queue
  std::vector<std::function<void()>> deferred_actions_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_COORDINATOR_H_


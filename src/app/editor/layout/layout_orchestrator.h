#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_ORCHESTRATOR_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_ORCHESTRATOR_H_

#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/panel_manager.h"

namespace yaze {
namespace editor {

/**
 * @class LayoutOrchestrator
 * @brief Coordinates between LayoutManager, PanelManager, and LayoutPresets
 *
 * This class unifies the card and layout systems by:
 * 1. Using PanelInfo.window_title for DockBuilder operations
 * 2. Applying layout presets that show/hide cards appropriately
 * 3. Managing the relationship between card IDs and their window titles
 *
 * Usage:
 *   LayoutOrchestrator orchestrator(&layout_manager, &card_registry);
 *   orchestrator.ApplyPreset(EditorType::kDungeon, session_id);
 */
class LayoutOrchestrator {
 public:
  LayoutOrchestrator() = default;
  LayoutOrchestrator(LayoutManager* layout_manager,
                     PanelManager* panel_manager);

  /**
   * @brief Initialize with dependencies
   */
  void Initialize(LayoutManager* layout_manager,
                  PanelManager* panel_manager);

  /**
   * @brief Apply the default layout preset for an editor type
   * @param type The editor type to apply preset for
   * @param session_id Session ID for multi-session support (default = 0)
   *
   * This method:
   * 1. Shows default cards for the editor type
   * 2. Hides optional cards (they can be shown manually)
   * 3. Applies the DockBuilder layout
   */
  void ApplyPreset(EditorType type, size_t session_id = 0);

  /**
   * @brief Apply a named workspace preset (Developer, Designer, Modder)
   * @param preset_name Name of the preset to apply
   * @param session_id Session ID (default = 0)
   */
  void ApplyNamedPreset(const std::string& preset_name,
                        size_t session_id = 0);

  /**
   * @brief Reset to default layout for an editor type
   * @param type The editor type
   * @param session_id Session ID (default = 0)
   */
  void ResetToDefault(EditorType type, size_t session_id = 0);

  /**
   * @brief Get window title for a card from the registry
   * @param card_id The card ID
   * @param session_id Session ID (default = 0)
   * @return Window title string, or empty if not found
   */
  std::string GetWindowTitle(const std::string& card_id,
                             size_t session_id = 0) const;

  /**
   * @brief Get all visible panels for a session
   * @param session_id The session ID
   * @return Vector of visible panel IDs
   */
  std::vector<std::string> GetVisiblePanels(size_t session_id) const;

  /**
   * @brief Show panels specified in a preset
   * @param preset The preset containing panels to show
   * @param session_id Optional session ID
   */
  void ShowPresetPanels(const PanelLayoutPreset& preset,
                        size_t session_id,
                        EditorType editor_type);

  /**
   * @brief Hide all optional panels for an editor type
   * @param type The editor type
   * @param session_id Optional session ID
   */
  void HideOptionalPanels(EditorType type, size_t session_id = 0);

  /**
   * @brief Request layout rebuild on next frame
   */
  void RequestLayoutRebuild();

  /**
   * @brief Get the layout manager
   */
  LayoutManager* layout_manager() { return layout_manager_; }

  /**
   * @brief Get the card registry
   */
  PanelManager* panel_manager() { return panel_manager_; }

  /**
   * @brief Check if orchestrator is properly initialized
   */
  bool IsInitialized() const {
    return layout_manager_ != nullptr && panel_manager_ != nullptr;
  }

 private:
  /**
   * @brief Apply DockBuilder layout for an editor type
   */
  void ApplyDockLayout(EditorType type);

  /**
   * @brief Get prefixed card ID for a session
   */
  std::string GetPrefixedPanelId(const std::string& card_id,
                                size_t session_id) const;

  LayoutManager* layout_manager_ = nullptr;
  PanelManager* panel_manager_ = nullptr;
  bool rebuild_requested_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_ORCHESTRATOR_H_


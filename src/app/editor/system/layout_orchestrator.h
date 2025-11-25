#ifndef YAZE_APP_EDITOR_SYSTEM_LAYOUT_ORCHESTRATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_LAYOUT_ORCHESTRATOR_H_

#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/editor/ui/layout_manager.h"
#include "app/editor/ui/layout_presets.h"

namespace yaze {
namespace editor {

/**
 * @class LayoutOrchestrator
 * @brief Coordinates between LayoutManager, EditorCardRegistry, and LayoutPresets
 *
 * This class unifies the card and layout systems by:
 * 1. Using CardInfo.window_title for DockBuilder operations
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
                     EditorCardRegistry* card_registry);

  /**
   * @brief Initialize with dependencies
   */
  void Initialize(LayoutManager* layout_manager,
                  EditorCardRegistry* card_registry);

  /**
   * @brief Apply the default layout preset for an editor type
   * @param type The editor type to apply preset for
   * @param session_id Optional session ID for multi-session support
   *
   * This method:
   * 1. Shows default cards for the editor type
   * 2. Hides optional cards (they can be shown manually)
   * 3. Applies the DockBuilder layout
   */
  void ApplyPreset(EditorType type, const std::string& session_id = "");

  /**
   * @brief Apply a named workspace preset (Developer, Designer, Modder)
   * @param preset_name Name of the preset to apply
   * @param session_id Optional session ID
   */
  void ApplyNamedPreset(const std::string& preset_name,
                        const std::string& session_id = "");

  /**
   * @brief Reset to default layout for an editor type
   * @param type The editor type
   * @param session_id Optional session ID
   */
  void ResetToDefault(EditorType type, const std::string& session_id = "");

  /**
   * @brief Get window title for a card from the registry
   * @param card_id The card ID
   * @param session_id Optional session ID
   * @return Window title string, or empty if not found
   */
  std::string GetWindowTitle(const std::string& card_id,
                             const std::string& session_id = "") const;

  /**
   * @brief Get all visible cards for a session
   * @param session_id The session ID
   * @return Vector of visible card IDs
   */
  std::vector<std::string> GetVisibleCards(
      const std::string& session_id) const;

  /**
   * @brief Show cards specified in a preset
   * @param preset The preset containing cards to show
   * @param session_id Optional session ID
   */
  void ShowPresetCards(const CardLayoutPreset& preset,
                       const std::string& session_id = "");

  /**
   * @brief Hide all optional cards for an editor type
   * @param type The editor type
   * @param session_id Optional session ID
   */
  void HideOptionalCards(EditorType type, const std::string& session_id = "");

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
  EditorCardRegistry* card_registry() { return card_registry_; }

  /**
   * @brief Check if orchestrator is properly initialized
   */
  bool IsInitialized() const {
    return layout_manager_ != nullptr && card_registry_ != nullptr;
  }

 private:
  /**
   * @brief Apply DockBuilder layout for an editor type
   */
  void ApplyDockLayout(EditorType type);

  /**
   * @brief Get prefixed card ID for a session
   */
  std::string GetPrefixedCardId(const std::string& card_id,
                                const std::string& session_id) const;

  LayoutManager* layout_manager_ = nullptr;
  EditorCardRegistry* card_registry_ = nullptr;
  bool rebuild_requested_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_LAYOUT_ORCHESTRATOR_H_


#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCE_LIST_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCE_LIST_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonEntranceListPanel
 * @brief EditorPanel for entrance list selection
 *
 * This panel provides entrance selection UI for the Dungeon Editor.
 * Selecting an entrance will open/focus the associated room's resource panel.
 *
 * @section Features
 * - Entrance list with search/filter
 * - Entrance properties display
 * - Click to open associated room as resource panel
 *
 * @see DungeonRoomSelector - The underlying component
 * @see EditorPanel - Base interface
 */
class DungeonEntranceListPanel : public EditorPanel {
 public:
  /**
   * @brief Construct a new panel wrapping a DungeonRoomSelector
   * @param selector The room selector component (must outlive this panel)
   * @param on_entrance_selected Callback when entrance is selected
   */
  explicit DungeonEntranceListPanel(
      DungeonRoomSelector* selector,
      std::function<void(int)> on_entrance_selected = nullptr)
      : selector_(selector),
        on_entrance_selected_(std::move(on_entrance_selected)) {
    // Wire up the callback directly to the selector
    if (selector_ && on_entrance_selected_) {
      selector_->SetEntranceSelectedCallback(on_entrance_selected_);
    }
  }

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.entrance_list"; }
  std::string GetDisplayName() const override { return "Entrance List"; }
  std::string GetIcon() const override { return ICON_MD_DOOR_FRONT; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 25; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!selector_) return;

    // Draw just the entrance selector (no tabs)
    selector_->DrawEntranceSelector();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  /**
   * @brief Set the callback for entrance selection events
   * @param callback Function to call when an entrance is selected
   */
  void SetEntranceSelectedCallback(std::function<void(int)> callback) {
    on_entrance_selected_ = std::move(callback);
    if (selector_) {
      selector_->SetEntranceSelectedCallback(on_entrance_selected_);
    }
  }

  /**
   * @brief Get the underlying selector component
   * @return Pointer to DungeonRoomSelector
   */
  DungeonRoomSelector* selector() const { return selector_; }

 private:
  DungeonRoomSelector* selector_ = nullptr;
  std::function<void(int)> on_entrance_selected_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ENTRANCE_LIST_PANEL_H_


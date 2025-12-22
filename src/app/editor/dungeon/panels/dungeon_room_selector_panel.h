#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_SELECTOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_SELECTOR_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomSelectorPanel
 * @brief EditorPanel for room list selection
 *
 * This panel provides room selection UI for the Dungeon Editor.
 * Selecting a room will open/focus the room's resource panel.
 *
 * @section Features
 * - Room list with search/filter
 * - Click to open room as resource panel
 *
 * @see DungeonRoomSelector - The underlying component
 * @see EditorPanel - Base interface
 */
class DungeonRoomSelectorPanel : public EditorPanel {
 public:
  /**
   * @brief Construct a new panel wrapping a DungeonRoomSelector
   * @param selector The room selector component (must outlive this panel)
   * @param on_room_selected Callback when a room is selected (opens resource panel)
   */
  explicit DungeonRoomSelectorPanel(
      DungeonRoomSelector* selector,
      std::function<void(int)> on_room_selected = nullptr)
      : selector_(selector), on_room_selected_(std::move(on_room_selected)) {
    // Wire up the callback directly to the selector
    if (selector_ && on_room_selected_) {
      selector_->SetRoomSelectedCallback(on_room_selected_);
    }
  }

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_selector"; }
  std::string GetDisplayName() const override { return "Room List"; }
  std::string GetIcon() const override { return ICON_MD_LIST; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 20; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!selector_) return;

    // Draw just the room selector (no tabs)
    selector_->DrawRoomSelector();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  /**
   * @brief Set the callback for room selection events
   * @param callback Function to call when a room is selected
   */
  void SetRoomSelectedCallback(std::function<void(int)> callback) {
    on_room_selected_ = std::move(callback);
    if (selector_) {
      selector_->SetRoomSelectedCallback(on_room_selected_);
    }
  }

  /**
   * @brief Get the underlying selector component
   * @return Pointer to DungeonRoomSelector
   */
  DungeonRoomSelector* selector() const { return selector_; }

 private:
  DungeonRoomSelector* selector_ = nullptr;
  std::function<void(int)> on_room_selected_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_SELECTOR_PANEL_H_

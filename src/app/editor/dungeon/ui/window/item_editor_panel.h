#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL.h>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class ItemEditorPanel
 * @brief WindowContent for placing and managing dungeon pot items
 *
 * This panel provides item selection and placement functionality
 * for dungeon rooms, similar to ObjectSelectorContent and SpriteEditorPanel.
 */
class ItemEditorPanel : public WindowContent {
 public:
  ItemEditorPanel(int* current_room_id, DungeonRoomStore* rooms,
                  DungeonCanvasViewer* canvas_viewer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        canvas_viewer_(canvas_viewer) {}

  std::string GetId() const override { return "dungeon.item_editor"; }
  std::string GetDisplayName() const override { return "Item Editor"; }
  std::string GetIcon() const override { return ICON_MD_INVENTORY; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 66; }
  std::string GetWorkflowGroup() const override { return "Editors"; }

  void Draw(bool* p_open) override;
  void DrawInWorkbench();

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { canvas_viewer_ = viewer; }

  void SetItemPlacedCallback(
      std::function<void(const zelda3::PotItem&)> callback) {
    item_placed_callback_ = std::move(callback);
  }
  void SetOpenSelectionInspectorCallback(std::function<void()> callback) {
    open_selection_inspector_callback_ = std::move(callback);
  }

 private:
  void DrawContent(bool show_room_items);
  bool HasMatchingViewer() const;
  bool CanEditRoom() const;
  void DrawPlacementControls();
  void DrawItemSelector(bool show_room_items);
  void DrawRoomItems();
  ImVec4 GetItemTypeColor(int item_id, const AgentUITheme& theme) const;
  const char* GetItemTypeIcon(int item_id) const;

  struct CachedItemPreview {
    std::vector<uint8_t> pixels;
    SDL_Rect bounds{0, 0, 0, 0};
  };

  const CachedItemPreview& GetOrCreateItemPreview(uint8_t item_id,
                                                  const zelda3::Room& room);

  mutable std::unordered_map<uint8_t, CachedItemPreview> item_preview_cache_;
  mutable uint64_t cached_item_graphics_revision_ = 0;

  int* current_room_id_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;

  int selected_item_id_ = 0;
  bool placement_mode_ = false;

  std::function<void(const zelda3::PotItem&)> item_placed_callback_;
  std::function<void()> open_selection_inspector_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_

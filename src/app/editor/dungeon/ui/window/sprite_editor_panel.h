#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/system/workspace/editor_panel.h"

struct ImVec4;

namespace yaze {
namespace zelda3 {
class Sprite;
}  // namespace zelda3
namespace editor {

struct AgentUITheme;
class DungeonCanvasViewer;
class DungeonRoomStore;

/**
 * @class SpriteEditorPanel
 * @brief WindowContent for placing and managing dungeon sprites
 *
 * Panel layout, filtering, and placement behavior live in the matching .cc file.
 * Room edits continue through DungeonCanvasViewer's interaction handlers.
 */
class SpriteEditorPanel : public WindowContent {
 public:
  SpriteEditorPanel(int* current_room_id, DungeonRoomStore* rooms,
                    DungeonCanvasViewer* canvas_viewer = nullptr);

  std::string GetId() const override;
  std::string GetDisplayName() const override;
  std::string GetIcon() const override;
  std::string GetEditorCategory() const override;
  int GetPriority() const override;
  std::string GetWorkflowGroup() const override;

  void Draw(bool* p_open) override;

  void SetCanvasViewer(DungeonCanvasViewer* viewer);
  void SetSpritePlacedCallback(
      std::function<void(const zelda3::Sprite&)> callback);
  void SetOpenSelectionInspectorCallback(std::function<void()> callback);

 private:
  // These three stages follow the panel's visible top-to-bottom layout.
  void DrawPlacementControls();
  void DrawSpriteSelector();
  void DrawRoomSprites();

  bool MatchesFilter(int sprite_id);
  ImVec4 GetSpriteTypeColor(int sprite_id, const AgentUITheme& theme);
  const char* GetSpriteTypeIcon(int sprite_id);
  const char* GetSpriteCategoryName(int sprite_id);

  int* current_room_id_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;

  // Selection state
  int selected_sprite_id_ = 0;
  int selected_category_ = 0;
  char search_filter_[64] = {0};
  bool placement_mode_ = false;

  std::function<void(const zelda3::Sprite&)> sprite_placed_callback_;
  std::function<void()> open_selection_inspector_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

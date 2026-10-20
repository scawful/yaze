#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "app/editor/system/workspace/editor_panel.h"
#include "zelda3/sprite/sprite_oam_tables.h"

namespace yaze {
namespace zelda3 {
class Room;
class Sprite;
}  // namespace zelda3
namespace editor {

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
  void DrawInWorkbench();

  void SetCanvasViewer(DungeonCanvasViewer* viewer);
  void SetSpritePlacedCallback(
      std::function<void(const zelda3::Sprite&)> callback);
  void SetOpenSelectionInspectorCallback(std::function<void()> callback);

 private:
  void DrawContent(bool show_room_sprites);
  void DrawPlacementControls();
  void DrawSpriteSelector(bool show_room_sprites);
  void DrawRoomSprites();

  std::string SpriteName(int sprite_id) const;
  bool MatchesFilter(int sprite_id) const;

  struct CachedSpritePreview {
    std::vector<uint8_t> pixels;
    SDL_Rect bounds{0, 0, 0, 0};
  };

  const CachedSpritePreview& GetOrCreateSpritePreview(int sprite_id,
                                                      const zelda3::Room& room);

  int* current_room_id_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;

  mutable std::unordered_map<int, CachedSpritePreview> sprite_preview_cache_;
  mutable uint64_t cached_graphics_revision_ = 0;
  zelda3::SpritePreviewResourceCache sprite_preview_resources_;

  // Selection state
  int selected_sprite_id_ = 0;
  char search_filter_[64] = {0};

  std::function<void(const zelda3::Sprite&)> sprite_placed_callback_;
  std::function<void()> open_selection_inspector_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

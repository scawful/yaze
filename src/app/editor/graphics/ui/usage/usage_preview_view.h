#ifndef YAZE_APP_EDITOR_GRAPHICS_UI_USAGE_USAGE_PREVIEW_VIEW_H
#define YAZE_APP_EDITOR_GRAPHICS_UI_USAGE_USAGE_PREVIEW_VIEW_H

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/usage_preview.h"
#include "app/gfx/core/bitmap.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"

namespace yaze::editor {

/**
 * @brief Shows how the game uses the current graphics sheet while it is
 * edited: a room that loads it, a sprite animation drawn from it, and the
 * tile16s that use the selected 8x8 tile.
 *
 * Renders from the Arena sheets (unsaved pixel edits) through
 * usage_preview's renderers; nothing is written to the ROM or GameData.
 * Re-renders at most 10 times a second while pixels change, and once more
 * when the stroke ends.
 */
class UsagePreviewView {
 public:
  explicit UsagePreviewView(GraphicsEditorState* state) : state_(state) {}

  void SetRom(Rom* rom);
  void SetGameData(zelda3::GameData* game_data);
  void SetHackName(std::string hack_name) { hack_name_ = std::move(hack_name); }

  void Draw();

  static constexpr double kMinRenderInterval = 0.1;  // 10 renders/s

 private:
  enum class Mode { kRoom, kSprite, kTile16 };

  void EnsureTables();
  void DrawRoomContext(bool render_now);
  void DrawSpriteContext(bool render_now, float delta);
  void DrawTile16Context(bool render_now);
  // Hash of the current sheet's and the modified sheets' Arena pixels.
  uint64_t HashWatchedSheets() const;
  usage_preview::SheetPixels Snapshot() const;
  void ShowImage(gfx::Bitmap& bitmap, const usage_preview::IndexedImage& image);
  void DrawBitmapFit(gfx::Bitmap& bitmap, const std::vector<SDL_Rect>& rects,
                     float max_scale);
  int SelectedTileIndex() const;
  void InvalidateTables();

  GraphicsEditorState* state_ = nullptr;
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::string hack_name_;

  Mode mode_ = Mode::kRoom;
  bool live_ = true;
  bool dirty_ = true;
  uint64_t last_hash_ = 0;
  double last_render_time_ = -1.0;
  uint16_t last_sheet_ = 0xFFFF;
  std::string status_;

  // Group tables, built once per ROM.
  bool tables_ready_ = false;
  std::map<int, std::array<uint8_t, 16>> room_blocks_;
  std::map<int, zelda3::RoomGfxInfo> room_gfx_;
  std::optional<std::map<int, zelda3::OverworldAreaGfxInfo>> areas_;

  // Room context.
  usage_preview::RoomUsagePreview room_preview_;
  std::vector<int> rooms_for_sheet_;
  int room_choice_ = 0;
  bool objects_only_ = false;
  bool outline_objects_ = true;
  std::vector<SDL_Rect> object_rects_;

  // Sprite context.
  usage_preview::SpriteUsagePreview sprite_preview_;
  std::vector<int> spritesets_for_sheet_;
  int spriteset_choice_ = 0;
  int configured_spriteset_ = -1;
  int vanilla_sprite_id_ = 0xA7;  // Stalfos
  char zsm_path_[512] = {};
  std::string zsm_loaded_path_;
  std::optional<zsprite::ZSprite> zsm_;
  int zsm_animation_ = 0;
  bool playing_ = true;
  gfx::Bitmap sprite_bitmap_;

  // Tile16 context.
  usage_preview::Tile16UsagePreview tile16_preview_;
  std::vector<int> areas_for_sheet_;
  int area_choice_ = 0;
  int tile_index_ = 0;
  bool follow_selection_ = true;
  std::vector<int> tile16s_;
  std::vector<SDL_Rect> tile16_highlights_;
  gfx::Bitmap tile16_bitmap_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_UI_USAGE_USAGE_PREVIEW_VIEW_H

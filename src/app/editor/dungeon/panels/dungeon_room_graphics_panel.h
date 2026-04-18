#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_

#include <array>
#include <string>

#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

class DungeonEditorV2;

/**
 * @class DungeonRoomGraphicsPanel
 * @brief WindowContent for displaying room graphics blocks
 *
 * This panel shows the graphics blocks used by the current room,
 * displaying a 2x8 grid of 128x32 graphics blocks.
 *
 * @see WindowContent - Base interface
 */
class DungeonRoomGraphicsPanel : public WindowContent {
 public:
  // Default constructor for ContentRegistry self-registration
  DungeonRoomGraphicsPanel()
      : room_gfx_canvas_("##RoomGfxCanvasPanel", ImVec2(256 + 1, 256 + 1)) {}

  // Legacy constructor for direct instantiation
  DungeonRoomGraphicsPanel(int* current_room_id, DungeonRoomStore* rooms,
                           gfx::IRenderer* renderer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        renderer_(renderer),
        room_gfx_canvas_("##RoomGfxCanvasPanel", ImVec2(256 + 1, 256 + 1)) {}

  /**
   * @brief Set the current palette group for graphics rendering
   * @param group The palette group from the current room
   */
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    current_palette_group_ = group;
    palette_dirty_ = true;
  }

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.room_graphics"; }
  std::string GetDisplayName() const override { return "Room Graphics"; }
  std::string GetIcon() const override { return ICON_MD_IMAGE; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 50; }

  // ==========================================================================
  // WindowContent Drawing
  // ==========================================================================

  void Draw(bool* p_open) override;

 private:
  struct SheetPreviewMetadata {
    uint8_t block_id = 0;
    size_t source_offset = 0;
    int width = 0;
    int height = 0;
    int nonzero_pixels = 0;
    int palette_colors = 0;
    bool bitmap_active = false;
    bool has_surface = false;
    bool has_texture = false;
  };

  void RefreshSheetPreviews(const zelda3::Room& room);

  int* current_room_id_ = nullptr;
  DungeonRoomStore* rooms_ = nullptr;
  gfx::IRenderer* renderer_ = nullptr;
  gui::Canvas room_gfx_canvas_;
  std::array<gfx::Bitmap, 16> sheet_previews_;
  std::array<SheetPreviewMetadata, 16> sheet_preview_metadata_{};
  std::array<uint8_t, 16> preview_block_ids_{};
  int preview_room_id_ = -1;
  bool preview_cache_valid_ = false;
  bool show_source_trace_ = false;

  // Palette tracking for proper sheet coloring
  gfx::PaletteGroup current_palette_group_;
  bool palette_dirty_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_GRAPHICS_PANEL_H_

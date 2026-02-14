#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_tile_editor.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @brief Panel for editing the tile8 composition of dungeon objects
 *
 * Shows an interactive grid of an object's constituent 8x8 tiles alongside
 * a tile source sheet from the room's graphics buffer. Users can click a
 * cell in the grid, then pick a replacement tile from the source sheet.
 * Tile properties (palette, flip, priority) can be edited per-cell.
 *
 * Opened from the Object Editor panel via "Edit Tiles" button.
 */
class ObjectTileEditorPanel : public EditorPanel {
 public:
  ObjectTileEditorPanel(gfx::IRenderer* renderer, Rom* rom);

  std::string GetId() const override { return "dungeon.object_tile_editor"; }
  std::string GetDisplayName() const override { return "Object Tile Editor"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 65; }
  float GetPreferredWidth() const override { return 550.0f; }

  void Draw(bool* p_open) override;

  void OpenForObject(int16_t object_id, int room_id,
                     std::array<zelda3::Room, 0x128>* rooms);
  void OpenForNewObject(int width, int height, const std::string& filename,
                        int16_t object_id, int room_id,
                        std::array<zelda3::Room, 0x128>* rooms);
  void Close();
  bool IsOpen() const { return is_open_; }

  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group);

  // Callback fired on first successful save of a new object
  void SetObjectCreatedCallback(
      std::function<void(int, const std::string&)> cb) {
    on_object_created_ = std::move(cb);
  }

 private:
  void DrawTileGrid();
  void DrawSourceSheet();
  void DrawTileProperties();
  void DrawActionBar();
  void HandleKeyboardShortcuts();

  void RenderObjectPreview();
  void RenderTile8Atlas();

  // Apply: write back, re-render room, reset modified flags.
  // If confirm_shared is true and tile data is shared, opens confirmation modal
  // instead of applying immediately.
  void ApplyChanges(bool confirm_shared = true);

  // State
  std::unique_ptr<zelda3::ObjectTileEditor> tile_editor_;
  zelda3::ObjectTileLayout current_layout_;
  int selected_cell_index_ = -1;
  int selected_source_tile_ = -1;
  int source_palette_ = 2;

  // Canvases
  gui::Canvas tile_grid_canvas_{"##ObjTileGrid", ImVec2(256, 256)};
  gui::Canvas source_sheet_canvas_{"##ObjTileSource", ImVec2(256, 512)};

  // Bitmaps
  gfx::Bitmap object_preview_bmp_;
  gfx::Bitmap tile8_atlas_bmp_;
  bool preview_dirty_ = true;
  bool atlas_dirty_ = true;

  // Shared tile data confirmation
  bool show_shared_confirm_ = false;
  int shared_object_count_ = 0;

  // New object creation state
  bool is_new_object_ = false;
  std::function<void(int, const std::string&)> on_object_created_;

  // Context
  gfx::IRenderer* renderer_;
  Rom* rom_;
  int current_room_id_ = -1;
  int16_t current_object_id_ = -1;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  gfx::PaletteGroup current_palette_group_;
  bool is_open_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_OBJECT_TILE_EDITOR_PANEL_H_

#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gui {

using app::gfx::Bitmap;
using app::gfx::BitmapTable;

class Canvas {
 public:
  Canvas() = default;
  explicit Canvas(ImVec2 canvas_size)
      : custom_canvas_size_(true), canvas_sz_(canvas_size) {}

  // Background for the Canvas represents region without any content drawn to
  // it, but can be controlled by the user.
  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0));

  // Context Menu refers to what happens when the right mouse button is pressed
  // This routine also handles the scrolling for the canvas.
  void DrawContextMenu();

  // Tile painter shows a preview of the currently selected tile
  // and allows the user to left click to paint the tile or right
  // click to select a new tile to paint with.
  bool DrawTilePainter(const Bitmap& bitmap, int size);

  // Dictates which tile is currently selected based on what the user clicks
  // in the canvas window. Represented and split apart into a grid of tiles.
  void DrawTileSelector(int size);

  // Draws the contents of the Bitmap image to the Canvas

  void DrawBitmap(const Bitmap& bitmap, int border_offset = 0,
                  bool ready = true);
  void DrawBitmap(const Bitmap& bitmap, int x_offset, int y_offset);

  void DrawBitmapTable(const BitmapTable& gfx_bin);
  void DrawOutline(int x, int y, int w, int h);
  void DrawRect(int x, int y, int w, int h, ImVec4 color);
  void DrawText(std::string text, int x, int y);
  void DrawGrid(float grid_step = 64.0f);
  void DrawOverlay();  // last

  auto Points() const { return points_; }
  auto GetDrawList() const { return draw_list_; }
  auto GetZeroPoint() const { return canvas_p0_; }
  auto GetCurrentDrawnTilePosition() const { return drawn_tile_pos_; }
  auto GetCanvasSize() const { return canvas_sz_; }
  void SetCanvasSize(ImVec2 canvas_size) {
    canvas_sz_ = canvas_size;
    custom_canvas_size_ = true;
  }

 private:
  bool enable_grid_ = true;
  bool enable_context_menu_ = true;
  bool custom_canvas_size_ = false;
  bool is_hovered_ = false;

  ImDrawList* draw_list_;
  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
  ImVec2 mouse_pos_in_canvas_;
  ImVec2 drawn_tile_pos_;

  std::vector<app::gfx::Bitmap> changed_tiles_;
  app::gfx::Bitmap current_tile_;

  std::string title_;
};

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif
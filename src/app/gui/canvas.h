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

  void Update(const gfx::Bitmap& bitmap, ImVec2 bg_size, int tile_size,
              float scale = 1.0f, float grid_size = 64.0f);

  void UpdateColorPainter(const gfx::Bitmap& bitmap, const ImVec4& color,
                          const std::function<void()>& event, ImVec2 bg_size,
                          int tile_size, float scale = 1.0f,
                          float grid_size = 64.0f);

  void UpdateEvent(const std::function<void()>& event, ImVec2 bg_size,
                   int tile_size, float scale = 1.0f, float grid_size = 64.0f);

  // Background for the Canvas represents region without any content drawn to
  // it, but can be controlled by the user.
  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0));

  // Context Menu refers to what happens when the right mouse button is pressed
  // This routine also handles the scrolling for the canvas.
  void DrawContextMenu();

  // Tile painter shows a preview of the currently selected tile
  // and allows the user to left click to paint the tile or right
  // click to select a new tile to paint with.
  bool DrawTilePainter(const Bitmap& bitmap, int size, float scale = 1.0f);
  bool DrawSolidTilePainter(const ImVec4& color, int size);

  // Draws a tile on the canvas at the specified position
  void DrawTileOnBitmap(int tile_size, gfx::Bitmap& bitmap, ImVec4 color);

  // Dictates which tile is currently selected based on what the user clicks
  // in the canvas window. Represented and split apart into a grid of tiles.
  void DrawTileSelector(int size);

  void HandleTileEdits(Canvas& blockset_canvas,
                       std::vector<gfx::Bitmap>& source_blockset,
                       gfx::Bitmap& destination, int& current_tile,
                       float scale = 1.0f, int tile_painter_size = 16,
                       int tiles_per_row = 8);

  void RenderUpdatedBitmap(const ImVec2& click_position, const Bytes& tile_data,
                           gfx::Bitmap& destination);

  // Draws the contents of the Bitmap image to the Canvas
  void DrawBitmap(const Bitmap& bitmap, int border_offset = 0,
                  bool ready = true);
  void DrawBitmap(const Bitmap& bitmap, int border_offset, float scale);
  void DrawBitmap(const Bitmap& bitmap, int x_offset = 0, int y_offset = 0,
                  float scale = 1.0f);

  void DrawBitmapTable(const BitmapTable& gfx_bin);
  void DrawOutline(int x, int y, int w, int h);
  void DrawSelectRect(int tile_size, float scale = 1.0f);
  void DrawRect(int x, int y, int w, int h, ImVec4 color);
  void DrawText(std::string text, int x, int y);
  void DrawGrid(float grid_step = 64.0f);
  void DrawOverlay();  // last

  auto Points() const { return points_; }
  auto GetDrawList() const { return draw_list_; }
  auto zero_point() const { return canvas_p0_; }
  auto Scrolling() const { return scrolling_; }
  auto drawn_tile_position() const { return drawn_tile_pos_; }
  auto canvas_size() const { return canvas_sz_; }
  void SetCanvasSize(ImVec2 canvas_size) {
    canvas_sz_ = canvas_size;
    custom_canvas_size_ = true;
  }
  auto IsMouseHovering() const { return is_hovered_; }
  void ZoomIn() { global_scale_ += 0.1f; }
  void ZoomOut() { global_scale_ -= 0.1f; }

  void set_global_scale(float scale) { global_scale_ = scale; }
  auto global_scale() const { return global_scale_; }

 private:
  bool enable_grid_ = true;
  bool enable_hex_tile_labels_ = false;
  bool enable_context_menu_ = true;
  bool custom_canvas_size_ = false;
  bool is_hovered_ = false;

  float custom_step_ = 0.0f;
  float global_scale_ = 1.0f;

  ImDrawList* draw_list_;
  ImVector<ImVec2> points_;
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
  ImVec2 mouse_pos_in_canvas_;
  ImVec2 drawn_tile_pos_;
};

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif
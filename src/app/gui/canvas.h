#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <cstdint>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {

/**
 * @namespace yaze::gui
 * @brief Graphical User Interface (GUI) components for the application.
 */
namespace gui {

using gfx::Bitmap;
using gfx::BitmapTable;

enum class CanvasType { kTile, kBlock, kMap };
enum class CanvasMode { kPaint, kSelect };
enum class CanvasGridSize { k8x8, k16x16, k32x32, k64x64 };

/**
 * @class Canvas
 * @brief Represents a canvas for drawing and manipulating graphics.
 *
 * The Canvas class provides various functions for updating and drawing graphics
 * on a canvas. It supports features such as bitmap drawing, context menu
 * handling, tile painting, custom grid, and more.
 */
class Canvas : public SharedRom {
 public:
  Canvas() = default;
  explicit Canvas(const std::string &id) : canvas_id_(id) {
    context_id_ = id + "Context";
  }
  explicit Canvas(const std::string &id, ImVec2 canvas_size)
      : canvas_id_(id), custom_canvas_size_(true), canvas_sz_(canvas_size) {
    context_id_ = id + "Context";
  }
  explicit Canvas(const std::string &id, ImVec2 canvas_size,
                  CanvasGridSize grid_size)
      : canvas_id_(id), custom_canvas_size_(true), canvas_sz_(canvas_size) {
    context_id_ = id + "Context";
    SetCanvasGridSize(grid_size);
  }
  explicit Canvas(const std::string &id, ImVec2 canvas_size,
                  CanvasGridSize grid_size, float global_scale)
      : canvas_id_(id),
        custom_canvas_size_(true),
        canvas_sz_(canvas_size),
        global_scale_(global_scale) {
    context_id_ = id + "Context";
    SetCanvasGridSize(grid_size);
  }

  void SetCanvasGridSize(CanvasGridSize grid_size) {
    switch (grid_size) {
      case CanvasGridSize::k8x8:
        custom_step_ = 8.0f;
        break;
      case CanvasGridSize::k16x16:
        custom_step_ = 16.0f;
        break;
      case CanvasGridSize::k32x32:
        custom_step_ = 32.0f;
        break;
      case CanvasGridSize::k64x64:
        custom_step_ = 64.0f;
        break;
    }
  }

  void UpdateColorPainter(gfx::Bitmap &bitmap, const ImVec4 &color,
                          const std::function<void()> &event, int tile_size,
                          float scale = 1.0f);

  void UpdateInfoGrid(ImVec2 bg_size, int tile_size, float scale = 1.0f,
                      float grid_size = 64.0f, int label_id = 0);

  // Background for the Canvas represents region without any content drawn to
  // it, but can be controlled by the user.
  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0), bool drag = false);

  // Context Menu refers to what happens when the right mouse button is pressed
  // This routine also handles the scrolling for the canvas.
  void DrawContextMenu();

  // Tile painter shows a preview of the currently selected tile
  // and allows the user to left click to paint the tile or right
  // click to select a new tile to paint with.
  bool DrawTilePainter(const Bitmap &bitmap, int size, float scale = 1.0f);
  bool DrawSolidTilePainter(const ImVec4 &color, int size);

  // Draws a tile on the canvas at the specified position
  void DrawTileOnBitmap(int tile_size, gfx::Bitmap *bitmap, ImVec4 color);

  // Dictates which tile is currently selected based on what the user clicks
  // in the canvas window. Represented and split apart into a grid of tiles.
  bool DrawTileSelector(int size);

  // Draws the selection rectangle when the user is selecting multiple tiles
  void DrawSelectRect(int current_map, int tile_size = 0x10,
                      float scale = 1.0f);

  // Draws the contents of the Bitmap image to the Canvas
  void DrawBitmap(Bitmap &bitmap, int border_offset = 0, bool ready = true);
  void DrawBitmap(Bitmap &bitmap, int border_offset, float scale);
  void DrawBitmap(Bitmap &bitmap, int x_offset = 0, int y_offset = 0,
                  float scale = 1.0f, int alpha = 255);
  void DrawBitmapTable(const BitmapTable &gfx_bin);

  void DrawBitmapGroup(std::vector<int> &group,
                       std::array<gfx::Bitmap, 4096> &tile16_individual_,
                       int tile_size, float scale = 1.0f);

  void DrawOutline(int x, int y, int w, int h);
  void DrawOutlineWithColor(int x, int y, int w, int h, ImVec4 color);
  void DrawOutlineWithColor(int x, int y, int w, int h, uint32_t color);

  void DrawRect(int x, int y, int w, int h, ImVec4 color);

  void DrawText(std::string text, int x, int y);
  void DrawGridLines(float grid_step);
  void DrawGrid(float grid_step = 64.0f, int tile_id_offset = 8);
  void DrawOverlay();  // last

  void DrawInfoGrid(float grid_step = 64.0f, int tile_id_offset = 8,
                    int label_id = 0);

  void DrawLayeredElements();

  int GetTileIdFromMousePos() {
    float x = mouse_pos_in_canvas_.x;
    float y = mouse_pos_in_canvas_.y;
    int num_columns = (canvas_sz_.x / global_scale_) / custom_step_;
    int num_rows = (canvas_sz_.y / global_scale_) / custom_step_;
    int tile_id = (x / custom_step_) + (y / custom_step_) * num_columns;
    tile_id = tile_id / global_scale_;
    if (tile_id >= num_columns * num_rows) {
      tile_id = -1;  // Invalid tile ID
    }
    return tile_id;
  }
  void SetCanvasSize(ImVec2 canvas_size) {
    canvas_sz_ = canvas_size;
    custom_canvas_size_ = true;
  }
  void DrawCustomHighlight(float grid_step);
  bool IsMouseHovering() const { return is_hovered_; }
  void ZoomIn() { global_scale_ += 0.25f; }
  void ZoomOut() { global_scale_ -= 0.25f; }

  auto points() const { return points_; }
  auto mutable_points() { return &points_; }
  auto push_back(ImVec2 pos) { points_.push_back(pos); }
  auto draw_list() const { return draw_list_; }
  auto zero_point() const { return canvas_p0_; }
  auto scrolling() const { return scrolling_; }
  auto drawn_tile_position() const { return drawn_tile_pos_; }
  auto canvas_size() const { return canvas_sz_; }
  void set_global_scale(float scale) { global_scale_ = scale; }
  auto global_scale() const { return global_scale_; }
  auto custom_labels_enabled() { return &enable_custom_labels_; }
  auto custom_step() const { return custom_step_; }
  auto width() const { return canvas_sz_.x; }
  auto height() const { return canvas_sz_.y; }
  auto set_draggable(bool value) { draggable_ = value; }
  auto canvas_id() const { return canvas_id_; }
  auto labels(int i) {
    if (i >= labels_.size()) {
      labels_.push_back(ImVector<std::string>());
    }
    return labels_[i];
  }
  auto mutable_labels(int i) {
    if (i >= labels_.size()) {
      int x = i;
      while (x >= labels_.size()) {
        labels_.push_back(ImVector<std::string>());
        x--;
      }
      labels_.push_back(ImVector<std::string>());
    }
    return &labels_[i];
  }

  auto set_current_labels(int i) { current_labels_ = i; }
  auto set_highlight_tile_id(int i) { highlight_tile_id = i; }

  auto selected_tiles() const { return selected_tiles_; }
  auto mutable_selected_tiles() { return &selected_tiles_; }

  auto selected_tile_pos() const { return selected_tile_pos_; }
  auto set_selected_tile_pos(ImVec2 pos) { selected_tile_pos_ = pos; }
  bool select_rect_active() const { return select_rect_active_; }
  auto selected_points() const { return selected_points_; }

  auto hover_mouse_pos() const { return mouse_pos_in_canvas_; }

 private:
  bool draggable_ = false;
  bool is_hovered_ = false;
  bool enable_grid_ = true;
  bool enable_hex_tile_labels_ = false;
  bool enable_custom_labels_ = false;
  bool enable_context_menu_ = true;
  bool custom_canvas_size_ = false;
  bool select_rect_active_ = false;
  bool refresh_graphics_ = false;

  float custom_step_ = 0.0f;
  float global_scale_ = 1.0f;

  int current_labels_ = 0;
  int highlight_tile_id = -1;

  uint16_t edit_palette_index_ = 0;
  uint64_t edit_palette_group_name_index_ = 0;
  uint64_t edit_palette_sub_index_ = 0;

  Bitmap *bitmap_ = nullptr;

  ImDrawList *draw_list_ = nullptr;

  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
  ImVec2 drawn_tile_pos_;
  ImVec2 mouse_pos_in_canvas_;
  ImVec2 selected_tile_pos_ = ImVec2(-1, -1);

  ImVector<ImVec2> points_;
  ImVector<ImVec2> selected_points_;
  ImVector<ImVector<std::string>> labels_;

  std::string canvas_id_ = "Canvas";
  std::string context_id_ = "CanvasContext";
  std::vector<ImVec2> selected_tiles_;
};

void BeginCanvas(Canvas &canvas, ImVec2 child_size = ImVec2(0, 0));
void EndCanvas(Canvas &canvas);

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, BitmapTable &graphics_bin);

void BitmapCanvasPipeline(gui::Canvas &canvas, gfx::Bitmap &bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id);

}  // namespace gui
}  // namespace yaze

#endif

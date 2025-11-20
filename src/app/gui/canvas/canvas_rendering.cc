#include "canvas_rendering.h"

#include <algorithm>
#include <cmath>

#include "app/gui/canvas/canvas_utils.h"

namespace yaze {
namespace gui {

namespace {
// Constants extracted from canvas.cc
constexpr uint32_t kRectangleColor = IM_COL32(32, 32, 32, 255);
constexpr uint32_t kWhiteColor = IM_COL32(255, 255, 255, 255);
}  // namespace

void RenderCanvasBackground(ImDrawList* draw_list,
                            const CanvasGeometry& geometry) {
  // Draw border and background color (extracted from Canvas::DrawBackground)
  draw_list->AddRectFilled(geometry.canvas_p0, geometry.canvas_p1,
                           kRectangleColor);
  draw_list->AddRect(geometry.canvas_p0, geometry.canvas_p1, kWhiteColor);
}

void RenderCanvasGrid(ImDrawList* draw_list, const CanvasGeometry& geometry,
                      const CanvasConfig& config, int highlight_tile_id) {
  if (!config.enable_grid) {
    return;
  }

  // Create render context for utility functions (extracted from
  // Canvas::DrawGrid)
  CanvasUtils::CanvasRenderContext ctx = {
      .draw_list = draw_list,
      .canvas_p0 = geometry.canvas_p0,
      .canvas_p1 = geometry.canvas_p1,
      .scrolling = geometry.scrolling,
      .global_scale = config.global_scale,
      .enable_grid = config.enable_grid,
      .enable_hex_labels = config.enable_hex_labels,
      .grid_step = config.grid_step};

  // Use high-level utility function
  CanvasUtils::DrawCanvasGrid(ctx, highlight_tile_id);
}

void RenderCanvasOverlay(ImDrawList* draw_list, const CanvasGeometry& geometry,
                         const CanvasConfig& config,
                         const ImVector<ImVec2>& points,
                         const ImVector<ImVec2>& selected_points) {
  // Create render context for utility functions (extracted from
  // Canvas::DrawOverlay)
  CanvasUtils::CanvasRenderContext ctx = {
      .draw_list = draw_list,
      .canvas_p0 = geometry.canvas_p0,
      .canvas_p1 = geometry.canvas_p1,
      .scrolling = geometry.scrolling,
      .global_scale = config.global_scale,
      .enable_grid = config.enable_grid,
      .enable_hex_labels = config.enable_hex_labels,
      .grid_step = config.grid_step};

  // Use high-level utility function
  CanvasUtils::DrawCanvasOverlay(ctx, points, selected_points);
}

void RenderCanvasLabels(ImDrawList* draw_list, const CanvasGeometry& geometry,
                        const CanvasConfig& config,
                        const ImVector<ImVector<std::string>>& labels,
                        int current_labels, int tile_id_offset) {
  if (!config.enable_custom_labels || current_labels >= labels.size()) {
    return;
  }

  // Push clip rect to prevent drawing outside canvas
  draw_list->PushClipRect(geometry.canvas_p0, geometry.canvas_p1, true);

  // Create render context for utility functions
  CanvasUtils::CanvasRenderContext ctx = {
      .draw_list = draw_list,
      .canvas_p0 = geometry.canvas_p0,
      .canvas_p1 = geometry.canvas_p1,
      .scrolling = geometry.scrolling,
      .global_scale = config.global_scale,
      .enable_grid = config.enable_grid,
      .enable_hex_labels = config.enable_hex_labels,
      .grid_step = config.grid_step};

  // Use high-level utility function (extracted from Canvas::DrawInfoGrid)
  CanvasUtils::DrawCanvasLabels(ctx, labels, current_labels, tile_id_offset);

  draw_list->PopClipRect();
}

void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, int /*border_offset*/,
                          float scale) {
  if (!bitmap.is_active()) {
    return;
  }

  // Extracted from Canvas::DrawBitmap (border offset variant)
  draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                      ImVec2(geometry.canvas_p0.x, geometry.canvas_p0.y),
                      ImVec2(geometry.canvas_p0.x + (bitmap.width() * scale),
                             geometry.canvas_p0.y + (bitmap.height() * scale)));
  draw_list->AddRect(geometry.canvas_p0, geometry.canvas_p1, kWhiteColor);
}

void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, int x_offset, int y_offset,
                          float scale, int alpha) {
  if (!bitmap.is_active()) {
    return;
  }

  // Calculate the actual rendered size including scale and offsets
  // CRITICAL: Use scale parameter (NOT global_scale_) for per-bitmap scaling
  // Extracted from Canvas::DrawBitmap (x/y offset variant)
  ImVec2 rendered_size(bitmap.width() * scale, bitmap.height() * scale);

  // CRITICAL FIX: Draw bitmap WITHOUT additional global_scale multiplication
  // The scale parameter already contains the correct scale factor
  // The scrolling should NOT be scaled - it's already in screen space
  draw_list->AddImage(
      (ImTextureID)(intptr_t)bitmap.texture(),
      ImVec2(geometry.canvas_p0.x + x_offset + geometry.scrolling.x,
             geometry.canvas_p0.y + y_offset + geometry.scrolling.y),
      ImVec2(geometry.canvas_p0.x + x_offset + geometry.scrolling.x +
                 rendered_size.x,
             geometry.canvas_p0.y + y_offset + geometry.scrolling.y +
                 rendered_size.y),
      ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
}

void RenderBitmapOnCanvas(ImDrawList* draw_list, const CanvasGeometry& geometry,
                          gfx::Bitmap& bitmap, ImVec2 dest_pos,
                          ImVec2 dest_size, ImVec2 src_pos, ImVec2 src_size) {
  if (!bitmap.is_active()) {
    return;
  }

  // Extracted from Canvas::DrawBitmap (custom source/dest regions variant)
  draw_list->AddImage(
      (ImTextureID)(intptr_t)bitmap.texture(),
      ImVec2(geometry.canvas_p0.x + dest_pos.x,
             geometry.canvas_p0.y + dest_pos.y),
      ImVec2(geometry.canvas_p0.x + dest_pos.x + dest_size.x,
             geometry.canvas_p0.y + dest_pos.y + dest_size.y),
      ImVec2(src_pos.x / bitmap.width(), src_pos.y / bitmap.height()),
      ImVec2((src_pos.x + src_size.x) / bitmap.width(),
             (src_pos.y + src_size.y) / bitmap.height()));
}

void RenderBitmapGroup(ImDrawList* draw_list, const CanvasGeometry& geometry,
                       std::vector<int>& group, gfx::Tilemap& tilemap,
                       int tile_size, float scale, int local_map_size,
                       ImVec2 total_map_size) {
  // Extracted from Canvas::DrawBitmapGroup (lines 1148-1264)
  // This is used for multi-tile selection preview in overworld editor

  if (group.empty()) {
    return;
  }

  // OPTIMIZATION: Use optimized rendering for large groups to improve
  // performance
  bool use_optimized_rendering = group.size() > 128;

  // Pre-calculate common values to avoid repeated computation
  const float tile_scale = tile_size * scale;
  const int atlas_tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;

  // Get selected points (note: this assumes selected_points are available in
  // context) For now, we'll just render tiles at their grid positions The full
  // implementation would need the selected_points passed in

  int i = 0;
  for (const auto tile_id : group) {
    // Calculate grid position for this tile
    int tiles_per_row = 32;  // Default for standard maps
    int x = i % tiles_per_row;
    int y = i / tiles_per_row;

    int tile_pos_x = x * tile_size * scale;
    int tile_pos_y = y * tile_size * scale;

    // Check if tile_id is within the range
    auto tilemap_size = tilemap.map_size.x;
    if (tile_id >= 0 && tile_id < tilemap_size) {
      if (tilemap.atlas.is_active() && tilemap.atlas.texture() &&
          atlas_tiles_per_row > 0) {
        int atlas_tile_x =
            (tile_id % atlas_tiles_per_row) * tilemap.tile_size.x;
        int atlas_tile_y =
            (tile_id / atlas_tiles_per_row) * tilemap.tile_size.y;

        // Simple bounds check
        if (atlas_tile_x >= 0 && atlas_tile_x < tilemap.atlas.width() &&
            atlas_tile_y >= 0 && atlas_tile_y < tilemap.atlas.height()) {
          // Calculate UV coordinates once for efficiency
          const float atlas_width = static_cast<float>(tilemap.atlas.width());
          const float atlas_height = static_cast<float>(tilemap.atlas.height());
          ImVec2 uv0 =
              ImVec2(atlas_tile_x / atlas_width, atlas_tile_y / atlas_height);
          ImVec2 uv1 =
              ImVec2((atlas_tile_x + tilemap.tile_size.x) / atlas_width,
                     (atlas_tile_y + tilemap.tile_size.y) / atlas_height);

          // Calculate screen positions
          float screen_x =
              geometry.canvas_p0.x + geometry.scrolling.x + tile_pos_x;
          float screen_y =
              geometry.canvas_p0.y + geometry.scrolling.y + tile_pos_y;
          float screen_w = tilemap.tile_size.x * scale;
          float screen_h = tilemap.tile_size.y * scale;

          // Use higher alpha for large selections to make them more visible
          uint32_t alpha_color = use_optimized_rendering
                                     ? IM_COL32(255, 255, 255, 200)
                                     : IM_COL32(255, 255, 255, 150);

          // Draw from atlas texture with optimized parameters
          draw_list->AddImage((ImTextureID)(intptr_t)tilemap.atlas.texture(),
                              ImVec2(screen_x, screen_y),
                              ImVec2(screen_x + screen_w, screen_y + screen_h),
                              uv0, uv1, alpha_color);
        }
      }
    }
    i++;
  }
}

}  // namespace gui
}  // namespace yaze

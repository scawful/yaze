#include "app/gui/canvas/canvas.h"

#include <algorithm>
#include <cmath>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas_rendering.h"
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"

namespace yaze::gui {

namespace {
CanvasGeometry GetGeometryFromRuntime(const CanvasRuntime& rt) {
  CanvasGeometry geom;
  geom.canvas_p0 = rt.canvas_p0;
  geom.canvas_sz = rt.canvas_sz;
  geom.scrolling = rt.scrolling;
  geom.scaled_size =
      ImVec2(rt.canvas_sz.x * rt.scale, rt.canvas_sz.y * rt.scale);
  geom.canvas_p1 = ImVec2(geom.canvas_p0.x + geom.canvas_sz.x,
                          geom.canvas_p0.y + geom.canvas_sz.y);
  return geom;
}

ImVec2 AlignPosToGridHelper(ImVec2 pos, float scale) {
  return ImVec2(std::floor(pos.x / scale) * scale,
                std::floor(pos.y / scale) * scale);
}
}  // namespace

void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap, int border_offset,
                float scale) {
  if (!rt.draw_list)
    return;
  CanvasGeometry geom = GetGeometryFromRuntime(rt);
  RenderBitmapOnCanvas(rt.draw_list, geom, bitmap, border_offset, scale);
}

void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap, int x_offset,
                int y_offset, float scale, int alpha) {
  if (!rt.draw_list)
    return;
  CanvasGeometry geom = GetGeometryFromRuntime(rt);
  RenderBitmapOnCanvas(rt.draw_list, geom, bitmap, x_offset, y_offset, scale,
                       alpha);
}

void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap, ImVec2 dest_pos,
                ImVec2 dest_size, ImVec2 src_pos, ImVec2 src_size) {
  if (!rt.draw_list)
    return;
  CanvasGeometry geom = GetGeometryFromRuntime(rt);
  RenderBitmapOnCanvas(rt.draw_list, geom, bitmap, dest_pos, dest_size, src_pos,
                       src_size);
}

void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap,
                const BitmapDrawOpts& opts) {
  if (!rt.draw_list)
    return;

  if (opts.ensure_texture && !bitmap.texture() && bitmap.surface()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
  }

  if (opts.dest_size.x > 0 && opts.dest_size.y > 0) {
    ImVec2 src_size = opts.src_size;
    if (src_size.x <= 0 || src_size.y <= 0) {
      src_size = ImVec2(static_cast<float>(bitmap.width()),
                        static_cast<float>(bitmap.height()));
    }
    DrawBitmap(rt, bitmap, opts.dest_pos, opts.dest_size, opts.src_pos,
               src_size);
  } else {
    DrawBitmap(rt, bitmap, static_cast<int>(opts.dest_pos.x),
               static_cast<int>(opts.dest_pos.y), opts.scale, opts.alpha);
  }
}

void DrawBitmapPreview(const CanvasRuntime& rt, gfx::Bitmap& bitmap,
                       const BitmapPreviewOptions& options) {
  if (options.ensure_texture && !bitmap.texture() && bitmap.surface()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
  }

  if (options.dest_size.x > 0 && options.dest_size.y > 0) {
    ImVec2 src_size = options.src_size;
    if (src_size.x <= 0 || src_size.y <= 0) {
      src_size = ImVec2(bitmap.width(), bitmap.height());
    }
    DrawBitmap(rt, bitmap, options.dest_pos, options.dest_size, options.src_pos,
               src_size);
  } else {
    DrawBitmap(rt, bitmap, static_cast<int>(options.dest_pos.x),
               static_cast<int>(options.dest_pos.y), options.scale,
               options.alpha);
  }
}

bool RenderPreviewPanel(const CanvasRuntime& rt, gfx::Bitmap& bmp,
                        const PreviewPanelOpts& opts) {
  if (!rt.draw_list)
    return false;

  if (opts.ensure_texture && !bmp.texture() && bmp.surface()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bmp);
  }

  if (opts.dest_size.x > 0 && opts.dest_size.y > 0) {
    DrawBitmap(rt, bmp, opts.dest_pos, opts.dest_size, ImVec2(0, 0),
               ImVec2(static_cast<float>(bmp.width()),
                      static_cast<float>(bmp.height())));
  } else {
    DrawBitmap(rt, bmp, static_cast<int>(opts.dest_pos.x),
               static_cast<int>(opts.dest_pos.y), 1.0f, 255);
  }
  return true;
}

void DrawRect(const CanvasRuntime& rt, int x, int y, int w, int h,
              ImVec4 color) {
  if (!rt.draw_list)
    return;
  CanvasUtils::DrawCanvasRect(rt.draw_list, rt.canvas_p0, rt.scrolling, x, y, w,
                              h, color, rt.scale);
}

void DrawText(const CanvasRuntime& rt, const std::string& text, int x, int y) {
  if (!rt.draw_list)
    return;
  CanvasUtils::DrawCanvasText(rt.draw_list, rt.canvas_p0, rt.scrolling, text, x,
                              y, rt.scale);
}

void DrawOutline(const CanvasRuntime& rt, int x, int y, int w, int h,
                 ImU32 color) {
  if (!rt.draw_list)
    return;
  CanvasUtils::DrawCanvasOutline(rt.draw_list, rt.canvas_p0, rt.scrolling, x, y,
                                 w, h, color);
}

bool DrawTilemapPainter(const CanvasRuntime& rt, gfx::Tilemap& tilemap,
                        int current_tile, ImVec2* out_drawn_pos) {
  if (!rt.draw_list)
    return false;

  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(rt.canvas_p0.x + rt.scrolling.x,
                      rt.canvas_p0.y + rt.scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  if (!tilemap.atlas.is_active() || tilemap.tile_size.x <= 0) {
    return false;
  }

  const float scaled_size = tilemap.tile_size.x * rt.scale;

  if (!rt.hovered) {
    return false;
  }

  ImVec2 paint_pos = AlignPosToGridHelper(mouse_pos, scaled_size);

  if (tilemap.atlas.is_active() && tilemap.atlas.texture()) {
    int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
    if (tiles_per_row > 0) {
      int tile_x = (current_tile % tiles_per_row) * tilemap.tile_size.x;
      int tile_y = (current_tile / tiles_per_row) * tilemap.tile_size.y;

      if (tile_x >= 0 && tile_x < tilemap.atlas.width() && tile_y >= 0 &&
          tile_y < tilemap.atlas.height()) {
        ImVec2 uv0 =
            ImVec2(static_cast<float>(tile_x) / tilemap.atlas.width(),
                   static_cast<float>(tile_y) / tilemap.atlas.height());
        ImVec2 uv1 = ImVec2(static_cast<float>(tile_x + tilemap.tile_size.x) /
                                tilemap.atlas.width(),
                            static_cast<float>(tile_y + tilemap.tile_size.y) /
                                tilemap.atlas.height());

        rt.draw_list->AddImage(
            (ImTextureID)(intptr_t)tilemap.atlas.texture(),
            ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
            ImVec2(origin.x + paint_pos.x + scaled_size,
                   origin.y + paint_pos.y + scaled_size),
            uv0, uv1);
      }
    }
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    if (out_drawn_pos)
      *out_drawn_pos = paint_pos;
    return true;
  }

  return false;
}

bool DrawTileSelector(const CanvasRuntime& rt, int size, int size_y,
                      ImVec2* out_selected_pos) {
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(rt.canvas_p0.x + rt.scrolling.x,
                      rt.canvas_p0.y + rt.scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  if (size_y == 0) {
    size_y = size;
  }

  if (rt.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ImVec2 painter_pos =
        AlignPosToGridHelper(mouse_pos, static_cast<float>(size));
    if (out_selected_pos)
      *out_selected_pos = painter_pos;
  }

  if (rt.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return true;
  }

  return false;
}

void DrawSelectRect(const CanvasRuntime& rt, int current_map, int tile_size,
                    float scale, CanvasSelection& selection) {
  if (!rt.draw_list)
    return;

  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 origin(rt.canvas_p0.x + rt.scrolling.x,
                      rt.canvas_p0.y + rt.scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  static ImVec2 drag_start_pos;
  const float scaled_size = tile_size * scale;
  static bool dragging = false;
  constexpr int small_map_size = 0x200;
  constexpr uint32_t kWhite = IM_COL32(255, 255, 255, 255);

  if (!rt.hovered) {
    return;
  }

  int superY, superX;
  if (current_map < 0x40) {
    superY = current_map / 8;
    superX = current_map % 8;
  } else if (current_map < 0x80) {
    superY = (current_map - 0x40) / 8;
    superX = (current_map - 0x40) % 8;
  } else {
    superY = (current_map - 0x80) / 8;
    superX = (current_map - 0x80) % 8;
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImVec2 painter_pos = AlignPosToGridHelper(mouse_pos, scaled_size);
    int world_x = static_cast<int>(painter_pos.x / scale);
    int world_y = static_cast<int>(painter_pos.y / scale);

    auto tile16_x = (world_x % small_map_size) / (small_map_size / 0x20);
    auto tile16_y = (world_y % small_map_size) / (small_map_size / 0x20);

    int index_x = superX * 0x20 + tile16_x;
    int index_y = superY * 0x20 + tile16_y;
    selection.selected_tile_pos =
        ImVec2(static_cast<float>(index_x), static_cast<float>(index_y));
    selection.selected_points.clear();
    selection.select_rect_active = false;

    drag_start_pos = AlignPosToGridHelper(mouse_pos, scaled_size);
  }

  ImVec2 drag_end_pos = AlignPosToGridHelper(mouse_pos, scaled_size);
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    auto start =
        ImVec2(origin.x + drag_start_pos.x, origin.y + drag_start_pos.y);
    auto end = ImVec2(origin.x + drag_end_pos.x + scaled_size,
                      origin.y + drag_end_pos.y + scaled_size);
    rt.draw_list->AddRect(start, end, kWhite);
    dragging = true;
  }

  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    dragging = false;

    constexpr int tile16_size = 16;
    int start_x = static_cast<int>(std::floor(drag_start_pos.x / scaled_size)) *
                  tile16_size;
    int start_y = static_cast<int>(std::floor(drag_start_pos.y / scaled_size)) *
                  tile16_size;
    int end_x = static_cast<int>(std::floor(drag_end_pos.x / scaled_size)) *
                tile16_size;
    int end_y = static_cast<int>(std::floor(drag_end_pos.y / scaled_size)) *
                tile16_size;

    if (start_x > end_x)
      std::swap(start_x, end_x);
    if (start_y > end_y)
      std::swap(start_y, end_y);

    selection.selected_tiles.clear();
    selection.selected_tiles.reserve(
        static_cast<size_t>(((end_x - start_x) / tile16_size + 1) *
                            ((end_y - start_y) / tile16_size + 1)));

    constexpr int tiles_per_local_map = small_map_size / 16;

    for (int y = start_y; y <= end_y; y += tile16_size) {
      for (int x = start_x; x <= end_x; x += tile16_size) {
        int local_map_x = (x / small_map_size) % 8;
        int local_map_y = (y / small_map_size) % 8;
        int tile16_x = (x % small_map_size) / tile16_size;
        int tile16_y = (y % small_map_size) / tile16_size;
        int index_x = local_map_x * tiles_per_local_map + tile16_x;
        int index_y = local_map_y * tiles_per_local_map + tile16_y;
        selection.selected_tiles.emplace_back(static_cast<float>(index_x),
                                              static_cast<float>(index_y));
      }
    }

    selection.selected_points.clear();
    selection.selected_points.push_back(
        ImVec2(drag_start_pos.x / scale, drag_start_pos.y / scale));
    selection.selected_points.push_back(
        ImVec2(drag_end_pos.x / scale, drag_end_pos.y / scale));
    selection.select_rect_active = true;
  }
}

}  // namespace yaze::gui

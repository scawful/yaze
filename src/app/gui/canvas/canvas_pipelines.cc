#include "app/gui/canvas/canvas.h"

#include <algorithm>

#include "app/gui/canvas/canvas_utils.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"

namespace yaze::gui {

void BeginCanvas(Canvas& canvas, ImVec2 child_size) {
  BeginPadding(1);

  ImVec2 effective_size = child_size;
  if (child_size.x == 0 && child_size.y == 0) {
    if (canvas.IsAutoResize()) {
      effective_size = canvas.GetPreferredSize();
    } else {
      effective_size = canvas.GetCurrentSize();
    }
  }

  ImGui::BeginChild(canvas.canvas_id().c_str(), effective_size, true,
                    ImGuiWindowFlags_NoScrollbar);
  canvas.DrawBackground();
  EndPadding();
  canvas.DrawContextMenu();
}

void EndCanvas(Canvas& canvas) {
  canvas.DrawGrid();
  canvas.DrawOverlay();
  ImGui::EndChild();
}

CanvasRuntime BeginCanvas(Canvas& canvas, const CanvasFrameOptions& options) {
  canvas.Begin(options);

  CanvasRuntime runtime;
  runtime.draw_list = canvas.draw_list();
  runtime.canvas_p0 = canvas.zero_point();
  runtime.canvas_sz = canvas.canvas_size();
  runtime.scrolling = canvas.scrolling();
  runtime.hovered = canvas.IsMouseHovering();
  runtime.grid_step = options.grid_step.value_or(canvas.GetGridStep());
  runtime.scale = canvas.GetGlobalScale();
  runtime.content_size = canvas.GetCurrentSize();

  return runtime;
}

void EndCanvas(Canvas& canvas, CanvasRuntime& /*runtime*/,
               const CanvasFrameOptions& options) {
  canvas.End(options);
}

ZoomToFitResult ComputeZoomToFit(ImVec2 content_px, ImVec2 canvas_px,
                                 float padding_px) {
  ZoomToFitResult result;
  result.scale = 1.0f;
  result.scroll = ImVec2(0, 0);

  if (content_px.x <= 0 || content_px.y <= 0) {
    return result;
  }

  float available_x = canvas_px.x - padding_px * 2;
  float available_y = canvas_px.y - padding_px * 2;

  if (available_x <= 0 || available_y <= 0) {
    return result;
  }

  float scale_x = available_x / content_px.x;
  float scale_y = available_y / content_px.y;
  result.scale = std::min(scale_x, scale_y);

  float scaled_w = content_px.x * result.scale;
  float scaled_h = content_px.y * result.scale;
  result.scroll.x = (canvas_px.x - scaled_w) / 2.0f;
  result.scroll.y = (canvas_px.y - scaled_h) / 2.0f;

  return result;
}

ImVec2 ClampScroll(ImVec2 scroll, ImVec2 content_px, ImVec2 canvas_px) {
  float max_scroll_x = std::max(0.0f, content_px.x - canvas_px.x);
  float max_scroll_y = std::max(0.0f, content_px.y - canvas_px.y);

  return ImVec2(std::clamp(scroll.x, -max_scroll_x, 0.0f),
                std::clamp(scroll.y, -max_scroll_y, 0.0f));
}

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, gfx::BitmapTable& graphics_bin) {
  Canvas canvas;
  if (ImGuiID child_id =
          ImGui::GetID((ImTextureID)(intptr_t)(intptr_t)canvas_id);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    canvas.DrawBackground(ImVec2(width + 1, num_sheets_to_load * height + 1));
    canvas.DrawContextMenu();
    if (is_loaded) {
      for (const auto& [key, value] : graphics_bin) {
        if (!value || !value->texture()) {
          continue;
        }
        int offset = height * (key + 1);
        int top_left_y = canvas.zero_point().y + 2;
        if (key >= 1) {
          top_left_y = canvas.zero_point().y + height * key;
        }
        canvas.draw_list()->AddImage(
            (ImTextureID)(intptr_t)value->texture(),
            ImVec2(canvas.zero_point().x + 2, top_left_y),
            ImVec2(canvas.zero_point().x + 0x100,
                   canvas.zero_point().y + offset));
      }
    }
    canvas.DrawTileSelector(tile_size);
    canvas.DrawGrid(tile_size);
    canvas.DrawOverlay();
    canvas.RenderPersistentPopups();
  }
  ImGui::EndChild();
}

void BitmapCanvasPipeline(Canvas& canvas, gfx::Bitmap& bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id) {
  auto draw_canvas = [&](Canvas& c, gfx::Bitmap& bmp, int w, int h, int ts,
                         bool loaded) {
    c.DrawBackground(ImVec2(w + 1, h + 1));
    c.DrawContextMenu();
    c.DrawBitmap(bmp, 2, loaded);
    c.DrawTileSelector(ts);
    c.DrawGrid(ts);
    c.DrawOverlay();
    c.RenderPersistentPopups();
  };

  if (scrollbar) {
    if (ImGuiID child_id =
            ImGui::GetID((ImTextureID)(intptr_t)(intptr_t)canvas_id);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      draw_canvas(canvas, bitmap, width, height, tile_size, is_loaded);
    }
    ImGui::EndChild();
  } else {
    draw_canvas(canvas, bitmap, width, height, tile_size, is_loaded);
  }
}

void TableCanvasPipeline(Canvas& canvas, gfx::Bitmap& bitmap,
                         const std::string& label, bool auto_resize) {
  canvas.SetAutoResize(auto_resize);

  if (auto_resize && bitmap.is_active()) {
    ImVec2 content_size = ImVec2(bitmap.width(), bitmap.height());
    ImVec2 preferred_size = CanvasUtils::CalculatePreferredCanvasSize(
        content_size, canvas.GetGlobalScale());
    canvas.SetCanvasSize(preferred_size);
  }

  if (canvas.BeginTableCanvas(label)) {
    canvas.DrawBackground();
    canvas.DrawContextMenu();

    if (bitmap.is_active()) {
      canvas.DrawBitmap(bitmap, 2, 2, canvas.GetGlobalScale());
    }

    canvas.DrawGrid();
    canvas.DrawOverlay();
    canvas.RenderPersistentPopups();
  }
  canvas.EndTableCanvas();
}

}  // namespace yaze::gui

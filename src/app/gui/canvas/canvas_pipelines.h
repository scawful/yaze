#ifndef YAZE_APP_GUI_CANVAS_CANVAS_PIPELINES_H
#define YAZE_APP_GUI_CANVAS_CANVAS_PIPELINES_H

#include <string>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas_types.h"

namespace yaze {
namespace gui {

class Canvas;

void BeginCanvas(Canvas& canvas, ImVec2 child_size = ImVec2(0, 0));
void EndCanvas(Canvas& canvas);

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, gfx::BitmapTable& graphics_bin);

void BitmapCanvasPipeline(Canvas& canvas, gfx::Bitmap& bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id);

void TableCanvasPipeline(Canvas& canvas, gfx::Bitmap& bitmap,
                         const std::string& label = "",
                         bool auto_resize = true);

CanvasRuntime BeginCanvas(Canvas& canvas, const CanvasFrameOptions& options);
void EndCanvas(Canvas& canvas, CanvasRuntime& runtime,
               const CanvasFrameOptions& options);

ZoomToFitResult ComputeZoomToFit(ImVec2 content_px, ImVec2 canvas_px,
                                 float padding_px);
ImVec2 ClampScroll(ImVec2 scroll, ImVec2 content_px, ImVec2 canvas_px);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_PIPELINES_H

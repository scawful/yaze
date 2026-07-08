#ifndef YAZE_APP_GUI_CANVAS_CANVAS_DRAW_H
#define YAZE_APP_GUI_CANVAS_CANVAS_DRAW_H

#include <string>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas_state.h"
#include "app/gui/canvas/canvas_types.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap,
                int border_offset = 2, float scale = 1.0f);
void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap, int x_offset,
                int y_offset, float scale = 1.0f, int alpha = 255);
void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap, ImVec2 dest_pos,
                ImVec2 dest_size, ImVec2 src_pos, ImVec2 src_size);
void DrawBitmap(const CanvasRuntime& rt, gfx::Bitmap& bitmap,
                const BitmapDrawOpts& opts);

void DrawBitmapPreview(const CanvasRuntime& rt, gfx::Bitmap& bitmap,
                       const BitmapPreviewOptions& options);
bool RenderPreviewPanel(const CanvasRuntime& rt, gfx::Bitmap& bmp,
                        const PreviewPanelOpts& opts);

bool DrawTilemapPainter(const CanvasRuntime& rt, gfx::Tilemap& tilemap,
                        int current_tile, ImVec2* out_drawn_pos);
bool DrawTileSelector(const CanvasRuntime& rt, int size, int size_y,
                      ImVec2* out_selected_pos);
void DrawSelectRect(const CanvasRuntime& rt, int current_map, int tile_size,
                    float scale, CanvasSelection& selection);

void DrawRect(const CanvasRuntime& rt, int x, int y, int w, int h,
              ImVec4 color);
void DrawText(const CanvasRuntime& rt, const std::string& text, int x, int y);
void DrawOutline(const CanvasRuntime& rt, int x, int y, int w, int h,
                 ImU32 color = IM_COL32(255, 255, 255, 200));

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_DRAW_H

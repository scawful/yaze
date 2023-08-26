#ifndef YAZE_APP_CORE_PIPELINE_H
#define YAZE_APP_CORE_PIPELINE_H

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <functional>
#include <optional>

#include "absl/strings/string_view.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace core {

void SelectablePalettePipeline(uint64_t& palette_id, bool& refresh_graphics,
                               gfx::SNESPalette& palette);

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, gfx::BitmapTable& graphics_bin);

void ButtonPipe(absl::string_view button_text, std::function<void()> callback);

void BitmapCanvasPipeline(gui::Canvas& canvas, gfx::Bitmap& bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id);

void BuildAndRenderBitmapPipeline(int width, int height, int depth, Bytes data,
                                  ROM& z3_rom, gfx::Bitmap& bitmap,
                                  gfx::SNESPalette& palette);

void FileDialogPipeline(absl::string_view display_key,
                        absl::string_view file_extensions,
                        std::optional<absl::string_view> button_text,
                        std::function<void()> callback);

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif
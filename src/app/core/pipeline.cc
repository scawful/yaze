#include "pipeline.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <functional>
#include <optional>

#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace core {

void ButtonPipe(absl::string_view button_text, std::function<void()> callback) {
  if (ImGui::Button(button_text.data())) {
    callback();
  }
}

void BitmapCanvasPipeline(int width, int height, int tile_size, int canvas_id,
                          bool is_loaded, gfx::Bitmap& bitmap) {
  gui::Canvas canvas;

  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)canvas_id);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    canvas.DrawBackground(ImVec2(width + 1, height + 1));
    canvas.DrawContextMenu();
    canvas.DrawBitmap(bitmap, 2, is_loaded);
    canvas.DrawTileSelector(tile_size);
    canvas.DrawGrid(tile_size);
    canvas.DrawOverlay();
  }
  ImGui::EndChild();
}

void BuildAndRenderBitmapPipeline(int width, int height, int depth, Bytes data,
                                  ROM& z3_rom, gfx::Bitmap& bitmap,
                                  gfx::SNESPalette& palette) {
  bitmap.Create(width, height, depth, data);
  bitmap.ApplyPalette(palette);
  z3_rom.RenderBitmap(&bitmap);
}

void FileDialogPipeline(absl::string_view display_key,
                        absl::string_view file_extensions,
                        std::optional<absl::string_view> button_text,
                        std::function<void()> callback) {
  if (button_text.has_value() && ImGui::Button(button_text->data())) {
    ImGuiFileDialog::Instance()->OpenDialog(display_key.data(), "Choose File",
                                            file_extensions.data(), ".");
  }

  if (ImGuiFileDialog::Instance()->Display(display_key.data())) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      callback();
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

}  // namespace core
}  // namespace app
}  // namespace yaze
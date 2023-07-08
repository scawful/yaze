#ifndef YAZE_APP_EDITOR_GRAPHICS_EDITOR_H
#define YAZE_APP_EDITOR_GRAPHICS_EDITOR_H

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

class GraphicsEditor {
 public:
  absl::Status Update();
  void SetupROM(ROM &rom) { rom_ = rom; }

 private:
  absl::Status DrawImport();
  absl::Status DecompressImportData(char *filePath, int offset, int size);

  ROM rom_;
  ROM temp_rom;

  gfx::Bitmap bitmap_;

  gui::Canvas import_canvas_;

  Bytes import_data_;

  bool gfx_loaded_ = false;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_EDITOR_H
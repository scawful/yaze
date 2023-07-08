#include "app/editor/graphics_editor.h"

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

absl::Status GraphicsEditor::Update() {
  DrawImport();

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawImport() {
  static int offset = 0;
  static int size = 0;
  static char filePath[256] = "";

  ImGui::InputText("File", filePath, sizeof(filePath));

  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Browse")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportDlgKey", "Choose File",
                                            ".bin\0", ".");
  }

  // Draw the file dialog
  if (ImGuiFileDialog::Instance()->Display("ImportDlgKey")) {
    // If the user made a selection, copy the filename to the filePath buffer
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(filePath, ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(filePath));
    }

    // Close the modal
    ImGuiFileDialog::Instance()->Close();
  }

  gui::InputHex("Offset", &offset);
  gui::InputHex("Size ", &size);

  if (ImGui::Button("Import")) {
    if (strlen(filePath) > 0) {
      // Add your importing code here, using filePath and offset as parameters
      RETURN_IF_ERROR(DecompressImportData(filePath, offset, size))
    } else {
      // Show an error message if no file has been selected
      ImGui::Text("Please select a file before importing.");
    }
  }

  import_canvas_.DrawBackground(ImVec2(0x100 + 1, (8192 * 2) + 1));
  import_canvas_.DrawContextMenu();
  import_canvas_.DrawBitmap(bitmap_, 2, gfx_loaded_);
  import_canvas_.DrawTileSelector(32);
  import_canvas_.DrawGrid(32.0f);
  import_canvas_.DrawOverlay();

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressImportData(char *filePath, int offset,
                                                  int size) {
  RETURN_IF_ERROR(temp_rom.LoadFromFile(filePath))
  ASSIGN_OR_RETURN(import_data_, temp_rom.DecompressGraphics(offset, size))

  bitmap_.Create(core::kTilesheetWidth, core::kTilesheetHeight * 0x10,
                 core::kTilesheetDepth, import_data_.data());
  rom_.RenderBitmap(&bitmap_);

  gfx_loaded_ = true;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
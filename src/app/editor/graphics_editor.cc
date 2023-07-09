#include "app/editor/graphics_editor.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status GraphicsEditor::Update() {
  if (ImGui::BeginTable("#gfxEditTable", 2, gfx_edit_flags, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Bin Importer", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Graphics Manager");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    RETURN_IF_ERROR(DrawImport())

    ImGui::TableNextColumn();

    ImGui::EndTable();
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawImport() {
  static int size = 0;
  static char filePath[256] = "";
  static bool open = false;

  ImGui::SetNextItemWidth(350.f);
  ImGui::InputText("File", filePath, sizeof(filePath));

  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Browse")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportDlgKey", "Choose File",
                                            ".bin\0.hex\0", ".");
  }

  // Draw the file dialog
  if (ImGuiFileDialog::Instance()->Display("ImportDlgKey")) {
    // If the user made a selection, copy the filename to the filePath buffer
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(filePath, ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(filePath));
      RETURN_IF_ERROR(temp_rom_.LoadFromFile(filePath))
      open = true;
    }

    // Close the modal
    ImGuiFileDialog::Instance()->Close();
  }

  if (open) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow(filePath, (void *)&temp_rom_, temp_rom_.size());
  }

  gui::InputHex("Offset", &current_offset_);
  gui::InputHex("Size ", &size);

  if (ImGui::Button("Super Donkey")) {
    current_offset_ = 0x98219;
    size = 0x30000;
  }

  if (ImGui::Button("Import")) {
    if (strlen(filePath) > 0) {
      // Add your importing code here, using filePath and offset as parameters
      RETURN_IF_ERROR(DecompressImportData(size))
    } else {
      // Show an error message if no file has been selected
      ImGui::Text("Please select a file before importing.");
    }
  }

  if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)2);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    import_canvas_.DrawBackground(ImVec2(0x100 + 1, (8192 * 2) + 1));
    import_canvas_.DrawContextMenu();
    import_canvas_.DrawBitmap(bitmap_, 2, gfx_loaded_);
    import_canvas_.DrawTileSelector(32);
    import_canvas_.DrawGrid(32.0f);
    import_canvas_.DrawOverlay();
  }
  ImGui::EndChild();

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, temp_rom_.Decompress(current_offset_, size))
  std::cout << "Size of import data" << import_data_.size() << std::endl;

  Bytes new_sheet;

  bitmap_.Create(core::kTilesheetWidth, core::kTilesheetHeight,
                 core::kTilesheetDepth, import_data_);
  rom_.RenderBitmap(&bitmap_);

  gfx_loaded_ = true;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
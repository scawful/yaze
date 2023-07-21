#include "app/editor/graphics_editor.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status GraphicsEditor::Update() {
  BEGIN_TABLE("#gfxEditTable", 2, gfx_edit_flags)
  SETUP_COLUMN("Graphics Manager")
  SETUP_COLUMN("Memory Editor")
  TABLE_HEADERS()
  NEXT_COLUMN()

  TAB_BAR("##GfxTabBar")
  TAB_ITEM("File Import")
  RETURN_IF_ERROR(DrawFileImport())
  END_TAB_ITEM()
  TAB_ITEM("Clipboard Import")
  RETURN_IF_ERROR(DrawClipboardImport())
  END_TAB_ITEM()
  END_TAB_BAR()
  ImGui::Separator();
  ImGui::Text("Graphics");
  ImGui::Separator();
  RETURN_IF_ERROR(DrawDecompressedData())

  NEXT_COLUMN()
  RETURN_IF_ERROR(DrawMemoryEditor())
  END_TABLE()

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  static int size = 0;

  ImGui::InputText("File", file_path_, sizeof(file_path_));
  ImGui::SameLine();
  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Browse")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportDlgKey", "Choose File",
                                            ".bin\0.hex\0", ".");
  }

  // Draw the file dialog
  if (ImGuiFileDialog::Instance()->Display("ImportDlgKey")) {
    // If the user made a selection, copy the filename to the file_path_ buffer
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(file_path_,
              ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(file_path_));
      RETURN_IF_ERROR(temp_rom_.LoadFromFile(file_path_))
      is_open_ = true;
    }

    // Close the modal
    ImGuiFileDialog::Instance()->Close();
  }

  gui::InputHex("Offset", &current_offset_);
  gui::InputHex("Size ", &size);
  gui::InputHex("Palette ", &current_palette_);

  if (ImGui::Button("Super Donkey Offsets")) {
    current_offset_ = 0x98219;
    size = 0x30000;
  }

  ImGui::SameLine();

  if (ImGui::Button("Import")) {
    if (strlen(file_path_) > 0) {
      // Add your importing code here, using file_path_ and offset as parameters
      RETURN_IF_ERROR(DecompressImportData(size))
    } else {
      // Show an error message if no file has been selected
      ImGui::Text("Please select a file before importing.");
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawClipboardImport() {
  static Bytes clipboard_data;

  ImGui::Button("Paste");

  if (!is_open_) {
    clipboard_data.resize(0x1000);
    for (int i = 0; i < 0x1000; i++) clipboard_data.push_back(0x00);
    RETURN_IF_ERROR(temp_rom_.LoadFromBytes(clipboard_data))
    is_open_ = true;
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawMemoryEditor() {
  std::string title = "Memory Editor";
  if (is_open_) {
    static MemoryEditor mem_edit;
    // mem_edit.DrawWindow(title.data(), (void *)&temp_rom_, temp_rom_.size());
    mem_edit.DrawContents(temp_rom_.data(), temp_rom_.size());
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawDecompressedData() {
  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)2);
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
  std::cout << "Size of import data " << import_data_.size() << std::endl;

  Bytes new_sheet;
  bitmap_.Create(core::kTilesheetWidth, 0x2000, core::kTilesheetDepth,
                 import_data_.data(), size);

  if (rom_.isLoaded()) {
    auto palette_group = rom_.GetPaletteGroup("ow_main");
    palette_ = palette_group.palettes[current_palette_];
    bitmap_.ApplyPalette(palette_);
  }

  rom_.RenderBitmap(&bitmap_);
  gfx_loaded_ = true;

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressSuperDonkey() {
  for (const auto& offset : kSuperDonkeyTiles) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(auto decompressed_data,
                     temp_rom_.Decompress(offset_value, 0x1000))
    
  }

  for (const auto& offset : kSuperDonkeySprites) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(auto decompressed_data,
                     temp_rom_.Decompress(offset_value, 0x1000))
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
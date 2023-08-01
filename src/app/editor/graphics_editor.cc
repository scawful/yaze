#include "app/editor/graphics_editor.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status GraphicsEditor::Update() {
  BEGIN_TABLE("#gfxEditTable", 3, kGfxEditFlags)
  SETUP_COLUMN("Graphics Manager")
  SETUP_COLUMN("Memory Editor")
  SETUP_COLUMN("Preview")
  TABLE_HEADERS()
  NEXT_COLUMN()

  status_ = DrawFileImport();
  status_ = DrawPaletteControls();

  NEXT_COLUMN()
  RETURN_IF_ERROR(DrawMemoryEditor())

  NEXT_COLUMN()
  if (super_donkey_) {
    status_ = DrawGraphicsBin();
  } else {
    status_ = DrawDecompressedData();
  }
  END_TABLE()

  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  static int size = 0;

  gui::TextWithSeparators("File Import");

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

  if (ImGui::Button("Import")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressImportData(size))
    } else {
      return absl::InvalidArgumentError(
          "Please select a file before importing.");
    }
  }

  if (ImGui::Button("Import Super Donkey Full")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressSuperDonkey())
    } else {
      return absl::InvalidArgumentError(
          "Please select `super_donkey_1.bin` before importing.");
    }
  }

  gui::TextWithSeparators("Clipboard Import");
  RETURN_IF_ERROR(DrawClipboardImport())

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("Palette");
  gui::InputHex("Palette Index", &current_palette_index_);

  ImGui::Combo("Palette", &current_palette_, kPaletteGroupAddressesKeys,
               IM_ARRAYSIZE(kPaletteGroupAddressesKeys));

  ImGui::InputText("COL File", col_file_path_, sizeof(col_file_path_));
  ImGui::SameLine();
  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Browse")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportColKey", "Choose File",
                                            ".col\0", ".");
  }

  if (ImGuiFileDialog::Instance()->Display("ImportColKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(col_file_path_,
              ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(col_file_path_));
      RETURN_IF_ERROR(temp_rom_.LoadFromFile(col_file_path_, /*z3_load=*/false))
      auto col_data_ = gfx::GetColFileData(temp_rom_.data());
      col_file_palette_ = gfx::SNESPalette(col_data_);
      col_file_ = true;
      is_open_ = true;
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (col_file_palette_.size() != 0) {
    palette_editor_.DrawPortablePalette(col_file_palette_);
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawClipboardImport() {
  static Bytes clipboard_data;

  if (ImGui::Button("Paste from Clipboard")) {
    const char* text = ImGui::GetClipboardText();
    if (text) {
      clipboard_data = Bytes(text, text + strlen(text));
      ImGui::MemFree((void*)text);
      RETURN_IF_ERROR(temp_rom_.LoadFromBytes(clipboard_data))
      is_open_ = true;
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawMemoryEditor() {
  std::string title = "Memory Editor";
  if (is_open_) {
    static MemoryEditor mem_edit;
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
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     temp_rom_.data(), current_offset_, size))
  std::cout << "Size of import data " << import_data_.size() << std::endl;

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bitmap_.Create(core::kTilesheetWidth, 0x2000, core::kTilesheetDepth,
                 converted_sheet.data(), size);

  if (rom_.isLoaded()) {
    auto palette_group = rom_.GetPaletteGroup("ow_main");
    palette_ = palette_group.palettes[current_palette_];
    if (col_file_) {
      bitmap_.ApplyPalette(col_file_palette_);
    } else {
      bitmap_.ApplyPalette(palette_);
    }
  }

  rom_.RenderBitmap(&bitmap_);
  gfx_loaded_ = true;

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawGraphicsBin() {
  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)3);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    super_donkey_canvas_.DrawBackground(
        ImVec2(0x100 + 1, num_sheets_to_load_ * 0x40 + 1));
    super_donkey_canvas_.DrawContextMenu();
    if (super_donkey_) {
      for (const auto& [key, value] : graphics_bin_) {
        int offset = 0x40 * (key + 1);
        int top_left_y = super_donkey_canvas_.GetZeroPoint().y + 2;
        if (key >= 1) {
          top_left_y = super_donkey_canvas_.GetZeroPoint().y + 0x40 * key;
        }
        super_donkey_canvas_.GetDrawList()->AddImage(
            (void*)value.GetTexture(),
            ImVec2(super_donkey_canvas_.GetZeroPoint().x + 2, top_left_y),
            ImVec2(super_donkey_canvas_.GetZeroPoint().x + 0x100,
                   super_donkey_canvas_.GetZeroPoint().y + offset));
      }
    }
    super_donkey_canvas_.DrawGrid(16.0f);
    super_donkey_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressSuperDonkey() {
  if (rom_.isLoaded()) {
    auto palette_group =
        rom_.GetPaletteGroup(kPaletteGroupAddressesKeys[current_palette_]);
    palette_ = palette_group.palettes[current_palette_index_];
  }

  int i = 0;
  for (const auto& offset : kSuperDonkeyTiles) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(
        auto decompressed_data,
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000))
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    graphics_bin_[i] =
        gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                    core::kTilesheetDepth, converted_sheet.data(), 0x1000);
    graphics_bin_[i].ApplyPalette(palette_);

    rom_.RenderBitmap(&graphics_bin_[i]);
    i++;
  }

  for (const auto& offset : kSuperDonkeySprites) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(
        auto decompressed_data,
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000))
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    graphics_bin_[i] =
        gfx::Bitmap(core::kTilesheetWidth, core::kTilesheetHeight,
                    core::kTilesheetDepth, converted_sheet.data(), 0x1000);
    graphics_bin_[i].ApplyPalette(palette_);

    rom_.RenderBitmap(&graphics_bin_[i]);
    i++;
  }
  super_donkey_ = true;
  num_sheets_to_load_ = i;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
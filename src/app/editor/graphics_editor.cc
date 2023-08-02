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
  RETURN_IF_ERROR(DrawToolset())

  if (open_memory_editor_) {
    ImGui::Begin("Memory Editor", &open_memory_editor_);
    RETURN_IF_ERROR(DrawMemoryEditor())
    ImGui::End();
  }

  BEGIN_TABLE("#gfxEditTable", 3, kGfxEditFlags)
  SETUP_COLUMN("Graphics (BIN, CGX, SCR)")
  SETUP_COLUMN("Palette (COL)")
  SETUP_COLUMN("Preview")
  TABLE_HEADERS()
  NEXT_COLUMN()

  status_ = DrawCgxImport();
  status_ = DrawFileImport();
  status_ = DrawExperimentalFeatures();

  NEXT_COLUMN()
  status_ = DrawPaletteControls();

  NEXT_COLUMN()
  if (super_donkey_) {
    status_ = DrawGraphicsBin();
  } else if (cgx_loaded_ && col_file_) {
    status_ = DrawCgxViewer();
  } else {
    status_ = DrawDecompressedData();
  }
  END_TABLE()

  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawToolset() {
  if (ImGui::BeginTable("GraphicsToolset", 2, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    for (const auto& name : kGfxToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_MEMORY)) {
      open_memory_editor_ = true;
    }

    TEXT_COLUMN("Open Memory Editor")  // Separator

    ImGui::EndTable();
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawCgxImport() {
  gui::TextWithSeparators("Cgx Import");

  ImGui::InputText("##CGXFile", cgx_file_name_, sizeof(cgx_file_name_));
  ImGui::SameLine();
  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Open CGX")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportCgxKey", "Choose File",
                                            ".CGX,.cgx\0", ".");
  }

  if (ImGuiFileDialog::Instance()->Display("ImportCgxKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(cgx_file_path_,
              ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(cgx_file_path_));
      strncpy(cgx_file_name_,
              ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
              sizeof(cgx_file_name_));
      RETURN_IF_ERROR(temp_rom_.LoadFromFile(cgx_file_path_, /*z3_load=*/false))
      cgx_viewer_.LoadCgx(temp_rom_);
      auto all_tiles_data = cgx_viewer_.GetCgxData();
      cgx_bitmap_.Create(core::kTilesheetWidth, 8192, core::kTilesheetDepth,
                         all_tiles_data.data(), all_tiles_data.size());
      rom_.RenderBitmap(&cgx_bitmap_);
      is_open_ = true;
      cgx_loaded_ = true;
    }
    ImGuiFileDialog::Instance()->Close();
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  static int size = 0;

  gui::TextWithSeparators("Clipboard Import");
  RETURN_IF_ERROR(DrawClipboardImport())

  gui::TextWithSeparators("BIN Import");

  ImGui::InputText("##ROMFile", file_path_, sizeof(file_path_));
  ImGui::SameLine();
  // Open the file dialog when the user clicks the "Browse" button
  if (ImGui::Button("Open BIN")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportDlgKey", "Choose File",
                                            ".bin,.hex\0", ".");
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

  if (ImGui::Button("BIN Import")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressImportData(size))
    } else {
      return absl::InvalidArgumentError(
          "Please select a file before importing.");
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  ImGui::InputText("##ColFile", col_file_name_, sizeof(col_file_name_));
  ImGui::SameLine();

  if (ImGui::Button("Open COL")) {
    ImGuiFileDialog::Instance()->OpenDialog("ImportColKey", "Choose File",
                                            ".COL,.col", ".");
  }

  if (ImGuiFileDialog::Instance()->Display("ImportColKey")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      strncpy(col_file_path_,
              ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
              sizeof(col_file_path_));
      strncpy(col_file_name_,
              ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
              sizeof(col_file_name_));
      RETURN_IF_ERROR(temp_rom_.LoadFromFile(col_file_path_, /*z3_load=*/false))
      auto col_data_ = gfx::GetColFileData(temp_rom_.data());
      col_file_palette_ = gfx::SNESPalette(col_data_);
      col_file_ = true;
      is_open_ = true;

      if (cgx_loaded_) {
        cgx_bitmap_.ApplyPalette(col_file_palette_);
        rom_.RenderBitmap(&cgx_bitmap_);
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (rom_.isLoaded()) {
    gui::TextWithSeparators("ROM Palette");
    gui::InputHex("Palette Index", &current_palette_index_);
    ImGui::Combo("Palette", &current_palette_, kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
  }

  if (col_file_palette_.size() != 0) {
    ImGuiFileDialog::Instance()->prDrawFileListView(ImVec2(0, 200));
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
      open_memory_editor_ = true;
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawExperimentalFeatures() {
  gui::TextWithSeparators("Experimental");
  if (ImGui::Button("Import Super Donkey Full")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressSuperDonkey())
    } else {
      return absl::InvalidArgumentError(
          "Please select `super_donkey_1.bin` before importing.");
    }
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawMemoryEditor() {
  std::string title = "Memory Editor";
  if (is_open_) {
    static MemoryEditor mem_edit;
    mem_edit.DrawWindow(title.c_str(), temp_rom_.data(), temp_rom_.size());
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawCgxViewer() {
  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)5);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    import_canvas_.DrawBackground(ImVec2(0x100 + 1, (8192 * 2) + 1));
    import_canvas_.DrawContextMenu();
    import_canvas_.DrawBitmap(cgx_bitmap_, 2, cgx_loaded_);
    import_canvas_.DrawTileSelector(32);
    import_canvas_.DrawGrid(32.0f);
    import_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
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

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     temp_rom_.data(), current_offset_, size))

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
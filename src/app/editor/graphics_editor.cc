#include "app/editor/graphics_editor.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/pipeline.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/scad_format.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"

//

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

  BEGIN_TABLE("#gfxEditTable", 4, kGfxEditFlags)
  SETUP_COLUMN("File Import (BIN, CGX, ROM)")
  SETUP_COLUMN("Palette (COL)")
  ImGui::TableSetupColumn("Tilemaps and Objects (SCR, PNL, OBJ)",
                          ImGuiTableColumnFlags_WidthFixed);
  SETUP_COLUMN("Graphics Preview")
  TABLE_HEADERS()
  NEXT_COLUMN() {
    status_ = DrawCgxImport();
    status_ = DrawClipboardImport();
    status_ = DrawFileImport();
    status_ = DrawExperimentalFeatures();
  }

  NEXT_COLUMN() { status_ = DrawPaletteControls(); }

  NEXT_COLUMN()
  core::BitmapCanvasPipeline(scr_canvas_, scr_bitmap_, 0x200, 0x200, 0x20,
                             scr_loaded_, false, 0);
  status_ = DrawScrImport();

  NEXT_COLUMN()
  if (super_donkey_) {
    if (refresh_graphics_) {
      for (int i = 0; i < graphics_bin_.size(); i++) {
        graphics_bin_[i].ApplyPalette(
            col_file_palette_group_[current_palette_index_]);
        rom()->UpdateBitmap(&graphics_bin_[i]);
      }
      refresh_graphics_ = false;
    }
    // Load the full graphics space from `super_donkey_1.bin`
    core::GraphicsBinCanvasPipeline(0x100, 0x40, 0x20, num_sheets_to_load_, 3,
                                    super_donkey_, graphics_bin_);
  } else if (cgx_loaded_ && col_file_) {
    // Load the CGX graphics
    core::BitmapCanvasPipeline(import_canvas_, cgx_bitmap_, 0x100, 16384, 0x20,
                               cgx_loaded_, true, 5);
  } else {
    // Load the BIN/Clipboard Graphics
    core::BitmapCanvasPipeline(import_canvas_, bitmap_, 0x100, 16384, 0x20,
                               gfx_loaded_, true, 2);
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
      if (!open_memory_editor_) {
        open_memory_editor_ = true;
      } else {
        open_memory_editor_ = false;
      }
    }

    TEXT_COLUMN("Open Memory Editor")  // Separator

    ImGui::EndTable();
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawCgxImport() {
  gui::TextWithSeparators("Cgx Import");
  ImGui::InputInt("BPP", &current_bpp_);

  ImGui::InputText("##CGXFile", cgx_file_name_, sizeof(cgx_file_name_));
  ImGui::SameLine();

  core::FileDialogPipeline("ImportCgxKey", ".CGX,.cgx\0", "Open CGX", [this]() {
    strncpy(cgx_file_path_,
            ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
            sizeof(cgx_file_path_));
    strncpy(cgx_file_name_,
            ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
            sizeof(cgx_file_name_));
    is_open_ = true;
    cgx_loaded_ = true;
  });
  core::ButtonPipe("Copy CGX Path",
                   [this]() { ImGui::SetClipboardText(cgx_file_path_); });

  core::ButtonPipe("Load CGX Data", [this]() {
    status_ = gfx::LoadCgx(current_bpp_, cgx_file_path_, cgx_data_,
                           decoded_cgx_, extra_cgx_data_);
    cgx_bitmap_.Create(0x80, 0x200, 8, decoded_cgx_);
    if (col_file_) {
      cgx_bitmap_.ApplyPalette(decoded_col_);
      rom()->RenderBitmap(&cgx_bitmap_);
    }
  });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawScrImport() {
  ImGui::InputText("##ScrFile", scr_file_name_, sizeof(scr_file_name_));

  core::FileDialogPipeline(
      "ImportScrKey", ".SCR,.scr,.BAK\0", "Open SCR", [this]() {
        strncpy(scr_file_path_,
                ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
                sizeof(scr_file_path_));
        strncpy(scr_file_name_,
                ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
                sizeof(scr_file_name_));
        is_open_ = true;
        scr_loaded_ = true;
      });

  ImGui::InputInt("SCR Mod", &scr_mod_value_);

  core::ButtonPipe("Load Scr Data", [this]() {
    status_ = gfx::LoadScr(scr_file_path_, scr_mod_value_, scr_data_);

    decoded_scr_data_.resize(0x100 * 0x100);
    status_ = gfx::DrawScrWithCgx(current_bpp_, scr_data_, decoded_scr_data_,
                                  decoded_cgx_);

    scr_bitmap_.Create(0x100, 0x100, 8, decoded_scr_data_);
    if (scr_loaded_) {
      scr_bitmap_.ApplyPalette(decoded_col_);
      rom()->RenderBitmap(&scr_bitmap_);
    }
  });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  ImGui::InputText("##ColFile", col_file_name_, sizeof(col_file_name_));
  ImGui::SameLine();

  core::FileDialogPipeline(
      "ImportColKey", ".COL,.col,.BAK,.bak\0", "Open COL", [this]() {
        strncpy(col_file_path_,
                ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
                sizeof(col_file_path_));
        strncpy(col_file_name_,
                ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
                sizeof(col_file_name_));
        status_ = temp_rom_.LoadFromFile(col_file_path_,
                                         /*z3_load=*/false);
        auto col_data_ = gfx::GetColFileData(temp_rom_.data());
        if (col_file_palette_group_.size() != 0) {
          col_file_palette_group_.Clear();
        }
        col_file_palette_group_ = gfx::CreatePaletteGroupFromColFile(col_data_);
        col_file_palette_ = gfx::SNESPalette(col_data_);

        // gigaleak dev format based code
        decoded_col_ = gfx::DecodeColFile(col_file_path_);
        col_file_ = true;
        is_open_ = true;
      });

  core::ButtonPipe("Copy COL Path",
                   [this]() { ImGui::SetClipboardText(col_file_path_); });

  if (rom()->isLoaded()) {
    gui::TextWithSeparators("ROM Palette");
    gui::InputHex("Palette Index", &current_palette_index_);
    ImGui::Combo("Palette", &current_palette_, kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
  }

  if (col_file_palette_.size() != 0) {
    core::SelectablePalettePipeline(current_palette_index_, refresh_graphics_,
                                    col_file_palette_);
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawObjImport() {
  gui::TextWithSeparators("OBJ Import");

  ImGui::InputText("##ObjFile", obj_file_path_, sizeof(obj_file_path_));
  ImGui::SameLine();

  core::FileDialogPipeline(
      "ImportObjKey", ".obj,.OBJ,.bak,.BAK\0", "Open OBJ", [this]() {
        strncpy(file_path_,
                ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
                sizeof(file_path_));
        status_ = temp_rom_.LoadFromFile(file_path_);
        is_open_ = true;
      });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawTilemapImport() {
  gui::TextWithSeparators("Tilemap Import");

  ImGui::InputText("##TMapFile", tilemap_file_path_,
                   sizeof(tilemap_file_path_));
  ImGui::SameLine();

  core::FileDialogPipeline(
      "ImportTilemapKey", ".DAT,.dat,.BIN,.bin,.hex,.HEX\0", "Open Tilemap",
      [this]() {
        strncpy(tilemap_file_path_,
                ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
                sizeof(tilemap_file_path_));
        status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);

        // Extract the high and low bytes from the file.
        auto decomp_sheet = gfx::lc_lz2::DecompressV2(
            tilemap_rom_.data(), gfx::lc_lz2::kNintendoMode1);
        tilemap_loaded_ = true;
        is_open_ = true;
      });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  gui::TextWithSeparators("BIN Import");

  ImGui::InputText("##ROMFile", file_path_, sizeof(file_path_));
  ImGui::SameLine();

  core::FileDialogPipeline("ImportDlgKey", ".bin,.hex\0", "Open BIN", [this]() {
    strncpy(file_path_, ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
            sizeof(file_path_));
    status_ = temp_rom_.LoadFromFile(file_path_);
    is_open_ = true;
  });

  core::ButtonPipe("Copy File Path",
                   [this]() { ImGui::SetClipboardText(file_path_); });

  gui::InputHex("BIN Offset", &current_offset_);
  gui::InputHex("BIN Size", &bin_size_);

  if (ImGui::Button("Decompress BIN")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressImportData(bin_size_))
    } else {
      return absl::InvalidArgumentError(
          "Please select a file before importing.");
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawClipboardImport() {
  gui::TextWithSeparators("Clipboard Import");
  core::ButtonPipe("Paste from Clipboard", [this]() {
    const char* text = ImGui::GetClipboardText();
    if (text) {
      const auto clipboard_data = Bytes(text, text + strlen(text));
      ImGui::MemFree((void*)text);
      status_ = temp_rom_.LoadFromBytes(clipboard_data);
      is_open_ = true;
      open_memory_editor_ = true;
    }
  });
  gui::InputHex("Offset", &clipboard_offset_);
  gui::InputHex("Size", &clipboard_size_);
  gui::InputHex("Num Sheets", &num_sheets_to_load_);

  core::ButtonPipe("Decompress Clipboard Data", [this]() {
    if (temp_rom_.isLoaded()) {
      status_ = DecompressImportData(0x40000);
    } else {
      status_ = absl::InvalidArgumentError(
          "Please paste data into the clipboard before "
          "decompressing.");
    }
  });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawExperimentalFeatures() {
  gui::TextWithSeparators("Experimental");
  if (ImGui::Button("Decompress Super Donkey Full")) {
    if (strlen(file_path_) > 0) {
      RETURN_IF_ERROR(DecompressSuperDonkey())
    } else {
      return absl::InvalidArgumentError(
          "Please select `super_donkey_1.bin` before "
          "importing.");
    }
  }
  ImGui::SetItemTooltip(
      "Requires `super_donkey_1.bin` to be imported under the "
      "BIN import section.");
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

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     temp_rom_.data(), current_offset_, size))

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bitmap_.Create(core::kTilesheetWidth, 0x2000, core::kTilesheetDepth,
                 converted_sheet.data(), size);

  if (rom()->isLoaded()) {
    auto palette_group = rom()->GetPaletteGroup("ow_main");
    z3_rom_palette_ = palette_group[current_palette_];
    if (col_file_) {
      bitmap_.ApplyPalette(col_file_palette_);
    } else {
      bitmap_.ApplyPalette(z3_rom_palette_);
    }
  }

  rom()->RenderBitmap(&bitmap_);
  gfx_loaded_ = true;

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressSuperDonkey() {
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
                    core::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      graphics_bin_[i].ApplyPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      auto palette_group =
          rom()->GetPaletteGroup(kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = palette_group[current_palette_index_];
      graphics_bin_[i].ApplyPalette(z3_rom_palette_);
    }

    rom()->RenderBitmap(&graphics_bin_[i]);
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
                    core::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      graphics_bin_[i].ApplyPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      auto palette_group =
          rom()->GetPaletteGroup(kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = palette_group[current_palette_index_];
      graphics_bin_[i].ApplyPalette(z3_rom_palette_);
    }

    rom()->RenderBitmap(&graphics_bin_[i]);
    i++;
  }
  super_donkey_ = true;
  num_sheets_to_load_ = i;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
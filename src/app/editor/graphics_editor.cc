#include "app/editor/graphics_editor.h"

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/scad_format.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/gui/pipeline.h"
#include "app/gui/style.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::Button;
using ImGui::InputInt;
using ImGui::InputText;
using ImGui::SameLine;

absl::Status GraphicsEditor::Update() {
  TAB_BAR("##TabBar")
  status_ = UpdateGfxEdit();
  status_ = UpdateScadView();
  status_ = UpdateLinkGfxView();
  END_TAB_BAR()
  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateGfxEdit() {
  TAB_ITEM("Graphics Editor")

  if (ImGui::BeginTable("##GfxEditTable", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Hideable |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    for (const auto& name : kGfxEditColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableHeadersRow();

    NEXT_COLUMN();

    NEXT_COLUMN() {
      if (rom()->isLoaded()) {
        status_ = UpdateGfxTabView();
      }
    }

    NEXT_COLUMN() {
      if (rom()->isLoaded()) {
        status_ = UpdatePaletteColumn();
      }
    }
  }
  ImGui::EndTable();

  END_TAB_ITEM()
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateGfxSheetList() {
  ImGui::BeginChild(
      "##GfxEditChild", ImVec2(0, 0), true,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  for (auto& [key, value] : rom()->bitmap_manager()) {
    ImGui::BeginChild(absl::StrFormat("##GfxSheet%02X", key).c_str(),
                      ImVec2(0x100 + 1, 0x40 + 1), true,
                      ImGuiWindowFlags_NoDecoration);
    ImGui::PopStyleVar();
    gui::Canvas graphics_bin_canvas_;
    graphics_bin_canvas_.UpdateEvent(
        [&]() {
          if (value.get()->IsActive()) {
            auto texture = value.get()->texture();
            graphics_bin_canvas_.GetDrawList()->AddImage(
                (void*)texture,
                ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 2,
                       graphics_bin_canvas_.GetZeroPoint().y + 2),
                ImVec2(graphics_bin_canvas_.GetZeroPoint().x +
                           value.get()->width() * sheet_scale_,
                       graphics_bin_canvas_.GetZeroPoint().y +
                           value.get()->height() * sheet_scale_));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
              current_sheet_ = key;
              open_sheets_.insert(key);
            }

            // Add a slightly transparent rectangle behind the text
            ImVec2 textPos(graphics_bin_canvas_.GetZeroPoint().x + 2,
                           graphics_bin_canvas_.GetZeroPoint().y + 2);
            ImVec2 textSize =
                ImGui::CalcTextSize(absl::StrFormat("%02X", key).c_str());
            ImVec2 rectMin(textPos.x, textPos.y);
            ImVec2 rectMax(textPos.x + textSize.x, textPos.y + textSize.y);
            graphics_bin_canvas_.GetDrawList()->AddRectFilled(
                rectMin, rectMax, IM_COL32(0, 125, 0, 128));
            graphics_bin_canvas_.GetDrawList()->AddText(
                textPos, IM_COL32(125, 255, 125, 255),
                absl::StrFormat("%02X", key).c_str());
          }
        },
        ImVec2(0x100 + 1, 0x40 + 1), 0x20, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::EndChild();
  }
  ImGui::PopStyleVar();
  ImGui::EndChild();
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateGfxTabView() {
  static int next_tab_id = 0;

  if (ImGui::BeginTabBar("##GfxEditTabBar",
                         ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_Reorderable |
                             ImGuiTabBarFlags_FittingPolicyResizeDown |
                             ImGuiTabBarFlags_TabListPopupButton)) {
    // TODO: Manage the room that is being added to the tab bar.
    if (ImGui::TabItemButton(
            "+", ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                     ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
                     ImGuiTableFlags_BordersV)) {
      open_sheets_.insert(next_tab_id++);  // Add new tab
    }

    // Submit our regular tabs
    for (auto& each : open_sheets_) {
      bool open = true;

      if (ImGui::BeginTabItem(absl::StrFormat("%d", each).c_str(), &open,
                              ImGuiTabItemFlags_None)) {
        ImGui::BeginChild(
            absl::StrFormat("##GfxEditPaletteChild%d", each).c_str(),
            ImVec2(0, 0), true,
            ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysVerticalScrollbar);
        current_sheet_canvas_.Update(*rom()->bitmap_manager()[each],
                                     ImVec2(0x100 + 1, 0x40 + 1), 0x20, 4.0f,
                                     16.0f);
        ImGui::EndChild();

        ImGui::EndTabItem();
      }

      if (!open) release_queue_.push(each);
    }

    ImGui::EndTabBar();
  }
  ImGui::Separator();
  while (!release_queue_.empty()) {
    auto each = release_queue_.top();
    open_sheets_.erase(each);
    release_queue_.pop();
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdatePaletteColumn() {
  if (rom()->isLoaded()) {
    gui::TextWithSeparators("ROM Palette");
    ImGui::Combo("Palette", (int*)&edit_palette_group_,
                 kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
    gui::InputHex("Palette Index", &edit_palette_index_);
  }

  auto palette_group = rom()->mutable_palette_group(
      kPaletteGroupAddressesKeys[edit_palette_group_])
                           [edit_palette_group_index_];
  auto palette = palette_group[edit_palette_index_];
  gui::SelectablePalettePipeline(edit_palette_index_, refresh_graphics_,
                                 palette);

  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateLinkGfxView() {
  TAB_ITEM("Player Animations")

  const auto link_gfx_offset = 0x80000;
  const auto link_gfx_length = 0x7000;

  // Load Links graphics from the ROM
  RETURN_IF_ERROR(rom()->LoadLinkGraphics());

  // Split it into the pose data frames

  // Create an animation step display for the poses

  // Allow the user to modify the frames used in an anim step

  // LinkOAM_AnimationSteps:
  // #_0D85FB

  END_TAB_ITEM()
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateScadView() {
  TAB_ITEM("Prototype")

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
  gui::BitmapCanvasPipeline(scr_canvas_, scr_bitmap_, 0x200, 0x200, 0x20,
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
    gui::GraphicsBinCanvasPipeline(0x100, 0x40, 0x20, num_sheets_to_load_, 3,
                                   super_donkey_, graphics_bin_);
  } else if (cgx_loaded_ && col_file_) {
    // Load the CGX graphics
    gui::BitmapCanvasPipeline(import_canvas_, cgx_bitmap_, 0x100, 16384, 0x20,
                              cgx_loaded_, true, 5);
  } else {
    // Load the BIN/Clipboard Graphics
    gui::BitmapCanvasPipeline(import_canvas_, bin_bitmap_, 0x100, 16384, 0x20,
                              gfx_loaded_, true, 2);
  }
  END_TABLE()

  END_TAB_ITEM()
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawToolset() {
  if (ImGui::BeginTable("GraphicsToolset", 2, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    for (const auto& name : kGfxToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    ImGui::TableNextColumn();
    if (Button(ICON_MD_MEMORY)) {
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
  InputInt("BPP", &current_bpp_);

  InputText("##CGXFile", cgx_file_name_, sizeof(cgx_file_name_));
  SameLine();

  gui::FileDialogPipeline("ImportCgxKey", ".CGX,.cgx\0", "Open CGX", [this]() {
    strncpy(cgx_file_path_,
            ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
            sizeof(cgx_file_path_));
    strncpy(cgx_file_name_,
            ImGuiFileDialog::Instance()->GetCurrentFileName().c_str(),
            sizeof(cgx_file_name_));
    is_open_ = true;
    cgx_loaded_ = true;
  });
  gui::ButtonPipe("Copy CGX Path",
                  [this]() { ImGui::SetClipboardText(cgx_file_path_); });

  gui::ButtonPipe("Load CGX Data", [this]() {
    status_ = gfx::LoadCgx(current_bpp_, cgx_file_path_, cgx_data_,
                           decoded_cgx_, extra_cgx_data_);

    cgx_bitmap_.InitializeFromData(0x80, 0x200, 8, decoded_cgx_);
    if (col_file_) {
      cgx_bitmap_.ApplyPalette(decoded_col_);
      rom()->RenderBitmap(&cgx_bitmap_);
    }
  });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawScrImport() {
  InputText("##ScrFile", scr_file_name_, sizeof(scr_file_name_));

  gui::FileDialogPipeline(
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

  InputInt("SCR Mod", &scr_mod_value_);

  gui::ButtonPipe("Load Scr Data", [this]() {
    status_ = gfx::LoadScr(scr_file_path_, scr_mod_value_, scr_data_);

    decoded_scr_data_.resize(0x100 * 0x100);
    status_ = gfx::DrawScrWithCgx(current_bpp_, scr_data_, decoded_scr_data_,
                                  decoded_cgx_);

    scr_bitmap_.InitializeFromData(0x100, 0x100, 8, decoded_scr_data_);
    if (scr_loaded_) {
      scr_bitmap_.ApplyPalette(decoded_col_);
      rom()->RenderBitmap(&scr_bitmap_);
    }
  });

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  InputText("##ColFile", col_file_name_, sizeof(col_file_name_));
  SameLine();

  gui::FileDialogPipeline(
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

  gui::ButtonPipe("Copy COL Path",
                  [this]() { ImGui::SetClipboardText(col_file_path_); });

  if (rom()->isLoaded()) {
    gui::TextWithSeparators("ROM Palette");
    gui::InputHex("Palette Index", &current_palette_index_);
    ImGui::Combo("Palette", &current_palette_, kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
  }

  if (col_file_palette_.size() != 0) {
    gui::SelectablePalettePipeline(current_palette_index_, refresh_graphics_,
                                   col_file_palette_);
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawObjImport() {
  gui::TextWithSeparators("OBJ Import");

  InputText("##ObjFile", obj_file_path_, sizeof(obj_file_path_));
  SameLine();

  gui::FileDialogPipeline(
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

  InputText("##TMapFile", tilemap_file_path_, sizeof(tilemap_file_path_));
  SameLine();

  gui::FileDialogPipeline(
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

  InputText("##ROMFile", file_path_, sizeof(file_path_));
  SameLine();

  gui::FileDialogPipeline("ImportDlgKey", ".bin,.hex\0", "Open BIN", [this]() {
    strncpy(file_path_, ImGuiFileDialog::Instance()->GetFilePathName().c_str(),
            sizeof(file_path_));
    status_ = temp_rom_.LoadFromFile(file_path_);
    is_open_ = true;
  });

  gui::ButtonPipe("Copy File Path",
                  [this]() { ImGui::SetClipboardText(file_path_); });

  gui::InputHex("BIN Offset", &current_offset_);
  gui::InputHex("BIN Size", &bin_size_);

  if (Button("Decompress BIN")) {
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
  gui::ButtonPipe("Paste from Clipboard", [this]() {
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

  gui::ButtonPipe("Decompress Clipboard Data", [this]() {
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
  if (Button("Decompress Super Donkey Full")) {
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
  bin_bitmap_.Create(core::kTilesheetWidth, 0x2000, core::kTilesheetDepth,
                     converted_sheet);

  if (rom()->isLoaded()) {
    auto palette_group = rom()->GetPaletteGroup("ow_main");
    z3_rom_palette_ = palette_group[current_palette_];
    if (col_file_) {
      bin_bitmap_.ApplyPalette(col_file_palette_);
    } else {
      bin_bitmap_.ApplyPalette(z3_rom_palette_);
    }
  }

  rom()->RenderBitmap(&bin_bitmap_);
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
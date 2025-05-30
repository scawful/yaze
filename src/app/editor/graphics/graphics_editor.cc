#include "graphics_editor.h"

#include <filesystem>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "app/core/platform/clipboard.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/compression.h"
#include "app/gfx/scad_format.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/modules/asset_browser.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_memory_editor.h"

namespace yaze {
namespace editor {

using core::Renderer;

using gfx::kPaletteGroupAddressesKeys;
using ImGui::Button;
using ImGui::InputInt;
using ImGui::InputText;
using ImGui::SameLine;
using ImGui::TableNextColumn;

constexpr ImGuiTableFlags kGfxEditTableFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
    ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
    ImGuiTableFlags_SizingFixedFit;

void GraphicsEditor::Initialize() {}

absl::Status GraphicsEditor::Load() { return absl::OkStatus(); }

absl::Status GraphicsEditor::Update() {
  if (ImGui::BeginTabBar("##TabBar")) {
    status_ = UpdateGfxEdit();
    if (ImGui::BeginTabItem("Sheet Browser")) {
      if (asset_browser_.Initialized == false) {
        asset_browser_.Initialize(gfx::Arena::Get().gfx_sheets());
      }
      asset_browser_.Draw(gfx::Arena::Get().gfx_sheets());
      ImGui::EndTabItem();
    }
    status_ = UpdateScadView();
    status_ = UpdateLinkGfxView();
    ImGui::EndTabBar();
  }
  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateGfxEdit() {
  if (ImGui::BeginTabItem("Sheet Editor")) {
    if (ImGui::BeginTable("##GfxEditTable", 3, kGfxEditTableFlags,
                          ImVec2(0, 0))) {
      for (const auto& name :
           {"Tilesheets", "Current Graphics", "Palette Controls"})
        ImGui::TableSetupColumn(name);

      ImGui::TableHeadersRow();
      ImGui::TableNextColumn();
      status_ = UpdateGfxSheetList();

      ImGui::TableNextColumn();
      if (rom()->is_loaded()) {
        DrawGfxEditToolset();
        status_ = UpdateGfxTabView();
      }

      ImGui::TableNextColumn();
      if (rom()->is_loaded()) {
        status_ = UpdatePaletteColumn();
      }
    }
    ImGui::EndTable();

    ImGui::EndTabItem();
  }
  return absl::OkStatus();
}

void GraphicsEditor::DrawGfxEditToolset() {
  if (ImGui::BeginTable("##GfxEditToolset", 9, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    for (const auto& name :
         {"Select", "Pencil", "Fill", "Copy Sheet", "Paste Sheet", "Zoom Out",
          "Zoom In", "Current Color", "Tile Size"})
      ImGui::TableSetupColumn(name);

    TableNextColumn();
    if (Button(ICON_MD_SELECT_ALL)) {
      gfx_edit_mode_ = GfxEditMode::kSelect;
    }

    TableNextColumn();
    if (Button(ICON_MD_DRAW)) {
      gfx_edit_mode_ = GfxEditMode::kPencil;
    }
    HOVER_HINT("Draw with current color");

    TableNextColumn();
    if (Button(ICON_MD_FORMAT_COLOR_FILL)) {
      gfx_edit_mode_ = GfxEditMode::kFill;
    }
    HOVER_HINT("Fill with current color");

    TableNextColumn();
    if (Button(ICON_MD_CONTENT_COPY)) {
      std::vector<uint8_t> png_data =
          gfx::Arena::Get().gfx_sheets().at(current_sheet_).GetPngData();
      core::CopyImageToClipboard(png_data);
    }
    HOVER_HINT("Copy to Clipboard");

    TableNextColumn();
    if (Button(ICON_MD_CONTENT_PASTE)) {
      std::vector<uint8_t> png_data;
      int width, height;
      core::GetImageFromClipboard(png_data, width, height);
      if (png_data.size() > 0) {
        gfx::Arena::Get()
            .mutable_gfx_sheets()
            ->at(current_sheet_)
            .Create(width, height, 8, png_data);
        Renderer::Get().UpdateBitmap(
            &gfx::Arena::Get().mutable_gfx_sheets()->at(current_sheet_));
      }
    }
    HOVER_HINT("Paste from Clipboard");

    TableNextColumn();
    if (Button(ICON_MD_ZOOM_OUT)) {
      if (current_scale_ >= 0.0f) {
        current_scale_ -= 1.0f;
      }
    }

    TableNextColumn();
    if (Button(ICON_MD_ZOOM_IN)) {
      if (current_scale_ <= 16.0f) {
        current_scale_ += 1.0f;
      }
    }

    TableNextColumn();
    auto bitmap = gfx::Arena::Get().gfx_sheets()[current_sheet_];
    auto palette = bitmap.palette();
    for (int i = 0; i < palette.size(); i++) {
      ImGui::SameLine();
      auto color =
          ImVec4(palette[i].rgb().x / 255.0f, palette[i].rgb().y / 255.0f,
                 palette[i].rgb().z / 255.0f, 255.0f);
      if (ImGui::ColorButton(absl::StrFormat("Palette Color %d", i).c_str(),
                             color)) {
        current_color_ = color;
      }
    }

    TableNextColumn();
    gui::InputHexByte("Tile Size", &tile_size_);

    ImGui::EndTable();
  }
}

absl::Status GraphicsEditor::UpdateGfxSheetList() {
  ImGui::BeginChild(
      "##GfxEditChild", ImVec2(0, 0), true,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  // TODO: Update the interaction for multi select on sheets
  static ImGuiSelectionBasicStorage selection;
  ImGuiMultiSelectFlags flags =
      ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect1d;
  ImGuiMultiSelectIO* ms_io =
      ImGui::BeginMultiSelect(flags, selection.Size, kNumGfxSheets);
  selection.ApplyRequests(ms_io);
  ImGuiListClipper clipper;
  clipper.Begin(kNumGfxSheets);
  if (ms_io->RangeSrcItem != -1)
    clipper.IncludeItemByIndex(
        (int)ms_io->RangeSrcItem);  // Ensure RangeSrc item is not clipped.

  int key = 0;
  for (auto& value : gfx::Arena::Get().gfx_sheets()) {
    ImGui::BeginChild(absl::StrFormat("##GfxSheet%02X", key).c_str(),
                      ImVec2(0x100 + 1, 0x40 + 1), true,
                      ImGuiWindowFlags_NoDecoration);
    ImGui::PopStyleVar();

    graphics_bin_canvas_.DrawBackground(ImVec2(0x100 + 1, 0x40 + 1));
    graphics_bin_canvas_.DrawContextMenu();
    if (value.is_active()) {
      auto texture = value.texture();
      graphics_bin_canvas_.draw_list()->AddImage(
          (ImTextureID)(intptr_t)texture,
          ImVec2(graphics_bin_canvas_.zero_point().x + 2,
                 graphics_bin_canvas_.zero_point().y + 2),
          ImVec2(graphics_bin_canvas_.zero_point().x +
                     value.width() * sheet_scale_,
                 graphics_bin_canvas_.zero_point().y +
                     value.height() * sheet_scale_));

      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        current_sheet_ = key;
        open_sheets_.insert(key);
      }

      // Add a slightly transparent rectangle behind the text
      ImVec2 text_pos(graphics_bin_canvas_.zero_point().x + 2,
                      graphics_bin_canvas_.zero_point().y + 2);
      ImVec2 text_size =
          ImGui::CalcTextSize(absl::StrFormat("%02X", key).c_str());
      ImVec2 rent_min(text_pos.x, text_pos.y);
      ImVec2 rent_max(text_pos.x + text_size.x, text_pos.y + text_size.y);

      graphics_bin_canvas_.draw_list()->AddRectFilled(rent_min, rent_max,
                                                      IM_COL32(0, 125, 0, 128));

      graphics_bin_canvas_.draw_list()->AddText(
          text_pos, IM_COL32(125, 255, 125, 255),
          absl::StrFormat("%02X", key).c_str());

      key++;
    }
    graphics_bin_canvas_.DrawGrid(16.0f);
    graphics_bin_canvas_.DrawOverlay();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::EndChild();
  }
  ImGui::PopStyleVar();
  ms_io = ImGui::EndMultiSelect();
  selection.ApplyRequests(ms_io);
  ImGui::EndChild();
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateGfxTabView() {
  static int next_tab_id = 0;
  constexpr ImGuiTabBarFlags kGfxEditTabBarFlags =
      ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable |
      ImGuiTabBarFlags_FittingPolicyResizeDown |
      ImGuiTabBarFlags_TabListPopupButton;

  if (ImGui::BeginTabBar("##GfxEditTabBar", kGfxEditTabBarFlags)) {
    if (ImGui::TabItemButton(ICON_MD_ADD, ImGuiTabItemFlags_Trailing |
                                              ImGuiTabItemFlags_NoTooltip)) {
      open_sheets_.insert(next_tab_id++);
    }

    for (auto& sheet_id : open_sheets_) {
      bool open = true;
      if (ImGui::BeginTabItem(absl::StrFormat("%d", sheet_id).c_str(), &open,
                              ImGuiTabItemFlags_None)) {
        current_sheet_ = sheet_id;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
          release_queue_.push(sheet_id);
        }
        if (ImGui::IsItemHovered()) {
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            release_queue_.push(sheet_id);
            child_window_sheets_.insert(sheet_id);
          }
        }

        const auto child_id =
            absl::StrFormat("##GfxEditPaletteChildWindow%d", sheet_id);
        ImGui::BeginChild(child_id.c_str(), ImVec2(0, 0), true,
                          ImGuiWindowFlags_NoDecoration |
                              ImGuiWindowFlags_AlwaysVerticalScrollbar |
                              ImGuiWindowFlags_AlwaysHorizontalScrollbar);

        gfx::Bitmap& current_bitmap =
            gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);

        auto draw_tile_event = [&]() {
          current_sheet_canvas_.DrawTileOnBitmap(tile_size_, &current_bitmap,
                                                 current_color_);
          Renderer::Get().UpdateBitmap(&current_bitmap);
        };

        current_sheet_canvas_.UpdateColorPainter(
            gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id),
            current_color_, draw_tile_event, tile_size_, current_scale_);

        ImGui::EndChild();
        ImGui::EndTabItem();
      }

      if (!open) release_queue_.push(sheet_id);
    }

    ImGui::EndTabBar();
  }

  // Release any tabs that were closed
  while (!release_queue_.empty()) {
    auto sheet_id = release_queue_.top();
    open_sheets_.erase(sheet_id);
    release_queue_.pop();
  }

  // Draw any child windows that were created
  if (!child_window_sheets_.empty()) {
    int id_to_release = -1;
    for (const auto& id : child_window_sheets_) {
      bool active = true;
      ImGui::SetNextWindowPos(ImGui::GetIO().MousePos, ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(0x100 + 1 * 16, 0x40 + 1 * 16),
                               ImGuiCond_Once);
      ImGui::Begin(absl::StrFormat("##GfxEditPaletteChildWindow%d", id).c_str(),
                   &active, ImGuiWindowFlags_AlwaysUseWindowPadding);
      current_sheet_ = id;
      //  ImVec2(0x100, 0x40),
      current_sheet_canvas_.UpdateColorPainter(
          gfx::Arena::Get().mutable_gfx_sheets()->at(id), current_color_,
          [&]() {

          },
          tile_size_, current_scale_);
      ImGui::End();

      if (active == false) {
        id_to_release = id;
      }
    }
    if (id_to_release != -1) {
      child_window_sheets_.erase(id_to_release);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdatePaletteColumn() {
  if (rom()->is_loaded()) {
    auto palette_group = *rom()->palette_group().get_group(
        kPaletteGroupAddressesKeys[edit_palette_group_name_index_]);
    auto palette = palette_group.palette(edit_palette_index_);
    gui::TextWithSeparators("ROM Palette");
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("Palette Group", (int*)&edit_palette_group_name_index_,
                 kPaletteGroupAddressesKeys,
                 IM_ARRAYSIZE(kPaletteGroupAddressesKeys));
    ImGui::SetNextItemWidth(100.f);
    gui::InputHex("Palette Group Index", &edit_palette_index_);

    gui::SelectablePalettePipeline(edit_palette_sub_index_, refresh_graphics_,
                                   palette);

    if (refresh_graphics_ && !open_sheets_.empty()) {
      gfx::Arena::Get()
          .mutable_gfx_sheets()
          ->data()[current_sheet_]
          .SetPaletteWithTransparent(palette, edit_palette_sub_index_);
      Renderer::Get().UpdateBitmap(
          &gfx::Arena::Get().mutable_gfx_sheets()->data()[current_sheet_]);
      refresh_graphics_ = false;
    }
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::UpdateLinkGfxView() {
  TAB_ITEM("Player Animations")

  if (ImGui::BeginTable("##PlayerAnimationTable", 3, kGfxEditTableFlags,
                        ImVec2(0, 0))) {
    for (const auto& name : {"Canvas", "Animation Steps", "Properties"})
      ImGui::TableSetupColumn(name);

    ImGui::TableHeadersRow();

    ImGui::TableNextColumn();
    link_canvas_.DrawBackground();
    link_canvas_.DrawGrid(16.0f);

    int i = 0;
    for (auto& link_sheet : link_sheets_) {
      int x_offset = 0;
      int y_offset = gfx::kTilesheetHeight * i * 4;
      link_canvas_.DrawContextMenu();
      link_canvas_.DrawBitmap(link_sheet, x_offset, y_offset, 4);
      i++;
    }
    link_canvas_.DrawOverlay();
    link_canvas_.DrawGrid();

    ImGui::TableNextColumn();
    ImGui::Text("Placeholder");

    ImGui::TableNextColumn();
    if (ImGui::Button("Load Link Graphics (Experimental)")) {
      if (rom()->is_loaded()) {
        // Load Links graphics from the ROM
        ASSIGN_OR_RETURN(link_sheets_, LoadLinkGraphics(*rom()));

        // Split it into the pose data frames
        // Create an animation step display for the poses
        // Allow the user to modify the frames used in an anim step
        // LinkOAM_AnimationSteps:
        // #_0D85FB
      }
    }
  }
  ImGui::EndTable();

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

  constexpr ImGuiTableFlags kGfxEditFlags = ImGuiTableFlags_Reorderable |
                                            ImGuiTableFlags_Resizable |
                                            ImGuiTableFlags_SizingStretchSame;

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
    // TODO: Implement the Super Donkey 1 graphics decompression
    // if (refresh_graphics_) {
    //   for (int i = 0; i < kNumGfxSheets; i++) {
    //     status_ = graphics_bin_[i].SetPalette(
    //         col_file_palette_group_[current_palette_index_]);
    //     Renderer::Get().UpdateBitmap(&graphics_bin_[i]);
    //   }
    //   refresh_graphics_ = false;
    // }
    // Load the full graphics space from `super_donkey_1.bin`
    // gui::GraphicsBinCanvasPipeline(0x100, 0x40, 0x20, num_sheets_to_load_, 3,
    //                                super_donkey_, graphics_bin_);
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
  static constexpr absl::string_view kGfxToolsetColumnNames[] = {
      "#memoryEditor",
  };

  if (ImGui::BeginTable("GraphicsToolset", 1, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    for (const auto& name : kGfxToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    TableNextColumn();
    if (Button(absl::StrCat(ICON_MD_MEMORY, "Open Memory Editor").c_str())) {
      if (!open_memory_editor_) {
        open_memory_editor_ = true;
      } else {
        open_memory_editor_ = false;
      }
    }

    ImGui::EndTable();
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawCgxImport() {
  gui::TextWithSeparators("Cgx Import");
  InputInt("BPP", &current_bpp_);

  InputText("##CGXFile", &cgx_file_name_);
  SameLine();

  if (ImGui::Button("Open CGX")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    cgx_file_name_ = filename;
    cgx_file_path_ = std::filesystem::absolute(filename).string();
    is_open_ = true;
    cgx_loaded_ = true;
  }

  if (ImGui::Button("Copy CGX Path")) {
    ImGui::SetClipboardText(cgx_file_path_.c_str());
  }

  if (ImGui::Button("Load CGX Data")) {
    status_ = gfx::LoadCgx(current_bpp_, cgx_file_path_, cgx_data_,
                           decoded_cgx_, extra_cgx_data_);

    cgx_bitmap_.Create(0x80, 0x200, 8, decoded_cgx_);
    if (col_file_) {
      cgx_bitmap_.SetPalette(decoded_col_);
      Renderer::Get().RenderBitmap(&cgx_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawScrImport() {
  InputText("##ScrFile", &scr_file_name_);

  if (ImGui::Button("Open SCR")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    scr_file_name_ = filename;
    scr_file_path_ = std::filesystem::absolute(filename).string();
    is_open_ = true;
    scr_loaded_ = true;
  }

  InputInt("SCR Mod", &scr_mod_value_);

  if (ImGui::Button("Load Scr Data")) {
    status_ = gfx::LoadScr(scr_file_path_, scr_mod_value_, scr_data_);

    decoded_scr_data_.resize(0x100 * 0x100);
    status_ = gfx::DrawScrWithCgx(current_bpp_, scr_data_, decoded_scr_data_,
                                  decoded_cgx_);

    scr_bitmap_.Create(0x100, 0x100, 8, decoded_scr_data_);
    if (scr_loaded_) {
      scr_bitmap_.SetPalette(decoded_col_);
      Renderer::Get().RenderBitmap(&scr_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  InputText("##ColFile", &col_file_name_);
  SameLine();

  if (ImGui::Button("Open COL")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    col_file_name_ = filename;
    col_file_path_ = std::filesystem::absolute(filename).string();
    status_ = temp_rom_.LoadFromFile(col_file_path_,
                                     /*z3_load=*/false);
    auto col_data_ = gfx::GetColFileData(temp_rom_.mutable_data());
    if (col_file_palette_group_.size() != 0) {
      col_file_palette_group_.clear();
    }
    auto col_file_palette_group_status =
        gfx::CreatePaletteGroupFromColFile(col_data_);
    if (col_file_palette_group_status.ok()) {
      col_file_palette_group_ = col_file_palette_group_status.value();
    }
    col_file_palette_ = gfx::SnesPalette(col_data_);

    // gigaleak dev format based code
    decoded_col_ = gfx::DecodeColFile(col_file_path_);
    col_file_ = true;
    is_open_ = true;
  }
  HOVER_HINT(".COL, .BAK");

  if (ImGui::Button("Copy Col Path")) {
    ImGui::SetClipboardText(col_file_path_.c_str());
  }

  if (rom()->is_loaded()) {
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

  InputText("##ObjFile", &obj_file_path_);
  SameLine();

  if (ImGui::Button("Open OBJ")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    obj_file_path_ = std::filesystem::absolute(filename).string();
    status_ = temp_rom_.LoadFromFile(obj_file_path_);
    is_open_ = true;
    obj_loaded_ = true;
  }
  HOVER_HINT(".OBJ, .BAK");

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawTilemapImport() {
  gui::TextWithSeparators("Tilemap Import");

  InputText("##TMapFile", &tilemap_file_path_);
  SameLine();

  if (ImGui::Button("Open Tilemap")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    tilemap_file_path_ = std::filesystem::absolute(filename).string();
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);

    // Extract the high and low bytes from the file.
    auto decomp_sheet = gfx::lc_lz2::DecompressV2(tilemap_rom_.data(),
                                                  gfx::lc_lz2::kNintendoMode1);
    tilemap_loaded_ = true;
    is_open_ = true;
  }
  HOVER_HINT(".DAT, .BIN, .HEX");

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawFileImport() {
  gui::TextWithSeparators("BIN Import");

  InputText("##ROMFile", &file_path_);
  SameLine();

  if (ImGui::Button("Open BIN")) {
    auto filename = core::FileDialogWrapper::ShowOpenFileDialog();
    file_path_ = filename;
    status_ = temp_rom_.LoadFromFile(file_path_);
    is_open_ = true;
  }
  HOVER_HINT(".BIN, .HEX");

  if (Button("Copy File Path")) {
    ImGui::SetClipboardText(file_path_.c_str());
  }

  gui::InputHex("BIN Offset", &current_offset_);
  gui::InputHex("BIN Size", &bin_size_);

  if (Button("Decompress BIN")) {
    if (file_path_.empty()) {
      return absl::InvalidArgumentError(
          "Please select a file before decompressing.");
    }
    RETURN_IF_ERROR(DecompressImportData(bin_size_))
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawClipboardImport() {
  gui::TextWithSeparators("Clipboard Import");
  if (Button("Paste From Clipboard")) {
    const char* text = ImGui::GetClipboardText();
    if (text) {
      const auto clipboard_data =
          std::vector<uint8_t>(text, text + strlen(text));
      ImGui::MemFree((void*)text);
      status_ = temp_rom_.LoadFromData(clipboard_data);
      is_open_ = true;
      open_memory_editor_ = true;
    }
  }
  gui::InputHex("Offset", &clipboard_offset_);
  gui::InputHex("Size", &clipboard_size_);
  gui::InputHex("Num Sheets", &num_sheets_to_load_);

  if (Button("Decompress Clipboard Data")) {
    if (temp_rom_.is_loaded()) {
      status_ = DecompressImportData(0x40000);
    } else {
      status_ = absl::InvalidArgumentError(
          "Please paste data into the clipboard before "
          "decompressing.");
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawExperimentalFeatures() {
  gui::TextWithSeparators("Experimental");
  if (Button("Decompress Super Donkey Full")) {
    if (file_path_.empty()) {
      return absl::InvalidArgumentError(
          "Please select `super_donkey_1.bin` before "
          "importing.");
    }
    RETURN_IF_ERROR(DecompressSuperDonkey())
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
    mem_edit.DrawWindow(title.c_str(), temp_rom_.mutable_data(),
                        temp_rom_.size());
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     temp_rom_.data(), current_offset_, size));

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bin_bitmap_.Create(gfx::kTilesheetWidth, 0x2000, gfx::kTilesheetDepth,
                     converted_sheet);

  if (rom()->is_loaded()) {
    auto palette_group = rom()->palette_group().overworld_animated;
    z3_rom_palette_ = palette_group[current_palette_];
    if (col_file_) {
      bin_bitmap_.SetPalette(col_file_palette_);
    } else {
      bin_bitmap_.SetPalette(z3_rom_palette_);
    }
  }

  Renderer::Get().RenderBitmap(&bin_bitmap_);
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
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette

      auto palette_group = rom()->palette_group().get_group(
          kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = *palette_group->mutable_palette(current_palette_index_);
      gfx_sheets_[i].SetPalette(z3_rom_palette_);
    }

    Renderer::Get().RenderBitmap(&gfx_sheets_[i]);
    i++;
  }

  for (const auto& offset : kSuperDonkeySprites) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(
        auto decompressed_data,
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      auto palette_group = rom()->palette_group().get_group(
          kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = *palette_group->mutable_palette(current_palette_index_);
      gfx_sheets_[i].SetPalette(z3_rom_palette_);
    }

    Renderer::Get().RenderBitmap(&gfx_sheets_[i]);
    i++;
  }
  super_donkey_ = true;
  num_sheets_to_load_ = i;

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze

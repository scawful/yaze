// Related header
#include "graphics_editor.h"

// C++ standard library headers
#include <algorithm>
#include <filesystem>
#include <set>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

// Project headers
#include "app/editor/graphics/panels/graphics_editor_panels.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "app/gfx/util/scad_format.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/imgui_memory_editor.h"
#include "app/gui/widgets/asset_browser.h"
#include "app/platform/window.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "util/file_util.h"
#include "util/log.h"

namespace yaze {
namespace editor {

using gfx::kPaletteGroupAddressesKeys;
using ImGui::Button;
using ImGui::InputInt;
using ImGui::InputText;
using ImGui::SameLine;

void GraphicsEditor::Initialize() {
  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  // Initialize panel components
  sheet_browser_panel_ = std::make_unique<SheetBrowserPanel>(&state_);
  pixel_editor_panel_ = std::make_unique<PixelEditorPanel>(&state_, rom_);
  palette_controls_panel_ = std::make_unique<PaletteControlsPanel>(&state_, rom_);
  link_sprite_panel_ = std::make_unique<LinkSpritePanel>(&state_, rom_);
  gfx_group_panel_ = std::make_unique<GfxGroupEditor>();
  gfx_group_panel_->SetRom(rom_);
  gfx_group_panel_->SetGameData(game_data_);
  paletteset_panel_ = std::make_unique<PalettesetEditorPanel>();
  paletteset_panel_->SetRom(rom_);
  paletteset_panel_->SetGameData(game_data_);

  sheet_browser_panel_->Initialize();
  pixel_editor_panel_->Initialize();
  palette_controls_panel_->Initialize();
  link_sprite_panel_->Initialize();

  // Register panels using EditorPanel system with callbacks
  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsSheetBrowserPanel>([this]() {
        if (sheet_browser_panel_) {
          status_ = sheet_browser_panel_->Update();
        }
      }));

  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsPixelEditorPanel>([this]() {
        if (pixel_editor_panel_) {
          status_ = pixel_editor_panel_->Update();
        }
      }));

  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsPaletteControlsPanel>([this]() {
        if (palette_controls_panel_) {
          status_ = palette_controls_panel_->Update();
        }
      }));

  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsLinkSpritePanel>([this]() {
        if (link_sprite_panel_) {
          status_ = link_sprite_panel_->Update();
        }
      }));

  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsGfxGroupPanel>([this]() {
        if (gfx_group_panel_) {
          status_ = gfx_group_panel_->Update();
        }
      }));

  // Paletteset editor panel (separated from GfxGroupEditor for better UX)
  panel_manager->RegisterEditorPanel(
      std::make_unique<GraphicsPalettesetPanel>([this]() {
        if (paletteset_panel_) {
          status_ = paletteset_panel_->Update();
        }
      }));

  // Prototype viewer and polyhedral panels are not registered by default.
}

absl::Status GraphicsEditor::Load() {
  gfx::ScopedTimer timer("GraphicsEditor::Load");

  // Initialize all graphics sheets with appropriate palettes from ROM
  // This ensures textures are created for editing
  if (rom()->is_loaded()) {
    auto& sheets = gfx::Arena::Get().gfx_sheets();

    // Apply default palettes to all sheets based on common SNES ROM structure
    // Sheets 0-112: Use overworld/dungeon palettes
    // Sheets 113-127: Use sprite palettes
    // Sheets 128-222: Use auxiliary/menu palettes

    LOG_INFO("GraphicsEditor", "Initializing textures for %d graphics sheets",
             zelda3::kNumGfxSheets);

    int sheets_queued = 0;
    for (int i = 0; i < zelda3::kNumGfxSheets; i++) {
      if (!sheets[i].is_active() || !sheets[i].surface()) {
        continue;  // Skip inactive or surface-less sheets
      }

      // Palettes are now applied during ROM loading in LoadAllGraphicsData()
      // Just queue texture creation for sheets that don't have textures yet
      if (!sheets[i].texture()) {
        // Fix: Ensure default palettes are applied if missing
        // This handles the case where sheets are loaded but have no palette assigned
        if (sheets[i].palette().empty()) {
          // Default palette assignment logic
          if (i <= 112) {
            // Overworld/Dungeon sheets - use Dungeon Main palette (Group 0, Index 0)
             if (game_data() && game_data()->palette_groups.dungeon_main.size() > 0) {
               sheets[i].SetPaletteWithTransparent(
                   game_data()->palette_groups.dungeon_main.palette(0), 0);
             }
          } else if (i >= 113 && i <= 127) {
            // Sprite sheets - use Sprites Aux1 palette (Group 4, Index 0)
             if (game_data() && game_data()->palette_groups.sprites_aux1.size() > 0) {
               sheets[i].SetPaletteWithTransparent(
                   game_data()->palette_groups.sprites_aux1.palette(0), 0);
             }
          } else {
             // Menu/Aux sheets - use HUD palette if available, or fallback
             if (game_data() && game_data()->palette_groups.hud.size() > 0) {
               sheets[i].SetPaletteWithTransparent(
                   game_data()->palette_groups.hud.palette(0), 0);
             }
          }
        }

        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &sheets[i]);
        sheets_queued++;
      }
    }

    LOG_INFO("GraphicsEditor", "Queued texture creation for %d graphics sheets",
             sheets_queued);
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Only save sheets that have been modified
  if (!state_.HasUnsavedChanges()) {
    LOG_INFO("GraphicsEditor", "No modified sheets to save");
    return absl::OkStatus();
  }

  LOG_INFO("GraphicsEditor", "Saving %zu modified graphics sheets",
           state_.modified_sheets.size());

  auto& sheets = gfx::Arena::Get().gfx_sheets();
  std::set<uint16_t> saved_sheets;
  std::vector<uint16_t> skipped_sheets;

  for (uint16_t sheet_id : state_.modified_sheets) {
    if (sheet_id >= zelda3::kNumGfxSheets) continue;

    auto& sheet = sheets[sheet_id];
    if (!sheet.is_active()) continue;

    // Determine BPP and compression based on sheet range
    int bpp = 3;  // Default 3BPP
    bool compressed = true;

    // Sheets 113-114, 218+ are 2BPP
    if (sheet_id == 113 || sheet_id == 114 || sheet_id >= 218) {
      bpp = 2;
    }

    // Sheets 115-126 are uncompressed
    if (sheet_id >= 115 && sheet_id <= 126) {
      compressed = false;
    }

    if (bpp == 2) {
      const size_t expected_size =
          gfx::kTilesheetWidth * gfx::kTilesheetHeight * 2;
      const size_t actual_size = sheet.vector().size();
      if (actual_size < expected_size) {
        LOG_WARN(
            "GraphicsEditor",
            "Skipping 2BPP sheet %02X save (expected %zu bytes, got %zu)",
            sheet_id, expected_size, actual_size);
        skipped_sheets.push_back(sheet_id);
        continue;
      }
    }

    // Calculate ROM offset for this sheet
    // Get version constants from game_data
    auto version_constants = zelda3::kVersionConstantsMap.at(game_data()->version);
    uint32_t offset = zelda3::GetGraphicsAddress(
        rom_->data(), static_cast<uint8_t>(sheet_id),
        version_constants.kOverworldGfxPtr1,
        version_constants.kOverworldGfxPtr2,
        version_constants.kOverworldGfxPtr3, rom_->size());

    // Convert 8BPP bitmap data to SNES planar format
    auto snes_tile_data = gfx::IndexedToSnesSheet(sheet.vector(), bpp);

    constexpr size_t kDecompressedSheetSize = 0x800;
    std::vector<uint8_t> base_data;
    if (compressed) {
      auto decomp_result = gfx::lc_lz2::DecompressV2(
          rom_->data(), offset, static_cast<int>(kDecompressedSheetSize), 1,
          rom_->size());
      if (!decomp_result.ok()) {
        return decomp_result.status();
      }
      base_data = std::move(*decomp_result);
    } else {
      auto read_result =
          rom_->ReadByteVector(offset, kDecompressedSheetSize);
      if (!read_result.ok()) {
        return read_result.status();
      }
      base_data = std::move(*read_result);
    }

    if (base_data.size() < snes_tile_data.size()) {
      base_data.resize(snes_tile_data.size(), 0);
    }
    std::copy(snes_tile_data.begin(), snes_tile_data.end(),
              base_data.begin());

    std::vector<uint8_t> final_data;
    if (compressed) {
      // Compress using Hyrule Magic LC-LZ2
      int compressed_size = 0;
      auto compressed_data = gfx::HyruleMagicCompress(
          base_data.data(), static_cast<int>(base_data.size()),
          &compressed_size, 1);
      final_data.assign(compressed_data.begin(),
                        compressed_data.begin() + compressed_size);
    } else {
      final_data = std::move(base_data);
    }

    // Write data to ROM buffer
    for (size_t i = 0; i < final_data.size(); i++) {
      rom_->WriteByte(offset + i, final_data[i]);
    }

    LOG_INFO("GraphicsEditor", "Saved sheet %02X (%zu bytes, %s) at offset %06X",
             sheet_id, final_data.size(), compressed ? "compressed" : "raw",
             offset);
    saved_sheets.insert(sheet_id);
  }

  // Clear modified tracking after successful save
  state_.ClearModifiedSheets(saved_sheets);
  if (!skipped_sheets.empty()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Skipped ", skipped_sheets.size(),
                     " 2BPP sheet(s); full data unavailable."));
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::Update() {
  // Panels are now drawn via PanelManager::DrawAllVisiblePanels()
  // This Update() only handles editor-level state and keyboard shortcuts

  // Handle editor-level keyboard shortcuts
  HandleEditorShortcuts();

  CLEAR_AND_RETURN_STATUS(status_)
  return absl::OkStatus();
}

void GraphicsEditor::HandleEditorShortcuts() {
  // Skip if ImGui wants keyboard input
  if (ImGui::GetIO().WantTextInput) {
    return;
  }

  // Tool shortcuts (only when graphics editor is active)
  if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
    state_.SetTool(PixelTool::kSelect);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
    state_.SetTool(PixelTool::kPencil);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
    state_.SetTool(PixelTool::kEraser);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kFill);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
    state_.SetTool(PixelTool::kEyedropper);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_L, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kLine);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !ImGui::GetIO().KeyCtrl) {
    state_.SetTool(PixelTool::kRectangle);
  }

  // Zoom shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)) {
    state_.ZoomIn();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)) {
    state_.ZoomOut();
  }

  // Grid toggle (Ctrl+G)
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
    state_.show_grid = !state_.show_grid;
  }

  // Sheet navigation
  if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
    NextSheet();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
    PrevSheet();
  }
}

void GraphicsEditor::DrawPrototypeViewer() {
  if (open_memory_editor_) {
    ImGui::Begin("Memory Editor", &open_memory_editor_);
    status_ = DrawMemoryEditor();
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

  NEXT_COLUMN() {
    status_ = DrawPaletteControls();
  }

  NEXT_COLUMN()
  gui::BitmapCanvasPipeline(scr_canvas_, scr_bitmap_, 0x200, 0x200, 0x20,
                            scr_loaded_, false, 0);
  status_ = DrawScrImport();

  NEXT_COLUMN()
  if (super_donkey_) {
    // Super Donkey prototype graphics
    for (size_t i = 0; i < num_sheets_to_load_ && i < gfx_sheets_.size(); i++) {
      if (gfx_sheets_[i].is_active() && gfx_sheets_[i].texture()) {
        ImGui::Image((ImTextureID)(intptr_t)gfx_sheets_[i].texture(),
                     ImVec2(128, 32));
        if ((i + 1) % 4 != 0) {
          ImGui::SameLine();
        }
      }
    }
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
}

// =============================================================================
// Prototype Viewer Import Methods
// =============================================================================

absl::Status GraphicsEditor::DrawCgxImport() {
  gui::TextWithSeparators("Cgx Import");
  InputInt("BPP", &current_bpp_);

  InputText("##CGXFile", &cgx_file_name_);
  SameLine();

  if (ImGui::Button("Open CGX")) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
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
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &cgx_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawScrImport() {
  InputText("##ScrFile", &scr_file_name_);

  if (ImGui::Button("Open SCR")) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
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
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &scr_bitmap_);
    }
  }

  return absl::OkStatus();
}

absl::Status GraphicsEditor::DrawPaletteControls() {
  gui::TextWithSeparators("COL Import");
  InputText("##ColFile", &col_file_name_);
  SameLine();

  if (ImGui::Button("Open COL")) {
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    col_file_name_ = filename;
    col_file_path_ = std::filesystem::absolute(filename).string();
    status_ = temp_rom_.LoadFromFile(col_file_path_);
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
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
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
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
    tilemap_file_path_ = std::filesystem::absolute(filename).string();
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);
    status_ = tilemap_rom_.LoadFromFile(tilemap_file_path_);

    // Extract the high and low bytes from the file.
    auto decomp_sheet = gfx::lc_lz2::DecompressV2(tilemap_rom_.data(), 0, 0x800,
                                                  gfx::lc_lz2::kNintendoMode1,
                                                  tilemap_rom_.size());
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
    auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
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
    static yaze::gui::MemoryEditorWidget mem_edit;
    mem_edit.DrawWindow(title.c_str(), temp_rom_.mutable_data(),
                        temp_rom_.size());
  }
  return absl::OkStatus();
}

absl::Status GraphicsEditor::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     temp_rom_.data(), current_offset_, size, 1,
                                     temp_rom_.size()));

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bin_bitmap_.Create(gfx::kTilesheetWidth, 0x2000, gfx::kTilesheetDepth,
                     converted_sheet);

  if (rom()->is_loaded() && game_data()) {
    auto palette_group = game_data()->palette_groups.overworld_animated;
    z3_rom_palette_ = palette_group[current_palette_];
    if (col_file_) {
      bin_bitmap_.SetPalette(col_file_palette_);
    } else {
      bin_bitmap_.SetPalette(z3_rom_palette_);
    }
  }

  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &bin_bitmap_);
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
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000, 1,
                                   temp_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      if (!game_data()) {
        return absl::FailedPreconditionError("GameData not available");
      }
      auto palette_group = game_data()->palette_groups.get_group(
          kPaletteGroupAddressesKeys[current_palette_]);
      z3_rom_palette_ = palette_group->palette(current_palette_index_);
      gfx_sheets_[i].SetPalette(z3_rom_palette_);
    }

    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &gfx_sheets_[i]);
    i++;
  }

  for (const auto& offset : kSuperDonkeySprites) {
    int offset_value =
        std::stoi(offset, nullptr, 16);  // convert hex string to int
    ASSIGN_OR_RETURN(
        auto decompressed_data,
        gfx::lc_lz2::DecompressV2(temp_rom_.data(), offset_value, 0x1000, 1,
                                   temp_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    if (col_file_) {
      gfx_sheets_[i].SetPalette(
          col_file_palette_group_[current_palette_index_]);
    } else {
      // ROM palette
      if (game_data()) {
        auto palette_group = game_data()->palette_groups.get_group(
            kPaletteGroupAddressesKeys[current_palette_]);
        z3_rom_palette_ = palette_group->palette(current_palette_index_);
        gfx_sheets_[i].SetPalette(z3_rom_palette_);
      }
    }

    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &gfx_sheets_[i]);
    i++;
  }
  super_donkey_ = true;
  num_sheets_to_load_ = i;

  return absl::OkStatus();
}

void GraphicsEditor::NextSheet() {
  if (state_.current_sheet_id + 1 < zelda3::kNumGfxSheets) {
    state_.current_sheet_id++;
  }
}

void GraphicsEditor::PrevSheet() {
  if (state_.current_sheet_id > 0) {
    state_.current_sheet_id--;
  }
}

}  // namespace editor
}  // namespace yaze

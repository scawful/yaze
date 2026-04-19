#include "app/editor/graphics/ui/research/prototype_research_view.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <string>

#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/compression.h"
#include "app/gfx/util/scad_format.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/imgui_memory_editor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

namespace {

constexpr uint64_t kDefaultClipboardDecompressSize = 0x40000;

const std::string kSuperDonkeyTiles[] = {
    "97C05", "98219", "9871E", "98C00", "99084", "995AF", "99DE0", "9A27E",
    "9A741", "9AC31", "9B07E", "9B55C", "9B963", "9BB99", "9C009", "9C4B4",
    "9C92B", "9CDD6", "9D2C2", "9E037", "9E527", "9EA56", "9EF65", "9FCD1",
    "A0193", "A059E", "A0B17", "A0FB6", "A14A5", "A1988", "A1E66", "A232B",
    "A27F0", "A2B6E", "A302C", "A3453", "A38CA", "A42BB", "A470C", "A4BA9",
    "A5089", "A5385", "A5742", "A5BCC", "A6017", "A6361", "A66F8"};

const std::string kSuperDonkeySprites[] = {
    "A8E5D", "A9435", "A9934", "A9D83", "AA2F1", "AA6D4", "AABE4", "AB127",
    "AB65A", "ABBDD", "AC38D", "AC797", "ACCC8", "AD0AE", "AD245", "AD554",
    "ADAAC", "ADECC", "AE453", "AE9D2", "AEF40", "AF3C9", "AF92E", "AFE9D",
    "B03D2", "B09AC", "B0F0C", "B1430", "B1859", "B1E01", "B229A", "B2854",
    "B2D27", "B31D7", "B3B58", "B40B5", "B45A5", "B4D64", "B5031", "B555F",
    "B5F30", "B6858", "B70DD", "B7526", "B79EC", "B7C83", "B80F7", "B85CC",
    "B8A3F", "B8F97", "B94F2", "B9A20", "B9E9A", "BA3A2", "BA8F6", "BACDC",
    "BB1F9", "BB781", "BBCCA", "BC26D", "BC7D4", "BCBB0", "BD082", "BD5FC",
    "BE115", "BE5C2", "BEB63", "BF0CB", "BF607", "BFA55", "BFD71", "C017D",
    "C0567", "C0981", "C0BA7", "C116D", "C166A", "C1FE0", "C24CE", "C2B19"};

std::string NormalizeSelectedPath(const std::string& path) {
  if (path.empty()) {
    return path;
  }
  return std::filesystem::absolute(path).string();
}

void QueueTextureRefresh(gfx::Bitmap* bitmap) {
  if (!bitmap || !bitmap->is_active() || !bitmap->surface()) {
    return;
  }

  gfx::Arena::Get().QueueTextureCommand(
      bitmap->texture() ? gfx::Arena::TextureCommandType::UPDATE
                        : gfx::Arena::TextureCommandType::CREATE,
      bitmap);
}

}  // namespace

void PrototypeResearchView::Initialize() {
  prototype_sheet_scale_ = 2.0f;
  prototype_sheet_columns_ = 4;
  active_graphics_preview_ = 0;
}

void PrototypeResearchView::Draw(bool* p_open) {
  Update().IgnoreError();
}

absl::Status PrototypeResearchView::Update() {
  DrawSummaryBar();
  DrawMemoryEditorWindow();

  constexpr ImGuiTableFlags kWorkspaceFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
      ImGuiTableFlags_NoSavedSettings;

  if (ImGui::BeginTable("##PrototypeWorkspace", 2, kWorkspaceFlags)) {
    ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed,
                            420.0f);
    ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                            0.0f);
    ImGui::TableNextColumn();
    DrawInspectorColumn();
    ImGui::TableNextColumn();
    DrawPreviewColumn();
    ImGui::EndTable();
  }

  return status_;
}

void PrototypeResearchView::DrawSummaryBar() {
  gui::SectionHeader(ICON_MD_TRAVEL_EXPLORE, "Prototype Research",
                     gui::GetInfoColor());
  ImGui::TextWrapped(
      "Decode CGX, SCR, COL, and raw prototype data directly inside the "
      "graphics workspace so research assets can be compared without leaving "
      "the editor.");
  gui::ColoredText(
      "SCR files are SNES tilemaps. Pair them with matching CGX and COL data "
      "to reconstruct early HUDs, menus, and screen layouts.",
      gui::GetInfoColor());
  DrawLoadedAssetBadges();

  if (!status_.ok()) {
    gui::ColoredTextF(gui::GetErrorColor(), "%s", status_.message().data());
  } else if (!HasGraphicsPreview() && !HasScreenPreview() &&
             !HasPrototypeSheetPreview()) {
    gui::ColoredText(
        "Load a CGX, BIN, or Super Donkey source to start previewing.",
        gui::GetDisabledColor());
  }
}

void PrototypeResearchView::DrawLoadedAssetBadges() {
  if (!cgx_loaded_ && !scr_loaded_ && !col_file_ && !gfx_loaded_ &&
      !super_donkey_ && !obj_loaded_ && !tilemap_loaded_) {
    gui::StatusBadge("No assets loaded", gui::ButtonType::Info);
    return;
  }

  bool first = true;
  auto draw_badge = [&](const char* label, gui::ButtonType type) {
    if (!first) {
      ImGui::SameLine();
    }
    gui::StatusBadge(label, type);
    first = false;
  };

  if (cgx_loaded_) {
    draw_badge("CGX", gui::ButtonType::Success);
  }
  if (scr_loaded_) {
    draw_badge("SCR", gui::ButtonType::Success);
  }
  if (col_file_) {
    draw_badge(UsingExternalPalette() ? "COL Palette" : "COL Loaded",
               UsingExternalPalette() ? gui::ButtonType::Info
                                      : gui::ButtonType::Default);
  }
  if (gfx_loaded_) {
    draw_badge("BIN Preview", gui::ButtonType::Success);
  }
  if (super_donkey_) {
    draw_badge("Super Donkey", gui::ButtonType::Warning);
  }
  if (obj_loaded_) {
    draw_badge("OBJ Bytes", gui::ButtonType::Default);
  }
  if (tilemap_loaded_) {
    draw_badge("Tilemap Bytes", gui::ButtonType::Default);
  }
}

void PrototypeResearchView::DrawInspectorColumn() {
  ImGui::BeginChild("##PrototypeInspector", ImVec2(0, 0), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  auto sync_status = [this](const absl::Status& section_status) {
    status_ = section_status;
  };

  if (ImGui::CollapsingHeader(ICON_MD_IMAGE_SEARCH " Graphics Sources",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    sync_status(DrawCgxImportSection());
    sync_status(DrawScrImportSection());
    sync_status(DrawBinImportSection());
  }

  if (ImGui::CollapsingHeader(ICON_MD_PALETTE " Preview Palette",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    sync_status(DrawPaletteSection());
  }

  if (ImGui::CollapsingHeader(ICON_MD_BUILD " Advanced Inputs",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    sync_status(DrawClipboardSection());
    sync_status(DrawObjImportSection());
    sync_status(DrawTilemapImportSection());
    sync_status(DrawExperimentalSection());
  }

  ImGui::EndChild();
}

void PrototypeResearchView::DrawPreviewColumn() {
  ImGui::BeginChild("##PrototypePreview", ImVec2(0, 0), false,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  gui::SectionHeader(ICON_MD_PREVIEW, "Preview", gui::GetAccentColor());

  if (ImGui::BeginTabBar("##PrototypePreviewTabs")) {
    if (ImGui::BeginTabItem("Graphics")) {
      DrawGraphicsPreview();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Screen")) {
      DrawScreenPreview();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Super Donkey")) {
      DrawSuperDonkeyPreview();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::EndChild();
}

void PrototypeResearchView::DrawGraphicsPreview() {
  if (!HasGraphicsPreview()) {
    DrawEmptyState("No graphics preview available",
                   "Load a CGX tilesheet or decode a BIN source.");
    return;
  }

  const bool has_cgx_preview = cgx_loaded_ && cgx_bitmap_.is_active();
  const bool has_bin_preview = gfx_loaded_ && bin_bitmap_.is_active();

  if (has_cgx_preview && has_bin_preview) {
    if (gui::ToggleButton("CGX", active_graphics_preview_ == 0)) {
      active_graphics_preview_ = 0;
    }
    ImGui::SameLine();
    if (gui::ToggleButton("BIN", active_graphics_preview_ == 1)) {
      active_graphics_preview_ = 1;
    }
    ImGui::SameLine();
    gui::HelpMarker(
        "Switch between the direct CGX decode and the generic BIN preview.");
  } else {
    active_graphics_preview_ = has_bin_preview ? 1 : 0;
  }

  gfx::Bitmap* active_bitmap =
      active_graphics_preview_ == 1 ? &bin_bitmap_ : &cgx_bitmap_;
  const char* preview_label =
      active_graphics_preview_ == 1 ? "BIN Decode" : "CGX Tilesheet";

  gui::ColoredTextF(gui::GetInfoColor(), "%s  %dx%d", preview_label,
                    active_bitmap->width(), active_bitmap->height());
  gui::BitmapCanvasPipeline(import_canvas_, *active_bitmap,
                            active_bitmap->width(), active_bitmap->height(),
                            0x20, true, true, active_graphics_preview_ + 1);
}

void PrototypeResearchView::DrawScreenPreview() {
  if (!HasScreenPreview()) {
    DrawEmptyState(
        "No screen preview available",
        "Load SCR data after CGX to reconstruct a prototype screen.");
    return;
  }

  gui::ColoredTextF(gui::GetInfoColor(), "SCR Composite  %dx%d",
                    scr_bitmap_.width(), scr_bitmap_.height());
  gui::BitmapCanvasPipeline(scr_canvas_, scr_bitmap_, scr_bitmap_.width(),
                            scr_bitmap_.height(), 0x20, true, true, 99);
}

void PrototypeResearchView::DrawSuperDonkeyPreview() {
  if (!HasPrototypeSheetPreview()) {
    DrawEmptyState("Super Donkey sheets not decoded",
                   "Use the experimental decoder after loading the prototype "
                   "BIN source.");
    return;
  }

  ImGui::SetNextItemWidth(120.0f);
  gui::SliderFloatWheel("Scale", &prototype_sheet_scale_, 1.0f, 4.0f, "%.1fx");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::SliderInt("Columns", &prototype_sheet_columns_, 1, 6);

  const float tile_width = 128.0f * prototype_sheet_scale_;
  const float tile_height = 32.0f * prototype_sheet_scale_;
  int current_column = 0;

  ImGui::BeginChild("##SuperDonkeySheets", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (size_t i = 0; i < num_sheets_to_load_ && i < gfx_sheets_.size(); ++i) {
    auto& bitmap = gfx_sheets_[i];
    if (!bitmap.is_active() || !bitmap.texture()) {
      continue;
    }

    ImGui::BeginGroup();
    ImGui::Image((ImTextureID)(intptr_t)bitmap.texture(),
                 ImVec2(tile_width, tile_height));
    ImGui::Text("Sheet %02zu", i);
    ImGui::EndGroup();

    current_column++;
    if (current_column < prototype_sheet_columns_) {
      ImGui::SameLine();
    } else {
      current_column = 0;
    }
  }
  ImGui::EndChild();
}

void PrototypeResearchView::DrawEmptyState(const char* title,
                                           const char* detail) {
  gui::ColoredText(title, gui::GetDisabledColor());
  ImGui::Spacing();
  ImGui::TextWrapped("%s", detail);
}

void PrototypeResearchView::DrawMemoryEditorWindow() {
  if (!open_memory_editor_ || !source_rom_.is_loaded()) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(900.0f, 540.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(720.0f, 420.0f),
                                      ImVec2(1600.0f, FLT_MAX));
  if (!ImGui::Begin(ICON_MD_MEMORY " Prototype Memory Inspector",
                    &open_memory_editor_, ImGuiWindowFlags_NoScrollbar)) {
    ImGui::End();
    return;
  }

  static gui::MemoryEditorWidget memory_editor;
  memory_editor.DrawContents(source_rom_.mutable_data(), source_rom_.size());
  ImGui::End();
}

bool PrototypeResearchView::DrawPathEditor(const char* id, const char* hint,
                                           std::string* path,
                                           const char* browse_label,
                                           const char* browse_spec) {
  bool changed = false;
  util::FileDialogOptions options;
  options.filters.push_back({browse_label, browse_spec});
  options.filters.push_back({"All Files", "*"});

  ImGui::PushID(id);
  ImGui::SetNextItemWidth(-68.0f);
  changed = ImGui::InputTextWithHint("##Path", hint, path);
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FOLDER_OPEN "##BrowsePath")) {
    const auto selected = util::FileDialogWrapper::ShowOpenFileDialog(options);
    if (!selected.empty()) {
      *path = NormalizeSelectedPath(selected);
      changed = true;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Browse");
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(path->empty());
  if (ImGui::Button(ICON_MD_CONTENT_COPY "##CopyPath")) {
    ImGui::SetClipboardText(path->c_str());
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Copy path");
  }
  ImGui::EndDisabled();
  ImGui::PopID();
  return changed;
}

absl::Status PrototypeResearchView::DrawCgxImportSection() {
  ImGui::SeparatorText("CGX Tilesheet");
  DrawPathEditor("cgx", "Select a .cgx or prototype graphics blob", &cgx_path_,
                 "CGX Files", "cgx,bin");

  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::InputInt("Bits Per Pixel", &current_bpp_)) {
    current_bpp_ = std::clamp(current_bpp_, 1, 8);
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(cgx_path_.empty());
  if (gui::ColoredButton(ICON_MD_DOWNLOAD " Load CGX", gui::ButtonType::Info)) {
    status_ = LoadCgxData();
    return status_;
  }
  ImGui::EndDisabled();

  return status_;
}

absl::Status PrototypeResearchView::DrawScrImportSection() {
  ImGui::SeparatorText("SCR Screen");
  DrawPathEditor("scr", "Select a .scr or prototype screen blob", &scr_path_,
                 "SCR Files", "scr,pnl,bin,bak");

  ImGui::SetNextItemWidth(120.0f);
  ImGui::InputInt("SCR Mod", &scr_mod_value_);
  ImGui::SameLine();
  ImGui::BeginDisabled(scr_path_.empty() || decoded_cgx_.empty());
  if (gui::ColoredButton(ICON_MD_SCREEN_SEARCH_DESKTOP " Load SCR",
                         gui::ButtonType::Info)) {
    status_ = LoadScrData();
    return status_;
  }
  ImGui::EndDisabled();
  if (decoded_cgx_.empty()) {
    gui::HelpMarker(
        "SCR preview depends on an already decoded CGX tilesheet. Use this "
        "to reconstruct prototype HUDs and menus from raw SNES tilemaps.");
  }

  return status_;
}

absl::Status PrototypeResearchView::DrawPaletteSection() {
  ImGui::SeparatorText("Palette Source");

  DrawPathEditor("col", "Select a .col or backup palette", &col_path_,
                 "COL Files", "col,bak,bin");

  ImGui::BeginDisabled(col_path_.empty());
  if (gui::ColoredButton(ICON_MD_PALETTE " Load COL", gui::ButtonType::Info)) {
    status_ = LoadColData();
    return status_;
  }
  ImGui::EndDisabled();

  if (col_file_) {
    if (ImGui::Checkbox("Prefer external COL palette",
                        &use_external_palette_)) {
      RefreshPreviewPalettes();
    }
  } else {
    bool disabled_toggle = false;
    ImGui::BeginDisabled();
    ImGui::Checkbox("Prefer external COL palette", &disabled_toggle);
    ImGui::EndDisabled();
  }

  if (UsingExternalPalette()) {
    bool refresh_palette = false;
    gui::ColoredText("Using COL palette rows for preview",
                     gui::GetSuccessColor());
    gui::SelectablePalettePipeline(external_palette_index_, refresh_palette,
                                   col_file_palette_);
    if (refresh_palette) {
      RefreshPreviewPalettes();
    }
  } else if (rom_ && rom_->is_loaded() && game_data_) {
    using gfx::kPaletteGroupAddressesKeys;

    ImGui::SetNextItemWidth(220.0f);
    bool palette_changed = ImGui::Combo(
        "ROM Group", &rom_palette_group_index_, kPaletteGroupAddressesKeys,
        IM_ARRAYSIZE(kPaletteGroupAddressesKeys));

    int palette_index = static_cast<int>(rom_palette_index_);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("ROM Palette", &palette_index)) {
      rom_palette_index_ = static_cast<uint64_t>(std::max(0, palette_index));
      palette_changed = true;
    }

    if (palette_changed) {
      RefreshPreviewPalettes();
    }
  } else {
    gui::ColoredText(
        "Load a ROM or a COL file to apply palette data to prototype previews.",
        gui::GetDisabledColor());
  }

  return status_;
}

absl::Status PrototypeResearchView::DrawBinImportSection() {
  ImGui::SeparatorText("Raw BIN / ROM Decompression");
  DrawPathEditor("bin", "Select a .bin, .hex, or ROM source", &bin_path_,
                 "Binary Sources", "bin,hex,sfc,smc");

  gui::InputHex("BIN Offset", &current_offset_);
  gui::InputHex("BIN Size", &bin_size_);

  ImGui::BeginDisabled(bin_path_.empty());
  if (gui::ColoredButton(ICON_MD_IMAGE_SEARCH " Decode BIN",
                         gui::ButtonType::Info)) {
    status_ = LoadBinPreview();
    return status_;
  }
  ImGui::EndDisabled();

  return status_;
}

absl::Status PrototypeResearchView::DrawClipboardSection() {
  ImGui::SeparatorText("Clipboard Source");

  if (ImGui::Button(ICON_MD_CONTENT_PASTE " Paste Raw Bytes")) {
    status_ = ImportClipboard();
    return status_;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!source_rom_.is_loaded());
  if (ImGui::Button(ICON_MD_MEMORY " Inspect")) {
    open_memory_editor_ = true;
  }
  ImGui::EndDisabled();

  gui::InputHex("Clipboard Offset", &clipboard_offset_);
  gui::InputHex("Clipboard Size", &clipboard_size_);

  ImGui::BeginDisabled(!source_rom_.is_loaded());
  if (gui::ColoredButton(ICON_MD_PREVIEW " Decode Clipboard",
                         gui::ButtonType::Info)) {
    current_offset_ = clipboard_offset_;
    const int decode_size =
        static_cast<int>(clipboard_size_ == 0 ? kDefaultClipboardDecompressSize
                                              : clipboard_size_);
    status_ = DecompressImportData(decode_size);
    return status_;
  }
  ImGui::EndDisabled();

  return status_;
}

absl::Status PrototypeResearchView::DrawObjImportSection() {
  ImGui::SeparatorText("OBJ Source");
  DrawPathEditor("obj", "Select an .obj or backup file", &obj_path_,
                 "OBJ Files", "obj,bak");

  ImGui::BeginDisabled(obj_path_.empty());
  if (ImGui::Button(ICON_MD_DOWNLOAD " Load OBJ")) {
    status_ = LoadObjData();
    return status_;
  }
  ImGui::EndDisabled();

  if (obj_loaded_) {
    gui::ColoredText("OBJ bytes loaded. Preview integration is not wired yet.",
                     gui::GetInfoColor());
  }

  return status_;
}

absl::Status PrototypeResearchView::DrawTilemapImportSection() {
  ImGui::SeparatorText("Tilemap Source");
  DrawPathEditor("tilemap", "Select a tilemap dump", &tilemap_path_,
                 "Tilemap Files", "dat,bin,hex");

  ImGui::BeginDisabled(tilemap_path_.empty());
  if (ImGui::Button(ICON_MD_DOWNLOAD " Load Tilemap")) {
    status_ = LoadTilemapData();
    return status_;
  }
  ImGui::EndDisabled();

  if (tilemap_loaded_) {
    gui::ColoredText(
        "Tilemap bytes loaded for research. Structured preview is pending.",
        gui::GetInfoColor());
  }

  return status_;
}

absl::Status PrototypeResearchView::DrawExperimentalSection() {
  ImGui::SeparatorText("Experimental");
  ImGui::BeginDisabled(bin_path_.empty());
  if (gui::ColoredButton(ICON_MD_BUILD " Decode Super Donkey Pack",
                         gui::ButtonType::Warning)) {
    status_ = source_rom_.LoadFromFile(bin_path_);
    if (!status_.ok()) {
      return status_;
    }
    status_ = DecompressSuperDonkey();
    return status_;
  }
  ImGui::EndDisabled();
  gui::HelpMarker(
      "Run the prototype sheet decoder against the currently selected BIN "
      "source.");

  return status_;
}

absl::Status PrototypeResearchView::LoadCgxData() {
  RETURN_IF_ERROR(gfx::LoadCgx(current_bpp_, cgx_path_, cgx_data_, decoded_cgx_,
                               extra_cgx_data_));

  cgx_bitmap_.Create(0x80, 0x200, 8, decoded_cgx_);
  ApplyPreviewPalette(cgx_bitmap_);
  cgx_loaded_ = true;
  active_graphics_preview_ = 0;
  if (scr_loaded_ && !scr_data_.empty()) {
    RETURN_IF_ERROR(RebuildScreenPreview());
  }
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::LoadScrData() {
  RETURN_IF_ERROR(gfx::LoadScr(scr_path_, scr_mod_value_, scr_data_));
  return RebuildScreenPreview();
}

absl::Status PrototypeResearchView::RebuildScreenPreview() {
  if (scr_data_.empty()) {
    return absl::FailedPreconditionError("SCR data is not loaded.");
  }
  if (decoded_cgx_.empty()) {
    return absl::FailedPreconditionError(
        "Decode a CGX tilesheet before reconstructing the SCR preview.");
  }

  decoded_scr_data_.assign(0x100 * 0x100, 0);
  RETURN_IF_ERROR(gfx::DrawScrWithCgx(current_bpp_, scr_data_,
                                      decoded_scr_data_, decoded_cgx_));

  scr_bitmap_.Create(0x100, 0x100, 8, decoded_scr_data_);
  ApplyPreviewPalette(scr_bitmap_);
  scr_loaded_ = true;
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::LoadColData() {
  RETURN_IF_ERROR(palette_rom_.LoadFromFile(col_path_));

  auto col_data = gfx::GetColFileData(palette_rom_.mutable_data());
  if (col_file_palette_group_.size() != 0) {
    col_file_palette_group_.clear();
  }

  ASSIGN_OR_RETURN(col_file_palette_group_,
                   gfx::CreatePaletteGroupFromColFile(col_data));
  col_file_palette_ = gfx::SnesPalette(col_data);
  decoded_col_ = gfx::DecodeColFile(col_path_);
  col_file_ = true;
  use_external_palette_ = true;
  external_palette_index_ = 0;
  RefreshPreviewPalettes();
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::LoadBinPreview() {
  RETURN_IF_ERROR(source_rom_.LoadFromFile(bin_path_));
  return DecompressImportData(static_cast<int>(bin_size_));
}

absl::Status PrototypeResearchView::LoadObjData() {
  RETURN_IF_ERROR(source_rom_.LoadFromFile(obj_path_));
  obj_loaded_ = true;
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::LoadTilemapData() {
  RETURN_IF_ERROR(tilemap_rom_.LoadFromFile(tilemap_path_));
  auto decomp_sheet = gfx::lc_lz2::DecompressV2(tilemap_rom_.data(), 0, 0x800,
                                                gfx::lc_lz2::kNintendoMode1,
                                                tilemap_rom_.size());
  if (!decomp_sheet.ok()) {
    return decomp_sheet.status();
  }
  tilemap_loaded_ = true;
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::ImportClipboard() {
  const char* text = ImGui::GetClipboardText();
  if (!text || *text == '\0') {
    return absl::InvalidArgumentError("Clipboard is empty.");
  }

  const auto clipboard_data = std::vector<uint8_t>(text, text + strlen(text));
  RETURN_IF_ERROR(source_rom_.LoadFromData(clipboard_data));
  open_memory_editor_ = true;
  return absl::OkStatus();
}

absl::Status PrototypeResearchView::DecompressImportData(int size) {
  ASSIGN_OR_RETURN(import_data_, gfx::lc_lz2::DecompressV2(
                                     source_rom_.data(), current_offset_, size,
                                     1, source_rom_.size()));

  auto converted_sheet = gfx::SnesTo8bppSheet(import_data_, 3);
  bin_bitmap_.Create(gfx::kTilesheetWidth, 0x2000, gfx::kTilesheetDepth,
                     converted_sheet);
  ApplyPreviewPalette(bin_bitmap_);
  gfx_loaded_ = true;
  active_graphics_preview_ = 1;

  return absl::OkStatus();
}

absl::Status PrototypeResearchView::DecompressSuperDonkey() {
  int i = 0;
  for (const auto& offset : kSuperDonkeyTiles) {
    const int offset_value = std::stoi(offset, nullptr, 16);
    ASSIGN_OR_RETURN(auto decompressed_data,
                     gfx::lc_lz2::DecompressV2(source_rom_.data(), offset_value,
                                               0x1000, 1, source_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    ApplyPreviewPalette(gfx_sheets_[i]);
    ++i;
  }

  for (const auto& offset : kSuperDonkeySprites) {
    const int offset_value = std::stoi(offset, nullptr, 16);
    ASSIGN_OR_RETURN(auto decompressed_data,
                     gfx::lc_lz2::DecompressV2(source_rom_.data(), offset_value,
                                               0x1000, 1, source_rom_.size()));
    auto converted_sheet = gfx::SnesTo8bppSheet(decompressed_data, 3);
    gfx_sheets_[i] = gfx::Bitmap(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                                 gfx::kTilesheetDepth, converted_sheet);
    ApplyPreviewPalette(gfx_sheets_[i]);
    ++i;
  }

  super_donkey_ = true;
  num_sheets_to_load_ = i;
  return absl::OkStatus();
}

void PrototypeResearchView::RefreshPreviewPalettes() {
  if (cgx_loaded_ && cgx_bitmap_.is_active()) {
    ApplyPreviewPalette(cgx_bitmap_);
  }
  if (scr_loaded_ && scr_bitmap_.is_active()) {
    ApplyPreviewPalette(scr_bitmap_);
  }
  if (gfx_loaded_ && bin_bitmap_.is_active()) {
    ApplyPreviewPalette(bin_bitmap_);
  }
  if (super_donkey_) {
    for (size_t i = 0; i < num_sheets_to_load_ && i < gfx_sheets_.size(); ++i) {
      if (gfx_sheets_[i].is_active()) {
        ApplyPreviewPalette(gfx_sheets_[i]);
      }
    }
  }
}

void PrototypeResearchView::ApplyPreviewPalette(gfx::Bitmap& bitmap) {
  if (UsingExternalPalette()) {
    if (col_file_palette_group_.size() > 0 &&
        external_palette_index_ < col_file_palette_group_.size()) {
      bitmap.SetPalette(col_file_palette_group_[external_palette_index_]);
    } else if (col_file_palette_.size() > 0) {
      bitmap.SetPalette(col_file_palette_);
    } else if (!decoded_col_.empty()) {
      bitmap.SetPalette(decoded_col_);
    }
  } else if (rom_ && rom_->is_loaded() && game_data_) {
    using gfx::kPaletteGroupAddressesKeys;
    auto palette_group = game_data_->palette_groups.get_group(
        kPaletteGroupAddressesKeys[rom_palette_group_index_]);
    if (palette_group && rom_palette_index_ < palette_group->size()) {
      bitmap.SetPalette(palette_group->palette(rom_palette_index_));
    }
  } else if (col_file_palette_.size() > 0) {
    bitmap.SetPalette(col_file_palette_);
  }

  QueueTextureRefresh(&bitmap);
}

bool PrototypeResearchView::UsingExternalPalette() const {
  return use_external_palette_ && col_file_;
}

bool PrototypeResearchView::HasGraphicsPreview() const {
  return (cgx_loaded_ && cgx_bitmap_.is_active()) ||
         (gfx_loaded_ && bin_bitmap_.is_active());
}

bool PrototypeResearchView::HasScreenPreview() const {
  return scr_loaded_ && scr_bitmap_.is_active();
}

bool PrototypeResearchView::HasPrototypeSheetPreview() const {
  return super_donkey_ && num_sheets_to_load_ > 0;
}

}  // namespace editor
}  // namespace yaze

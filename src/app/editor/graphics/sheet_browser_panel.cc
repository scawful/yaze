#include "app/editor/graphics/sheet_browser_panel.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/graphics_sheet_labels.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/indexed_png.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_png.h"

namespace yaze {
namespace editor {

void SheetBrowserPanel::Initialize() {
  // Initialize with sensible defaults
  thumbnail_scale_ = 2.0f;
  columns_ = 2;
}

void SheetBrowserPanel::Draw(bool* p_open) {
  // WindowContent interface - delegate to existing Update() logic
  (void)Update();
}

absl::Status SheetBrowserPanel::Update() {
  if (rom_ != inventory_rom_ ||
      (!inventory_.has_value() && inventory_error_.empty())) {
    RefreshInventory();
  }
  DrawSearchBar();
  ImGui::Separator();
  DrawBatchOperations();
  ImGui::Separator();
  DrawPendingSave();
  DrawSelectedSheetInfo();
  DrawPngTransfer(state_->current_sheet_id);
  DrawSheetGrid();
  return absl::OkStatus();
}

void SheetBrowserPanel::SetDataSources(
    Rom* rom, zelda3::GameData* game_data,
    std::function<const project::YazeProject*()> project_getter) {
  rom_ = rom;
  game_data_ = game_data;
  project_getter_ = std::move(project_getter);
}

void SheetBrowserPanel::RefreshInventory() {
  inventory_.reset();
  inventory_error_.clear();
  inventory_rom_ = rom_;
  if (rom_ == nullptr || !rom_->is_loaded()) {
    inventory_error_ = "No ROM loaded";
    return;
  }
  const project::YazeProject* project =
      project_getter_ ? project_getter_() : nullptr;
  const auto options = core::BuildGfxSheetInventoryOptions(project);
  const auto areas = zelda3::CollectOverworldAreaGfx(*rom_, game_data_);
  const auto rooms = zelda3::CollectRoomGfx(*rom_);
  auto inventory = zelda3::BuildGfxSheetInventory(*rom_, areas, rooms, options);
  if (!inventory.ok()) {
    inventory_error_ = std::string(inventory.status().message());
    return;
  }
  inventory_ = std::move(*inventory);
}

namespace {

std::string HexList(const std::vector<int>& values) {
  if (values.empty()) {
    return "-";
  }
  return absl::StrJoin(values, " ", [](std::string* out, int value) {
    out->append(absl::StrFormat("%02X", value));
  });
}

void DrawUsageRow(const char* label, const std::vector<int>& values) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextUnformatted(label);
  ImGui::TableSetColumnIndex(1);
  ImGui::Text("%zu", values.size());
  ImGui::TableSetColumnIndex(2);
  ImGui::TextWrapped("%s", HexList(values).c_str());
}

}  // namespace

std::string SheetBrowserPanel::SheetLabel(uint16_t sheet) const {
  const project::YazeProject* project =
      project_getter_ ? project_getter_() : nullptr;
  return project != nullptr ? core::GetGraphicsSheetLabel(*project, sheet)
                            : std::string();
}

void SheetBrowserPanel::DrawSheetLabelEditor(uint16_t sheet_id) {
  if (label_edit_sheet_ != sheet_id) {
    label_edit_sheet_ = sheet_id;
    label_buffer_ = SheetLabel(sheet_id);
  }
  const bool can_edit = static_cast<bool>(label_setter_);
  ImGui::BeginDisabled(!can_edit);
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth() * 2.0f);
  const bool submitted = ImGui::InputTextWithHint(
      "##SheetLabel", tr("Sheet label (stored in the project)"), &label_buffer_,
      ImGuiInputTextFlags_EnterReturnsTrue);
  if ((submitted || ImGui::IsItemDeactivatedAfterEdit()) && can_edit) {
    const absl::Status status = label_setter_(sheet_id, label_buffer_);
    label_status_ = status.ok() ? "Label set; save the project to keep it"
                                : std::string(status.message());
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_UPLOAD_FILE " Import Spritesets CSV") &&
      label_importer_) {
    const std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      std::ifstream file(path);
      std::stringstream text;
      text << file.rdbuf();
      auto imported = label_importer_(text.str());
      label_status_ = imported.ok()
                          ? absl::StrFormat(
                                "Imported %d sheet label(s); existing labels "
                                "were kept",
                                *imported)
                          : std::string(imported.status().message());
      label_edit_sheet_ = -1;  // reload the field
    }
  }
  HOVER_HINT(
      "Label sprite sheets from Oracle's \"Spritesets\" CSV: a cell like "
      "\"0x46 Cannon Soldiers\" names sheet 0x46 + 0x73.");
  ImGui::EndDisabled();
  if (!can_edit) {
    ImGui::TextDisabled(
        tr("Open the project that owns this ROM to edit "
           "sheet labels."));
  }
  if (!label_status_.empty()) {
    ImGui::TextDisabled("%s", label_status_.c_str());
  }
}

void SheetBrowserPanel::DrawSelectedSheetInfo() {
  const uint16_t sheet_id = state_->current_sheet_id;
  if (!ImGui::CollapsingHeader(
          absl::StrFormat("Sheet 0x%02X usage and free space###SheetUsage",
                          sheet_id)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (ImGui::SmallButton(ICON_MD_REFRESH " Refresh")) {
    RefreshInventory();
  }
  HOVER_HINT(
      "Re-read the ROM buffer (saved sheets, group tables, areas, "
      "rooms). Unsaved pixel edits are not included.");
  ImGui::SameLine();
  ImGui::Checkbox(tr("Show free 16x16"), &show_free_blocks_);
  HOVER_HINT("Outline empty, unreserved 16x16 blocks on the thumbnails");

  if (!inventory_.has_value()) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       inventory_error_.empty() ? "Inventory not built"
                                                : inventory_error_.c_str());
    return;
  }
  if (sheet_id >= inventory_->sheets.size()) {
    return;
  }
  const auto& entry = inventory_->sheets[sheet_id];

  DrawSheetLabelEditor(sheet_id);
  ImGui::Text("%s  PC 0x%06X", zelda3::GfxSheetInventoryKindName(entry.kind),
              entry.pc);
  if (entry.stored_bytes.has_value()) {
    ImGui::SameLine();
    ImGui::Text(tr("stored %zu bytes"), *entry.stored_bytes);
  }
  if (entry.sprite_value.has_value()) {
    ImGui::SameLine();
    ImGui::Text(tr("sprite value 0x%02X"), *entry.sprite_value);
  }
  if (!entry.error.empty()) {
    ImGui::TextColored(gui::GetErrorColor(), "%s", entry.error.c_str());
  }
  if (entry.reserved) {
    ImGui::TextColored(gui::GetErrorColor(),
                       ICON_MD_BLOCK " Reserved: never written, not free");
  }
  if (entry.flagged) {
    ImGui::TextColored(gui::GetWarningColor(),
                       ICON_MD_FLAG " Flagged: ask the owner before use");
  }
  if (entry.engine_loaded) {
    ImGui::TextColored(gui::GetInfoColor(),
                       ICON_MD_MEMORY " Engine-loaded (raw or 2bpp)");
  }
  if (!entry.labels.empty()) {
    ImGui::TextWrapped("%s", absl::StrJoin(entry.labels, "; ").c_str());
  }

  const auto free_blocks = entry.FreeBlocks();
  if (entry.has_block_stats) {
    ImGui::Text(tr("Free 16x16 blocks: %zu of 16  (empty 8x8 tiles: %d)"),
                free_blocks.size(), entry.empty_8x8);
    if (!free_blocks.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("[%s]", HexList(free_blocks).c_str());
    }
    if (!entry.reserved_blocks.empty()) {
      ImGui::Text(tr("Reserved blocks: %s"),
                  absl::StrJoin(entry.reserved_blocks, " ").c_str());
    }
  }

  if (ImGui::BeginTable("##SheetUsedBy", 3,
                        ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupColumn(tr("Used by"), ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn(tr("Ids (hex)"));
    ImGui::TableHeadersRow();
    const auto& use = entry.usage;
    if (entry.sprite_value.has_value()) {
      DrawUsageRow(tr("Spritesets"), use.spritesets);
      DrawUsageRow(tr("OW areas (sprites)"), use.ow_areas_sprite);
      DrawUsageRow(tr("Rooms (sprites)"), use.rooms_sprite);
    }
    DrawUsageRow(tr("Main blocksets"), use.main_blocksets);
    DrawUsageRow(tr("Room blocksets"), use.room_blocksets);
    DrawUsageRow(tr("OW areas (static)"), use.ow_areas_static);
    ImGui::EndTable();
  }
  if (entry.unreferenced) {
    ImGui::TextDisabled(tr("No table references this sheet."));
  }
  ImGui::Separator();
}

std::string SheetBrowserPanel::SheetUsageSummary(uint16_t sheet_id) const {
  if (!inventory_.has_value() || sheet_id >= inventory_->sheets.size()) {
    return {};
  }
  const auto& use = inventory_->sheets[sheet_id].usage;
  std::vector<std::string> parts;
  auto add = [&parts](size_t count, const char* what) {
    if (count > 0) {
      parts.push_back(absl::StrFormat("%zu %s", count, what));
    }
  };
  add(use.main_blocksets.size(), "main blocksets");
  add(use.room_blocksets.size(), "room blocksets");
  add(use.spritesets.size(), "spritesets");
  add(use.ow_areas_static.size() + use.ow_areas_sprite.size(), "OW areas");
  add(use.rooms_sprite.size(), "rooms");
  return parts.empty() ? std::string("no table uses it")
                       : "used by " + absl::StrJoin(parts, ", ");
}

void SheetBrowserPanel::DrawPendingSave() {
  if (!save_planner_ || state_->modified_sheets.empty()) {
    return;
  }
  if (!ImGui::CollapsingHeader(
          absl::StrFormat("Pending graphics save (%zu)###PendingSave",
                          state_->modified_sheets.size())
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  if (ImGui::SmallButton(ICON_MD_FACT_CHECK " Check save plan")) {
    auto plan = save_planner_();
    save_plan_sheets_ = state_->modified_sheets;
    if (plan.ok()) {
      save_plan_ = std::move(*plan);
      save_plan_error_.clear();
    } else {
      save_plan_.clear();
      save_plan_error_ = std::string(plan.status().message());
    }
  }
  HOVER_HINT(
      "Writes every dirty sheet to a scratch copy of the ROM and decodes it "
      "back. Saving the ROM does the same for real, and writes nothing if any "
      "sheet fails.");
  if (!save_plan_error_.empty()) {
    ImGui::TextColored(gui::GetErrorColor(), "%s", save_plan_error_.c_str());
    return;
  }
  if (save_plan_.empty()) {
    ImGui::TextDisabled("%s", tr("Check the plan before saving the ROM."));
    return;
  }
  if (save_plan_sheets_ != state_->modified_sheets) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       tr("Edits changed since the check; check again."));
  }
  for (const auto& entry : save_plan_) {
    const ImVec4 color =
        !entry.refusal.empty() ? gui::GetErrorColor()
        : entry.placement == zelda3::GfxSheetPlacement::kRelocated
            ? gui::GetWarningColor()
            : gui::GetSuccessColor();
    ImGui::TextColored(color, "%s",
                       DescribeGraphicsSavePlanEntry(entry).c_str());
    const std::string usage = SheetUsageSummary(entry.sheet_id);
    if (!usage.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", usage.c_str());
    }
  }
}

void SheetBrowserPanel::SetPngStatus(std::string message, bool is_error) {
  png_status_ = std::move(message);
  png_status_is_error_ = is_error;
}

absl::StatusOr<zelda3::SheetPalette> SheetBrowserPanel::PngPalette() {
  if (png_palette_mode_ == 0) {
    return zelda3::GrayscaleSheetPalette();
  }
  if (rom_ == nullptr || game_data_ == nullptr || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("Open a ROM to use room palettes");
  }
  return png_palette_mode_ == 1
             ? zelda3::RoomBackgroundSheetPalette(*rom_, *game_data_, png_room_,
                                                  png_palette_row_)
             : zelda3::RoomSpriteSheetPalette(*rom_, *game_data_, png_room_,
                                              png_palette_row_);
}

void SheetBrowserPanel::DrawPngTransfer(uint16_t sheet_id) {
  if (!ImGui::CollapsingHeader(tr("PNG export and import###SheetPng"))) {
    return;
  }
  // The Arena holds the last loaded ROM's sheets; only work on this one's.
  const bool arena_is_ours = game_data_ != nullptr &&
                             gfx::Arena::Get().gfx_sheets_owner() == game_data_;
  if (!arena_is_ours) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       tr("The sheets in memory belong to another open ROM."));
    return;
  }

  const char* modes[] = {tr("Grayscale"), tr("Room background row"),
                         tr("Room sprite row")};
  ImGui::SetNextItemWidth(180.0f);
  ImGui::Combo(tr("Palette##png"), &png_palette_mode_, modes,
               IM_ARRAYSIZE(modes));
  ImGui::SetNextItemWidth(90.0f);
  ImGui::InputInt(tr("Room##png"), &png_room_, 1, 16,
                  ImGuiInputTextFlags_CharsHexadecimal);
  png_room_ = std::clamp(png_room_, 0, 295);
  if (png_palette_mode_ != 0) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::SliderInt(tr("Row##png"), &png_palette_row_, 0, 7);
  }
  HOVER_HINT(
      "The room picks the palette rows and, for room PNGs, the room's 8 "
      "background sheets.");

  const auto& sheets = gfx::Arena::Get().gfx_sheets();
  const std::vector<uint8_t> pixels = sheet_id < sheets.size()
                                          ? sheets[sheet_id].vector()
                                          : std::vector<uint8_t>{};
  util::FileDialogOptions png_filter;
  png_filter.filters.push_back({"PNG image", "png"});

  if (ImGui::Button(ICON_MD_FILE_DOWNLOAD " Export sheet PNG")) {
    auto palette = PngPalette();
    auto png = palette.ok()
                   ? ExportSheetPixelsPng(sheet_id, pixels, *palette)
                   : absl::StatusOr<std::vector<uint8_t>>(palette.status());
    if (!png.ok()) {
      SetPngStatus(std::string(png.status().message()), true);
    } else {
      const std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
          absl::StrFormat("sheet_%02X", sheet_id), "png");
      if (!path.empty()) {
        const absl::Status status = util::WriteBinaryFile(path, *png);
        SetPngStatus(
            status.ok() ? "Exported " + path : std::string(status.message()),
            !status.ok());
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FILE_UPLOAD " Import into sheet")) {
    auto palette = PngPalette();
    const std::string path =
        palette.ok() ? util::FileDialogWrapper::ShowOpenFileDialog(png_filter)
                     : std::string();
    if (!palette.ok()) {
      SetPngStatus(std::string(palette.status().message()), true);
    } else if (!path.empty()) {
      auto bytes = util::ReadBinaryFile(path);
      auto preview =
          bytes.ok() ? PreviewSheetPngImport(sheet_id, pixels, *bytes, *palette)
                     : absl::StatusOr<SheetPngImportPreview>(bytes.status());
      if (!preview.ok()) {
        png_pending_.clear();
        SetPngStatus(std::string(preview.status().message()), true);
      } else {
        png_pending_ = {*preview};
        SetPngStatus("Preview of " + path, false);
      }
    }
  }

  if (ImGui::Button(ICON_MD_FILE_DOWNLOAD " Export room PNG")) {
    auto palette = PngPalette();
    if (!palette.ok()) {
      SetPngStatus(std::string(palette.status().message()), true);
    } else if (rom_ != nullptr && game_data_ != nullptr) {
      const auto sets =
          zelda3::ResolveAllRoomBackgroundSets(*rom_, *game_data_);
      auto png =
          zelda3::ExportRoomBackgroundPng(*rom_, sets[png_room_], *palette);
      if (!png.ok()) {
        SetPngStatus(std::string(png.status().message()), true);
      } else {
        const std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
            absl::StrFormat("room_%03X_background", png_room_), "png");
        if (!path.empty()) {
          const absl::Status status = util::WriteBinaryFile(path, *png);
          SetPngStatus(
              status.ok() ? "Exported " + path + " (sheets as saved in the ROM)"
                          : std::string(status.message()),
              !status.ok());
        }
      }
    }
  }
  HOVER_HINT(
      "128x256: the room's 8 background sheets in slot order, read from the "
      "ROM (save pixel edits first to include them).");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FILE_UPLOAD " Import room PNG")) {
    auto palette = PngPalette();
    const std::string path =
        palette.ok() ? util::FileDialogWrapper::ShowOpenFileDialog(png_filter)
                     : std::string();
    if (!palette.ok()) {
      SetPngStatus(std::string(palette.status().message()), true);
    } else if (!path.empty() && rom_ != nullptr && game_data_ != nullptr) {
      const auto sets =
          zelda3::ResolveAllRoomBackgroundSets(*rom_, *game_data_);
      auto bytes = util::ReadBinaryFile(path);
      auto previews =
          bytes.ok()
              ? PreviewRoomPngImport(*rom_, sets[png_room_], *bytes, *palette)
              : absl::StatusOr<std::vector<SheetPngImportPreview>>(
                    bytes.status());
      png_pending_.clear();
      if (!previews.ok()) {
        SetPngStatus(std::string(previews.status().message()), true);
      } else {
        // The room preview starts from the ROM's sheets, so it would discard
        // unsaved pixel edits to the same sheets.
        std::vector<std::string> unsaved;
        for (const auto& preview : *previews) {
          if (state_->modified_sheets.count(preview.sheet_id) != 0) {
            unsaved.push_back(absl::StrFormat("0x%02X", preview.sheet_id));
          }
        }
        if (!unsaved.empty()) {
          SetPngStatus(absl::StrFormat("Save or discard the pixel edits to "
                                       "sheet(s) %s before a room import",
                                       absl::StrJoin(unsaved, ", ")),
                       true);
        } else {
          png_pending_ = *previews;
          SetPngStatus("Preview of " + path, false);
        }
      }
    }
  }

  if (!png_status_.empty()) {
    ImGui::TextColored(
        png_status_is_error_ ? gui::GetErrorColor() : gui::GetInfoColor(), "%s",
        png_status_.c_str());
  }
  if (png_pending_.empty()) {
    return;
  }
  bool any_change = false;
  for (const auto& preview : png_pending_) {
    ImGui::BulletText("%s", DescribeSheetPngImport(preview).c_str());
    any_change = any_change || !preview.changed_tiles.empty();
  }
  ImGui::BeginDisabled(!any_change);
  if (ImGui::Button(ICON_MD_CHECK " Apply import")) {
    std::vector<std::string> errors;
    for (const auto& preview : png_pending_) {
      const absl::Status status =
          ApplySheetPngImport(preview, *state_, undo_manager_);
      if (!status.ok()) {
        errors.push_back(std::string(status.message()));
      }
    }
    SetPngStatus(errors.empty() ? std::string("Imported; save the ROM to write "
                                              "the sheets")
                                : absl::StrJoin(errors, "; "),
                 !errors.empty());
    png_pending_.clear();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CLOSE " Cancel")) {
    png_pending_.clear();
    SetPngStatus("Import cancelled", false);
  }
}

void SheetBrowserPanel::DrawSearchBar() {
  ImGui::Text(tr("Search:"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetHexInputWidth());
  if (ImGui::InputText("##SheetSearch", search_buffer_, sizeof(search_buffer_),
                       ImGuiInputTextFlags_CharsHexadecimal)) {
    // Parse hex input for sheet number
    if (strlen(search_buffer_) > 0) {
      int value = static_cast<int>(strtol(search_buffer_, nullptr, 16));
      if (value >= 0 && value <= 222) {
        state_->SelectSheet(static_cast<uint16_t>(value));
      }
    }
  }
  HOVER_HINT("Enter hex sheet number (00-DE)");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetCompactInputWidth());
  ImGui::DragInt("##FilterMin", &filter_min_, 1.0f, 0, 222, "%02X");
  ImGui::SameLine();
  ImGui::Text("-");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetCompactInputWidth());
  ImGui::DragInt("##FilterMax", &filter_max_, 1.0f, 0, 222, "%02X");

  ImGui::SameLine();
  ImGui::Checkbox(tr("Modified"), &show_only_modified_);
  HOVER_HINT("Show only modified sheets");
}

void SheetBrowserPanel::DrawBatchOperations() {
  if (ImGui::Button(ICON_MD_SELECT_ALL " Select All")) {
    for (int i = filter_min_; i <= filter_max_; i++) {
      state_->selected_sheets.insert(static_cast<uint16_t>(i));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DESELECT " Clear")) {
    state_->selected_sheets.clear();
  }

  if (!state_->selected_sheets.empty()) {
    ImGui::SameLine();
    ImGui::Text(tr("(%zu selected)"), state_->selected_sheets.size());
  }

  // Thumbnail size slider
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::LayoutHelpers::GetSliderWidth());
  ImGui::SliderFloat("##Scale", &thumbnail_scale_, 1.0f, 4.0f, "%.1fx");
}

void SheetBrowserPanel::DrawSheetGrid() {
  ImGui::BeginChild("##SheetGridChild", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  auto& sheets = gfx::Arena::Get().gfx_sheets();

  // Calculate thumbnail size
  const float thumb_width = 128 * thumbnail_scale_;
  const float thumb_height = 32 * thumbnail_scale_;
  const float padding = 4.0f;

  // Calculate columns based on available width
  float available_width = ImGui::GetContentRegionAvail().x;
  columns_ = std::max(
      1, static_cast<int>(available_width / (thumb_width + padding * 2)));

  int col = 0;
  for (int i = filter_min_; i <= filter_max_ && i < zelda3::kNumGfxSheets;
       i++) {
    // Filter by modification state if enabled
    if (show_only_modified_ &&
        state_->modified_sheets.find(static_cast<uint16_t>(i)) ==
            state_->modified_sheets.end()) {
      continue;
    }

    if (col > 0) {
      ImGui::SameLine();
    }

    ImGui::PushID(i);
    DrawSheetThumbnail(i, sheets[i]);
    ImGui::PopID();

    col++;
    if (col >= columns_) {
      col = 0;
    }
  }

  ImGui::EndChild();
}

void SheetBrowserPanel::DrawSheetThumbnail(int sheet_id, gfx::Bitmap& bitmap) {
  const float thumb_width = 128 * thumbnail_scale_;
  const float thumb_height = 32 * thumbnail_scale_;

  bool is_selected =
      state_->current_sheet_id == static_cast<uint16_t>(sheet_id);
  bool is_multi_selected =
      state_->selected_sheets.count(static_cast<uint16_t>(sheet_id)) > 0;
  bool is_modified =
      state_->modified_sheets.count(static_cast<uint16_t>(sheet_id)) > 0;

  // Selection highlight
  std::optional<gui::StyleColorGuard> sel_bg_guard;
  if (is_selected) {
    ImVec4 sel_bg = gui::GetSelectedColor();
    sel_bg.w = 0.3f;
    sel_bg_guard.emplace(ImGuiCol_ChildBg, sel_bg);
  } else if (is_multi_selected) {
    ImVec4 multi_bg = gui::GetModifiedColor();
    multi_bg.w = 0.3f;
    sel_bg_guard.emplace(ImGuiCol_ChildBg, multi_bg);
  }

  ImGui::BeginChild(absl::StrFormat("##Sheet%02X", sheet_id).c_str(),
                    ImVec2(thumb_width + 8, thumb_height + 24), true,
                    ImGuiWindowFlags_NoScrollbar);

  gui::BitmapPreviewOptions preview_opts;
  preview_opts.canvas_size = ImVec2(thumb_width + 1, thumb_height + 1);
  preview_opts.dest_pos = ImVec2(2, 2);
  preview_opts.dest_size = ImVec2(thumb_width - 2, thumb_height - 2);
  preview_opts.grid_step = 8.0f * thumbnail_scale_;
  preview_opts.draw_context_menu = false;
  preview_opts.ensure_texture = true;

  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = preview_opts.canvas_size;
  frame_opts.draw_context_menu = preview_opts.draw_context_menu;
  frame_opts.draw_grid = preview_opts.draw_grid;
  frame_opts.grid_step = preview_opts.grid_step;
  frame_opts.draw_overlay = preview_opts.draw_overlay;
  frame_opts.render_popups = preview_opts.render_popups;

  {
    thumbnail_canvas_.GetConfig().role = gui::CanvasRole::kPreviewOnly;
    auto rt = gui::BeginCanvas(thumbnail_canvas_, frame_opts);
    gui::DrawBitmapPreview(rt, bitmap, preview_opts);

    // Sheet label with modification indicator
    std::string label = absl::StrFormat("%02X", sheet_id);
    if (is_modified) {
      label += "*";
    }

    // Draw label with background
    ImVec2 text_pos = ImGui::GetCursorScreenPos();
    ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    thumbnail_canvas_.AddRectFilledAt(
        ImVec2(2, 2), ImVec2(text_size.x + 4, text_size.y + 2),
        is_modified ? IM_COL32(180, 100, 0, 200) : IM_COL32(0, 100, 0, 180));

    thumbnail_canvas_.AddTextAt(ImVec2(4, 2), label,
                                is_modified ? IM_COL32(255, 200, 100, 255)
                                            : IM_COL32(150, 255, 150, 255));
    if (is_modified) {
      // Dirty dot: unsaved pixel edits in this sheet.
      const ImVec2 dot =
          ImVec2(thumbnail_canvas_.zero_point().x + thumb_width - 8,
                 thumbnail_canvas_.zero_point().y + thumb_height - 8);
      ImGui::GetWindowDrawList()->AddCircleFilled(
          dot, 4.0f, ImGui::GetColorU32(gui::GetModifiedColor()));
    }

    if (inventory_.has_value() &&
        sheet_id < static_cast<int>(inventory_->sheets.size())) {
      const auto& entry = inventory_->sheets[sheet_id];
      const float block_w = preview_opts.dest_size.x / 8.0f;
      const float block_h = preview_opts.dest_size.y / 2.0f;
      if (show_free_blocks_) {
        for (int block : entry.FreeBlocks()) {
          thumbnail_canvas_.AddRectFilledAt(
              ImVec2(preview_opts.dest_pos.x + (block % 8) * block_w,
                     preview_opts.dest_pos.y + (block / 8) * block_h),
              ImVec2(block_w, block_h),
              entry.flagged ? IM_COL32(230, 180, 40, 70)
                            : IM_COL32(60, 200, 90, 70));
        }
      }
      const char* badge =
          entry.reserved ? "RES" : (entry.flagged ? "ASK" : nullptr);
      if (badge != nullptr) {
        const ImVec2 badge_size = ImGui::CalcTextSize(badge);
        const ImVec2 badge_pos(thumb_width - badge_size.x - 4, 2);
        thumbnail_canvas_.AddRectFilledAt(
            badge_pos, ImVec2(badge_size.x + 4, badge_size.y + 2),
            entry.reserved ? IM_COL32(170, 30, 30, 220)
                           : IM_COL32(170, 120, 20, 220));
        thumbnail_canvas_.AddTextAt(ImVec2(badge_pos.x + 2, badge_pos.y), badge,
                                    IM_COL32(255, 255, 255, 255));
      }
    }
    gui::EndCanvas(thumbnail_canvas_, rt, frame_opts);
  }

  // Click handling
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    if (ImGui::GetIO().KeyCtrl) {
      // Ctrl+click for multi-select
      if (is_multi_selected) {
        state_->selected_sheets.erase(static_cast<uint16_t>(sheet_id));
      } else {
        state_->selected_sheets.insert(static_cast<uint16_t>(sheet_id));
      }
    } else {
      // Normal click to select
      state_->SelectSheet(static_cast<uint16_t>(sheet_id));
    }
  }

  // Double-click to open in new tab
  if (ImGui::IsItemHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    state_->open_sheets.insert(static_cast<uint16_t>(sheet_id));
  }

  ImGui::EndChild();

  sel_bg_guard.reset();

  // Tooltip with sheet info
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text(tr("Sheet: 0x%02X (%d)"), sheet_id, sheet_id);
    if (const std::string label = SheetLabel(static_cast<uint16_t>(sheet_id));
        !label.empty()) {
      ImGui::TextUnformatted(label.c_str());
    }
    if (bitmap.is_active()) {
      ImGui::Text(tr("Size: %dx%d"), bitmap.width(), bitmap.height());
      ImGui::Text(tr("Depth: %d bpp"), bitmap.depth());
    } else {
      ImGui::Text(tr("(Inactive)"));
    }
    if (is_modified) {
      ImGui::TextColored(gui::GetModifiedColor(), tr("Modified"));
    }
    if (inventory_.has_value() &&
        sheet_id < static_cast<int>(inventory_->sheets.size())) {
      const auto& entry = inventory_->sheets[sheet_id];
      if (entry.reserved) {
        ImGui::TextColored(gui::GetErrorColor(), tr("Reserved"));
      } else if (entry.flagged) {
        ImGui::TextColored(gui::GetWarningColor(), tr("Flagged"));
      }
      if (entry.has_block_stats) {
        ImGui::Text(tr("Free 16x16 blocks: %zu"), entry.FreeBlocks().size());
      }
      if (!entry.labels.empty()) {
        ImGui::TextUnformatted(absl::StrJoin(entry.labels, "; ").c_str());
      }
    }
    ImGui::EndTooltip();
  }
}

}  // namespace editor
}  // namespace yaze

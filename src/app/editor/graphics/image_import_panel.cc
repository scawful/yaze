#include "app/editor/graphics/image_import_panel.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/sheet_png_transfer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"
#include "util/image_clipboard.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

namespace {

constexpr int kSheetWidth = 128;
constexpr int kSheetHeight = 32;
constexpr size_t kMaxPreviewPixels = 256 * 256;

ImU32 ToImU32(const std::array<uint8_t, 4>& color) {
  return IM_COL32(color[0], color[1], color[2], color[3]);
}

ImVec4 ToImVec4(const gfx::SnesColor& color) {
  const auto rgb = color.rgb();
  return ImVec4(rgb.x / 255.0f, rgb.y / 255.0f, rgb.z / 255.0f, 1.0f);
}

void DrawSwatch(const char* id, const ImVec4& color, const std::string& tip) {
  ImGui::ColorButton(id, color, ImGuiColorEditFlags_NoTooltip,
                     ImVec2(18.0f, 18.0f));
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tip.c_str());
  }
}

}  // namespace

void ImageImportPanel::SetStatus(std::string message, bool is_error) {
  status_ = std::move(message);
  status_is_error_ = is_error;
}

void ImageImportPanel::Load(absl::StatusOr<std::vector<uint8_t>> png_bytes,
                            const std::string& source) {
  auto image = png_bytes.ok()
                   ? ImportSheetImagePng(*png_bytes)
                   : absl::StatusOr<ImportedSheetImage>(png_bytes.status());
  if (!image.ok()) {
    image_.reset();
    source_.clear();
    SetStatus(std::string(image.status().message()), true);
    return;
  }
  image_ = std::move(*image);
  source_ = source;
  SetStatus("Imported " + source + ": " + DescribeImportedSheetImage(*image_),
            false);
}

absl::Status ImageImportPanel::Update() {
  ImGui::TextWrapped(
      "Imports keep the image's palette order and pixel indices. Use an "
      "indexed PNG, or an RGB PNG with an 8-pixel palette strip under the "
      "art (8x8 swatches, swatch k = color k). Colors outside the strip are "
      "an error, never a nearest match.");

  util::FileDialogOptions png_filter;
  png_filter.filters.push_back({"PNG image", "png"});
  ImGui::SetNextItemWidth(320.0f);
  ImGui::InputTextWithHint("File##ImageImportPath", "empty: choose in a dialog",
                           &path_);
  HOVER_HINT(
      "PNG path for Open and the save buttons. Leave it empty to choose the "
      "file in a dialog.");
  if (ImGui::Button(ICON_MD_FILE_OPEN " Open PNG")) {
    const std::string path =
        path_.empty() ? util::FileDialogWrapper::ShowOpenFileDialog(png_filter)
                      : path_;
    if (!path.empty()) {
      Load(util::ReadBinaryFile(path), path);
    }
  }
  ImGui::SameLine();
  const bool clipboard = util::ImageClipboardSupported();
  ImGui::BeginDisabled(!clipboard);
  if (ImGui::Button(ICON_MD_CONTENT_PASTE " Paste from clipboard")) {
    Load(util::GetClipboardPng(), "clipboard");
  }
  ImGui::EndDisabled();
  HOVER_HINT(
      clipboard
          ? "Reads the clipboard image. An indexed PNG keeps its indices; a "
            "TIFF-only image arrives as RGB and needs a palette strip."
          : "The image clipboard is only available on macOS.");

  if (!status_.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    if (status_is_error_) {
      ImGui::TextColored(gui::GetErrorColor(), "%s", status_.c_str());
    } else {
      ImGui::TextDisabled("%s", status_.c_str());
    }
    ImGui::PopTextWrapPos();
  }
  if (!image_) {
    return absl::OkStatus();
  }

  ImGui::Separator();
  DrawImage();
  DrawSaveButtons();
  ImGui::Separator();
  DrawWriteIntoSheet();
  ImGui::Separator();
  DrawPaletteImport();
  return absl::OkStatus();
}

void ImageImportPanel::DrawImage() {
  const auto& image = *image_;
  ImGui::Text("%s", DescribeImportedSheetImage(image).c_str());
  for (size_t i = 0; i < image.palette.size(); ++i) {
    const auto& c = image.palette[i];
    if (i % 16 != 0) {
      ImGui::SameLine(0.0f, 2.0f);
    }
    DrawSwatch(absl::StrFormat("##ImportColor%zu", i).c_str(),
               ImVec4(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, 1.0f),
               absl::StrFormat("%zu: #%02X%02X%02X%s", i, c[0], c[1], c[2],
                               c[3] == 0 ? " (transparent)" : ""));
  }

  const size_t pixels = static_cast<size_t>(image.width) * image.height;
  if (pixels > kMaxPreviewPixels) {
    ImGui::TextDisabled("Image is too large to preview");
    return;
  }
  const float avail = std::max(ImGui::GetContentRegionAvail().x, 64.0f);
  const float scale = std::clamp(
      std::floor(avail / static_cast<float>(image.width)), 1.0f, 3.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  auto* draw = ImGui::GetWindowDrawList();
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const uint8_t index =
          image.indices[static_cast<size_t>(y) * image.width + x];
      if (index >= image.palette.size()) {
        continue;
      }
      const ImVec2 a(origin.x + x * scale, origin.y + y * scale);
      draw->AddRectFilled(a, ImVec2(a.x + scale, a.y + scale),
                          ToImU32(image.palette[index]));
    }
  }
  const ImVec2 size(image.width * scale, image.height * scale);
  draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                IM_COL32(128, 128, 128, 255));
  ImGui::Dummy(size);
}

void ImageImportPanel::DrawSaveButtons() {
  const auto& image = *image_;
  auto save_path = [this](const std::string& default_name,
                          const std::string& extension) {
    return path_.empty() ? util::FileDialogWrapper::ShowSaveFileDialog(
                               default_name, extension)
                         : path_;
  };
  auto save = [&](absl::StatusOr<std::vector<uint8_t>> bytes,
                  const std::string& default_name,
                  const std::string& extension) {
    if (!bytes.ok()) {
      SetStatus(std::string(bytes.status().message()), true);
      return;
    }
    const std::string path = save_path(default_name, extension);
    if (path.empty()) {
      return;
    }
    const absl::Status status = util::WriteBinaryFile(path, *bytes);
    SetStatus(status.ok() ? absl::StrFormat("Saved %zu bytes to %s",
                                            bytes->size(), path)
                          : std::string(status.message()),
              !status.ok());
  };

  if (ImGui::Button(ICON_MD_SAVE " Save 4bpp .bin")) {
    save(EncodeImportedSheetTiles(image, 4), "import_4bpp", "bin");
  }
  HOVER_HINT("SNES 4bpp tiles, 32 bytes per 8x8 tile, tiles row-major.");
  ImGui::SameLine();
  ImGui::BeginDisabled(image.max_index > 7);
  if (ImGui::Button(ICON_MD_SAVE " Save 3bpp .bin")) {
    save(EncodeImportedSheetTiles(image, 3), "import_3bpp", "bin");
  }
  ImGui::EndDisabled();
  HOVER_HINT(image.max_index > 7
                 ? "The art uses indices above 7, which 3bpp cannot hold."
                 : "SNES 3bpp tiles, 24 bytes per 8x8 tile, tiles row-major.");
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE " Save indexed PNG")) {
    save(EncodeImportedSheetPng(image), "import_indexed", "png");
  }
  HOVER_HINT("The art without the strip, as an indexed PNG in index order.");
}

void ImageImportPanel::DrawWriteIntoSheet() {
  const auto& image = *image_;
  const uint16_t sheet_id = state_->current_sheet_id;
  const bool arena_is_ours =
      state_->game_data != nullptr &&
      gfx::Arena::Get().gfx_sheets_owner() == state_->game_data;
  const bool fits = image.width == kSheetWidth &&
                    image.height == kSheetHeight && image.max_index <= 7;
  ImGui::BeginDisabled(!arena_is_ours || !fits);
  const std::string label = absl::StrFormat(
      ICON_MD_FILE_UPLOAD " Write into sheet 0x%02X###ImageImportWrite",
      sheet_id);
  if (ImGui::Button(label.c_str())) {
    zelda3::SheetPalette palette{};
    for (size_t i = 0; i < palette.size() && i < image.palette.size(); ++i) {
      palette[i] = {image.palette[i][0], image.palette[i][1],
                    image.palette[i][2]};
    }
    const auto& sheets = gfx::Arena::Get().gfx_sheets();
    const std::vector<uint8_t> pixels = sheet_id < sheets.size()
                                            ? sheets[sheet_id].vector()
                                            : std::vector<uint8_t>{};
    auto png = EncodeImportedSheetPng(image);
    auto preview = png.ok()
                       ? PreviewSheetPngImport(sheet_id, pixels, *png, palette)
                       : absl::StatusOr<SheetPngImportPreview>(png.status());
    const absl::Status status =
        preview.ok() ? ApplySheetPngImport(*preview, *state_, undo_manager_)
                     : preview.status();
    SetStatus(status.ok() ? "Wrote " + DescribeSheetPngImport(*preview) +
                                "; undo reverts it, Save writes the ROM"
                          : std::string(status.message()),
              !status.ok());
  }
  ImGui::EndDisabled();
  HOVER_HINT(
      !arena_is_ours
          ? "The sheets in memory belong to another open ROM."
          : (!fits ? "Only 128x32 art with indices 0-7 fits a 3bpp sheet."
                   : "Writes the indices as they are, as one undoable step. "
                     "The palette is not changed."));
}

void ImageImportPanel::DrawPaletteImport() {
  const auto& image = *image_;
  const uint16_t sheet_id = state_->current_sheet_id;
  const GraphicsPaletteSelection* selection = state_->PreviewPalette(sheet_id);
  zelda3::GameData* data = state_->game_data;
  gfx::PaletteGroup* group =
      selection != nullptr && data != nullptr
          ? data->palette_groups.get_group(selection->group)
          : nullptr;
  if (selection == nullptr || group == nullptr || selection->palette < 0 ||
      selection->palette >= static_cast<int>(group->size())) {
    ImGui::TextDisabled(
        "To import the image's colors too, pick a preview palette for sheet "
        "0x%02X in Palette Controls. Colors are never imported "
        "automatically.",
        sheet_id);
    return;
  }

  // Copied: refreshing the previews below may rewrite the selection map.
  const GraphicsPaletteSelection chosen = *selection;
  const auto& target = group->palette_ref(chosen.palette);
  const int count =
      std::min<int>(7, static_cast<int>(image.palette.size()) - 1);
  auto slots =
      PreviewPaletteSlots(chosen.group, target.size(), chosen.row, count);
  const std::string label =
      absl::StrFormat(ICON_MD_PALETTE
                      " Import colors 1-%d into %s palette %d row "
                      "%d...###ImageImportPalette",
                      count, chosen.group, chosen.palette, chosen.row);
  ImGui::BeginDisabled(!slots.ok() || count < 1);
  if (ImGui::Button(label.c_str())) {
    ImGui::OpenPopup("Import palette colors?###ImageImportPaletteConfirm");
  }
  ImGui::EndDisabled();
  if (!slots.ok()) {
    ImGui::TextColored(gui::GetWarningColor(), "%s",
                       std::string(slots.status().message()).c_str());
    return;
  }
  HOVER_HINT(
      "Writes the image's colors 1-7 into the sheet's preview palette row, "
      "after a confirmation. Undo reverts it. Index 0 stays transparent.");

  if (!ImGui::BeginPopupModal(
          "Import palette colors?###ImageImportPaletteConfirm", nullptr,
          ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::Text(
      "%s palette %d, row %d (SNES colors are 15-bit; low bits "
      "are dropped):",
      chosen.group.c_str(), chosen.palette, chosen.row);
  std::vector<gfx::SnesColor> colors;
  for (int i = 0; i < count; ++i) {
    const auto& c = image.palette[i + 1];
    colors.emplace_back(c[0], c[1], c[2]);
    const int slot = (*slots)[i];
    DrawSwatch(absl::StrFormat("##Old%d", slot).c_str(), ToImVec4(target[slot]),
               absl::StrFormat("Slot %d now", slot));
    ImGui::SameLine();
    ImGui::TextUnformatted(ICON_MD_ARROW_FORWARD);
    ImGui::SameLine();
    DrawSwatch(absl::StrFormat("##New%d", slot).c_str(), ToImVec4(colors[i]),
               absl::StrFormat("Image color %d: #%02X%02X%02X", i + 1, c[0],
                               c[1], c[2]));
    ImGui::SameLine();
    ImGui::Text("slot %d <- image color %d", slot, i + 1);
  }
  if (ImGui::Button("Import colors")) {
    auto& manager = gfx::PaletteManager::Get();
    absl::Status status = absl::OkStatus();
    if (!manager.IsManaging(data)) {
      status = absl::FailedPreconditionError(
          "Palette editing is not active for this ROM");
    } else {
      manager.BeginBatch();
      for (int i = 0; i < count && status.ok(); ++i) {
        status = manager.SetColor(chosen.group, chosen.palette, (*slots)[i],
                                  colors[i]);
      }
      manager.EndBatch();
      RefreshGraphicsPreviewPalettes(*state_);
    }
    SetStatus(status.ok() ? absl::StrFormat("Imported %d colors into %s "
                                            "palette %d row %d",
                                            count, chosen.group, chosen.palette,
                                            chosen.row)
                          : std::string(status.message()),
              !status.ok());
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

}  // namespace yaze::editor

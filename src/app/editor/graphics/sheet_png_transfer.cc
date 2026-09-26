#include "app/editor/graphics/sheet_png_transfer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::editor {

namespace {

constexpr size_t kSheetPixels = 128 * 32;

// The Arena's 3bpp sheets are 128x32 with indices 0-7; anything else cannot
// round-trip through the 3bpp encoder without losing bits.
absl::Status CheckSheetPixels(uint16_t sheet_id,
                              const std::vector<uint8_t>& indexed_pixels) {
  if (sheet_id >= zelda3::kGfxSheetCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Sheet 0x%02X is out of range", sheet_id));
  }
  if (zelda3::GetGfxSheetStorageKind(sheet_id) ==
      zelda3::GfxSheetStorageKind::kCompressed2bpp) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Sheet 0x%02X is a 2bpp sheet; PNG import and export cover 3bpp "
        "sheets only",
        sheet_id));
  }
  if (indexed_pixels.size() != kSheetPixels) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Sheet 0x%02X is not loaded as a 128x32 sheet", sheet_id));
  }
  const auto high = std::find_if(indexed_pixels.begin(), indexed_pixels.end(),
                                 [](uint8_t index) { return index > 7; });
  if (high != indexed_pixels.end()) {
    const size_t at = static_cast<size_t>(high - indexed_pixels.begin());
    return absl::FailedPreconditionError(absl::StrFormat(
        "Sheet 0x%02X uses color %d at (%zu, %zu); 3bpp sheets hold colors "
        "0-7",
        sheet_id, *high, at % 128, at / 128));
  }
  return absl::OkStatus();
}

SheetPngImportPreview MakePreview(uint16_t sheet_id,
                                  const zelda3::SheetImportResult& result) {
  SheetPngImportPreview preview;
  preview.sheet_id = sheet_id;
  preview.indexed_pixels = gfx::SnesTo8bppSheet(result.snes_3bpp, /*bpp=*/3);
  preview.changed_blocks = result.changed_blocks;
  preview.changed_tiles = result.changed_tiles;
  return preview;
}

}  // namespace

absl::StatusOr<std::vector<uint8_t>> ExportSheetPixelsPng(
    uint16_t sheet_id, const std::vector<uint8_t>& indexed_pixels,
    const zelda3::SheetPalette& palette) {
  RETURN_IF_ERROR(CheckSheetPixels(sheet_id, indexed_pixels));
  const auto snes = gfx::IndexedToSnesSheet(indexed_pixels, /*bpp=*/3);
  return zelda3::ExportSheetBlocksPng(snes, palette);
}

absl::StatusOr<SheetPngImportPreview> PreviewSheetPngImport(
    uint16_t sheet_id, const std::vector<uint8_t>& indexed_pixels,
    const std::vector<uint8_t>& png_bytes,
    const zelda3::SheetPalette& palette) {
  RETURN_IF_ERROR(CheckSheetPixels(sheet_id, indexed_pixels));
  ASSIGN_OR_RETURN(const util::PngImage png, util::DecodePng(png_bytes));
  const auto snes = gfx::IndexedToSnesSheet(indexed_pixels, /*bpp=*/3);
  ASSIGN_OR_RETURN(
      const zelda3::SheetImportResult result,
      zelda3::ImportSheetBlocksPng(snes, png, /*first_block=*/0, palette));
  return MakePreview(sheet_id, result);
}

absl::StatusOr<std::vector<SheetPngImportPreview>> PreviewRoomPngImport(
    const Rom& rom, const zelda3::RoomBackgroundSet& set,
    const std::vector<uint8_t>& png_bytes,
    const zelda3::SheetPalette& palette) {
  ASSIGN_OR_RETURN(const util::PngImage png, util::DecodePng(png_bytes));
  ASSIGN_OR_RETURN(const auto imports,
                   zelda3::ImportRoomBackgroundPng(rom, set, png, palette));
  std::vector<SheetPngImportPreview> previews;
  previews.reserve(imports.size());
  for (const zelda3::RoomSheetImport& sheet_import : imports) {
    previews.push_back(MakePreview(sheet_import.sheet, sheet_import.result));
  }
  return previews;
}

absl::Status ApplySheetPngImport(const SheetPngImportPreview& preview,
                                 GraphicsEditorState& state,
                                 UndoManager* undo_manager) {
  auto* sheets = gfx::Arena::Get().mutable_gfx_sheets();
  if (preview.sheet_id >= sheets->size()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Sheet 0x%02X is out of range", preview.sheet_id));
  }
  gfx::Bitmap& sheet = sheets->at(preview.sheet_id);
  if (!sheet.is_active() || sheet.vector().size() != kSheetPixels ||
      preview.indexed_pixels.size() != kSheetPixels) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Sheet 0x%02X is not loaded as a 128x32 sheet; import it again",
        preview.sheet_id));
  }
  if (sheet.vector() == preview.indexed_pixels) {
    return absl::OkStatus();
  }

  std::vector<uint8_t> before = sheet.vector();
  sheet.set_data(preview.indexed_pixels);
  gfx::Arena::Get().NotifySheetModified(preview.sheet_id);
  state.MarkSheetModified(preview.sheet_id);
  if (undo_manager != nullptr) {
    undo_manager->Push(std::make_unique<GraphicsPixelEditAction>(
        preview.sheet_id, std::move(before), preview.indexed_pixels,
        absl::StrFormat("Import PNG into sheet 0x%02X", preview.sheet_id),
        [&state](uint16_t sheet_id) { state.MarkSheetModified(sheet_id); }));
  }
  return absl::OkStatus();
}

std::string DescribeSheetPngImport(const SheetPngImportPreview& preview) {
  if (preview.changed_tiles.empty()) {
    return absl::StrFormat("Sheet 0x%02X: no change", preview.sheet_id);
  }
  return absl::StrFormat(
      "Sheet 0x%02X: block%s %s (%zu tile%s)", preview.sheet_id,
      preview.changed_blocks.size() == 1 ? "" : "s",
      absl::StrJoin(preview.changed_blocks, ", "), preview.changed_tiles.size(),
      preview.changed_tiles.size() == 1 ? "" : "s");
}

}  // namespace yaze::editor

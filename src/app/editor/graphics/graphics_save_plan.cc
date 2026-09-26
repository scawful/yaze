#include "app/editor/graphics/graphics_save_plan.h"

#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "util/macro.h"

namespace yaze::editor {

absl::Status VerifyWrittenSheetPixels(
    const Rom& rom, uint16_t sheet_id,
    const std::vector<uint8_t>& expected_pixels,
    const zelda3::GfxSheetPointerTables& tables) {
  ASSIGN_OR_RETURN(const auto exact,
                   zelda3::ReadGfxSheetData(rom, sheet_id, tables));
  if (gfx::SnesTo8bppSheet(exact, /*bpp=*/3) != expected_pixels) {
    return absl::DataLossError(absl::StrFormat(
        "Sheet 0x%02X decodes (game decoder) to different pixels than were "
        "edited",
        sheet_id));
  }
  if (zelda3::GetGfxSheetStorageKind(sheet_id) !=
      zelda3::GfxSheetStorageKind::kCompressed3bpp) {
    return absl::OkStatus();
  }
  // The editor loads sheets with DecompressV2 (zelda3::LoadGameData); the
  // sheet must also reopen as edited through that path.
  ASSIGN_OR_RETURN(const uint32_t pc,
                   zelda3::ReadGfxSheetPc(rom, sheet_id, tables));
  ASSIGN_OR_RETURN(auto loaded, gfx::lc_lz2::DecompressV2(
                                    rom.data(), static_cast<int>(pc), 0x800,
                                    /*mode=*/1, rom.size()));
  if (loaded.size() < zelda3::kGfxSheet3bppBytes) {
    return absl::DataLossError(
        absl::StrFormat("Sheet 0x%02X reopens (editor loader) with %zu bytes",
                        sheet_id, loaded.size()));
  }
  loaded.resize(zelda3::kGfxSheet3bppBytes);
  if (gfx::SnesTo8bppSheet(loaded, /*bpp=*/3) != expected_pixels) {
    return absl::DataLossError(absl::StrFormat(
        "Sheet 0x%02X reopens (editor loader) with different pixels than "
        "were edited",
        sheet_id));
  }
  return absl::OkStatus();
}

std::string DescribeGraphicsSavePlanEntry(const GraphicsSavePlanEntry& entry) {
  if (!entry.refusal.empty()) {
    return absl::StrFormat("Sheet 0x%02X: refused (%s)", entry.sheet_id,
                           entry.refusal);
  }
  if (entry.placement == zelda3::GfxSheetPlacement::kRelocated) {
    return absl::StrFormat(
        "Sheet 0x%02X: relocated 0x%06X -> 0x%06X, %zu -> %zu bytes",
        entry.sheet_id, entry.old_pc, entry.new_pc, entry.old_stored_size,
        entry.new_stored_size);
  }
  return absl::StrFormat("Sheet 0x%02X: in place at 0x%06X, %zu -> %zu bytes",
                         entry.sheet_id, entry.new_pc, entry.old_stored_size,
                         entry.new_stored_size);
}

}  // namespace yaze::editor

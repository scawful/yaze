#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SAVE_PLAN_H
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SAVE_PLAN_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
class Rom;

namespace editor {

/// One dirty sheet in a pending graphics save: what the writer does with
/// it, measured on a scratch copy of the ROM, or why it refuses.
struct GraphicsSavePlanEntry {
  uint16_t sheet_id = 0;
  std::string refusal;  // empty when the sheet can be saved
  zelda3::GfxSheetPlacement placement = zelda3::GfxSheetPlacement::kInPlace;
  uint32_t old_pc = 0;
  uint32_t new_pc = 0;
  size_t old_stored_size = 0;
  size_t new_stored_size = 0;
};

/// Decodes a written sheet with the game's decoder (ReadGfxSheetData) and,
/// for compressed sheets, with the editor's loader (DecompressV2), and
/// compares both with the edited pixels (128x32 indices). DataLoss on any
/// difference.
absl::Status VerifyWrittenSheetPixels(
    const Rom& rom, uint16_t sheet_id,
    const std::vector<uint8_t>& expected_pixels,
    const zelda3::GfxSheetPointerTables& tables);

/// "Sheet 0x92: in place at 0x0AED16, 1476 -> 1473 bytes".
std::string DescribeGraphicsSavePlanEntry(const GraphicsSavePlanEntry& entry);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SAVE_PLAN_H

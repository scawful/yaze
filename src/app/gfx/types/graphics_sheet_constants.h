#ifndef YAZE_APP_GFX_TYPES_GRAPHICS_SHEET_CONSTANTS_H_
#define YAZE_APP_GFX_TYPES_GRAPHICS_SHEET_CONSTANTS_H_

#include <cstddef>

namespace yaze::gfx {

// Vanilla has 00..DE. Expanded pointer tables can expose DF..FE; FF is
// always the default-sheet sentinel, never a stored graphics sheet.
inline constexpr size_t kLegacyGraphicsSheetCount = 0xDF;
inline constexpr size_t kGraphicsSheetCapacity = 0xFF;

}  // namespace yaze::gfx

#endif  // YAZE_APP_GFX_TYPES_GRAPHICS_SHEET_CONSTANTS_H_

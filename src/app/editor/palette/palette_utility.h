#ifndef YAZE_APP_EDITOR_PALETTE_UTILITY_H
#define YAZE_APP_EDITOR_PALETTE_UTILITY_H

#include <string>

#include "app/gfx/types/snes_color.h"
#include "app/gui/core/color.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

class PaletteEditor;  // Forward declaration

/**
 * @brief Utility functions for palette operations across editors
 */
namespace palette_utility {

/**
 * @brief Draw a palette selector button that opens palette editor
 * @param label Button label
 * @param group_name The palette group name
 * @param palette_index The palette index within the group
 * @param editor Pointer to palette editor (can be null)
 * @return true if button was clicked
 */
bool DrawPaletteJumpButton(const char* label, const std::string& group_name,
                           int palette_index, PaletteEditor* editor);

/**
 * @brief Draw inline color edit with jump to palette
 * @param label Label for the color widget
 * @param color Color to edit
 * @param group_name Palette group this color belongs to
 * @param palette_index Palette index within group
 * @param color_index Color index within palette
 * @param editor Pointer to palette editor (can be null)
 * @return true if color was changed
 */
bool DrawInlineColorEdit(const char* label, gfx::SnesColor* color,
                         const std::string& group_name, int palette_index,
                         int color_index, PaletteEditor* editor);

/**
 * @brief Draw a compact palette ID selector with preview
 * @param label Label for the widget
 * @param palette_id Current palette ID (in/out)
 * @param group_name The palette group name
 * @param editor Pointer to palette editor (can be null)
 * @return true if palette ID changed
 */
bool DrawPaletteIdSelector(const char* label, int* palette_id,
                           const std::string& group_name,
                           PaletteEditor* editor);

/**
 * @brief Draw color info tooltip on hover
 * @param color The color to show info for
 */
void DrawColorInfoTooltip(const gfx::SnesColor& color);

/**
 * @brief Draw a small palette preview (8 colors in a row)
 * @param group_name Palette group name
 * @param palette_index Palette index
 * @param game_data GameData instance to read palette from
 */
void DrawPalettePreview(const std::string& group_name, int palette_index,
                        zelda3::GameData* game_data);

}  // namespace palette_utility

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_PALETTE_UTILITY_H

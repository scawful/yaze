#ifndef YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_WORKSPACE_STATE_H_
#define YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_WORKSPACE_STATE_H_

#include <cstdint>

#include "app/gfx/types/snes_palette.h"

namespace yaze::editor {

/**
 * @brief Per-ROM-session UI state shared by all GfxGroupEditor surfaces.
 *
 * Selection indices and preview palette controls stay in sync between the
 * Graphics editor dock and Overworld embeds. Each GfxGroupEditor keeps its own
 * gui::Canvas instances for ImGui ID isolation.
 */
struct GfxGroupWorkspaceState {
  uint8_t selected_blockset = 0;
  uint8_t selected_roomset = 0;
  uint8_t selected_spriteset = 0;
  float view_scale = 2.0f;
  gfx::PaletteCategory selected_palette_category =
      gfx::PaletteCategory::kDungeons;
  uint8_t selected_palette_index = 0;
  bool use_custom_palette = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_GFX_GROUP_WORKSPACE_STATE_H_

#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_CONTEXT_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_CONTEXT_H_

#include <array>
#include <span>
#include <string>
#include "app/editor/graphics/graphics_edit_context.h"
namespace yaze::editor::zsprite {
struct Frame;
}
namespace yaze::editor::sprite_graphics_context {
// Resolves every 8x8 component of the selected OAM entry independently.
absl::StatusOr<GraphicsEditContext> Make(
    const zsprite::Frame& frame, int frame_index, int oam_index,
    const std::array<uint8_t, 8>& slots, const gfx::PaletteGroup& palettes,
    const zelda3::GameData* owner, const std::string& source_label,
    const std::string& sheet_source = "sprite sheet selection");
// Captured pose/palette, live pixels from the owning session's store.
absl::StatusOr<usage_preview::PreviewImage> RenderCapturedFrame(
    const zsprite::Frame& frame, const std::array<uint8_t, 8>& slots,
    const gfx::PaletteGroup& palettes, const zelda3::GameData* owner);
// Common pose renderer for session-backed sheets and detached scratch slots.
absl::StatusOr<usage_preview::PreviewImage> RenderFrameBuffer(
    const zsprite::Frame& frame, std::span<const uint8_t> buffer,
    const gfx::PaletteGroup& palettes);
}  // namespace yaze::editor::sprite_graphics_context

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_GRAPHICS_CONTEXT_H_

#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_

#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"

namespace yaze {
namespace editor {
namespace internal {

// Initialize a sprite-preview bitmap once and queue its texture for creation.
// Idempotent: returns immediately if the bitmap is already active.
//
// SpriteEditor used to call Create()+Reformat() inline, never queueing a
// CREATE texture command, so canvas_rendering's `if (!texture()) return;`
// guard skipped the draw silently and the preview was always blank. This
// helper closes that gap and stamps the bitmap as a composite output (the
// pixels come from SpriteDrawer rendering OAM tiles into the surface, not
// from direct user paint).
//
// Safe to call every render frame; the `is_active()` short-circuit ensures
// only the first call does any work.
inline void EnsureSpritePreviewBitmapReady(
    gfx::Bitmap& bmp, int width, int height, int depth,
    const std::vector<uint8_t>& gfx_buffer) {
  if (bmp.is_active()) {
    return;
  }
  // Graphics sheets are input to the drawer, not initial composite pixels.
  const std::vector<uint8_t> blank(
      gfx_buffer.empty() ? 0 : static_cast<size_t>(width) * height, 0);
  bmp.Create(width, height, depth, blank);
  if (!bmp.is_active()) {
    // Create bailed (empty buffer, surface allocation failed). Don't call
    // Reformat. It would unconditionally allocate a fresh surface and set
    // active=true with no pixel data, masking the load failure and queuing
    // a CREATE for an effectively-blank bitmap. Leave the bitmap inactive
    // so the next frame's call retries Create with a real buffer.
    return;
  }
  bmp.Reformat(depth);
  if (bmp.surface() != nullptr) {
    bmp.CreateTexture();
  }
  bmp.metadata().purpose = gfx::Bitmap::BitmapPurpose::kCompositeOutput;
}

// SpriteDrawer writes Bitmap::mutable_data(); the renderer reads the SDL surface.
inline void PublishSpritePreviewPixels(gfx::Bitmap& bmp) {
  bmp.UpdateSurfacePixels();
  bmp.UpdateTexture();
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_EDITOR_INTERNAL_H_

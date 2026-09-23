#ifndef YAZE_APP_EDITOR_GRAPHICS_SCREEN_EDITOR_INTERNAL_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCREEN_EDITOR_INTERNAL_H_

#include "app/gfx/resource/bitmap_texture_queue.h"

namespace yaze {
namespace editor {
namespace internal {

// Make a composite-output bitmap safe to draw this frame.
//
// Composites (title-screen BG1+BG2 overlay, dungeon room renderer output,
// etc.) are owned by the editor pipeline that produced them, not by Arena's
// gfx_sheets array. The first time the editor reaches the canvas-draw call
// the bitmap may have a surface but no SDL texture yet, in which case
// canvas_rendering's `if (!texture()) return;` guard silently drops the
// draw and the canvas renders blank for one frame. A4 (title screen) and
// A3 (dungeon room) both surfaced this race during the audit.
//
// Behavior:
//   * surface == nullptr           -> no-op (bitmap not initialized).
//   * !is_active                   -> mark active.
//   * texture == nullptr           -> queue CREATE.
//   * texture != nullptr, modified -> queue UPDATE and clear modified.
//   * else                         -> no-op.
//
// Stamps `metadata().purpose = kCompositeOutput` so the canvas-render
// diagnostic log (canvas_rendering.cc) and any future role-aware tooling
// can identify the bitmap correctly.
//
// Safe to call every frame; the shared helper suppresses duplicate commands
// for the same bitmap generation while an upload is still pending.
inline void EnsureCompositeBitmapTextureQueued(gfx::Bitmap& composite) {
  gfx::EnsureCompositeBitmapTextureQueued(composite);
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_EDITOR_INTERNAL_H_

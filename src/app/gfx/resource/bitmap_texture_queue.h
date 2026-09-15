#ifndef YAZE_APP_GFX_RESOURCE_BITMAP_TEXTURE_QUEUE_H_
#define YAZE_APP_GFX_RESOURCE_BITMAP_TEXTURE_QUEUE_H_

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"

namespace yaze::gfx {

// Queue the first upload or an in-place refresh for a composed bitmap.
inline void EnsureCompositeBitmapTextureQueued(Bitmap& composite) {
  if (composite.surface() == nullptr) {
    return;
  }
  if (!composite.is_active()) {
    composite.set_active(true);
  }
  Arena& arena = Arena::Get();
  if (composite.texture() == nullptr) {
    if (!arena.HasPendingTextureCommand(Arena::TextureCommandType::CREATE,
                                        &composite)) {
      arena.QueueTextureCommand(Arena::TextureCommandType::CREATE, &composite);
    }
    composite.set_modified(false);
  } else if (composite.modified()) {
    if (!arena.HasPendingTextureCommand(Arena::TextureCommandType::UPDATE,
                                        &composite)) {
      arena.QueueTextureCommand(Arena::TextureCommandType::UPDATE, &composite);
    }
    composite.set_modified(false);
  }
  composite.metadata().purpose = Bitmap::BitmapPurpose::kCompositeOutput;
}

}  // namespace yaze::gfx

#endif  // YAZE_APP_GFX_RESOURCE_BITMAP_TEXTURE_QUEUE_H_

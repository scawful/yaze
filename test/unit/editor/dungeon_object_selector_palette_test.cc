#include "app/editor/dungeon/dungeon_object_selector.h"

#include <cstddef>

#include "app/gfx/types/snes_palette.h"
#include "gtest/gtest.h"

namespace yaze {
namespace editor {
namespace {

// Pins the cache-invalidation contract for DungeonObjectSelector's preview
// cache.
//
// The cache is keyed on (object_id, subtype, room.blockset(), room.palette()).
// None of those keys capture the *contents* of the active palette group, so
// switching dungeon palette banks (which the editor does via
// SetCurrentPaletteGroup, not by changing the slot value on the room) used
// to leave the cache holding entries whose colors were silently stale.
//
// The fix routes SetCurrentPaletteGroup through InvalidatePreviewCache.
// These tests pin that wiring without booting a renderer or ROM by
// observing the test-only invalidation counter.

TEST(DungeonObjectSelectorPaletteTest, SetCurrentPaletteGroupInvalidatesCache) {
  DungeonObjectSelector selector;
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 0u);

  selector.SetCurrentPaletteGroup(gfx::PaletteGroup("dungeon_main"));
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 1u)
      << "SetCurrentPaletteGroup must invalidate the preview cache so a "
         "subsequent palette-bank switch (e.g. green dungeon -> red dungeon "
         "with the same numeric slot) doesn't return stale colors";
}

TEST(DungeonObjectSelectorPaletteTest,
     RepeatedSetCurrentPaletteGroupInvalidatesEachTime) {
  // Each palette-group swap is conservatively treated as a distinct context,
  // even when the swap is to the same group. The cache rebuild cost is well
  // under one frame, and skipping invalidation here would require keeping a
  // fingerprint of the palette contents for equality comparison, which is
  // strictly worse than just rebuilding on demand.
  DungeonObjectSelector selector;
  selector.SetCurrentPaletteGroup(gfx::PaletteGroup("dungeon_main"));
  selector.SetCurrentPaletteGroup(gfx::PaletteGroup("ow_main"));
  selector.SetCurrentPaletteGroup(gfx::PaletteGroup("sprites_aux1"));
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 3u);
}

TEST(DungeonObjectSelectorPaletteTest,
     DirectInvalidatePreviewCacheBumpsCounter) {
  // The counter is the surface tests rely on; pin that the public
  // InvalidatePreviewCache (invoked by the custom-objects-added flow as well
  // as by SetCurrentPaletteGroup) shares the same accounting.
  DungeonObjectSelector selector;
  selector.InvalidatePreviewCache();
  selector.InvalidatePreviewCache();
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 2u);
}

TEST(DungeonObjectSelectorPaletteTest, InitialInvalidationCountIsZero) {
  // Defensive: a freshly-constructed selector must report no invalidations,
  // so a test asserting "+1" can rely on baseline 0 without an explicit
  // setup-phase reset.
  DungeonObjectSelector selector;
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 0u);
}

}  // namespace
}  // namespace editor
}  // namespace yaze

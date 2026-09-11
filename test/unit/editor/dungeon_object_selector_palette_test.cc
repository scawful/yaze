#include "app/editor/dungeon/dungeon_object_selector.h"

#include <cstddef>
#include <vector>

#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze {
namespace editor {

struct DungeonObjectSelectorTestAccess {
  static absl::Status OpenNewCustomObjectEditor(DungeonObjectSelector& selector,
                                                int width, int height,
                                                const std::string& filename,
                                                int16_t object_id,
                                                int room_id) {
    return selector.OpenNewCustomObjectEditor(width, height, filename,
                                              object_id, room_id);
  }

  static void SynchronizePreviewCacheRoomContext(
      DungeonObjectSelector& selector, const zelda3::Room& room) {
    selector.SynchronizePreviewCacheRoomContext(room);
  }

  static uint32_t MakeLayoutCacheKey(int object_id, uint8_t preview_size,
                                     const zelda3::Room* room) {
    return DungeonObjectSelector::MakeLayoutCacheKey(object_id, preview_size,
                                                     room);
  }

  static void SeedPreviewCache(DungeonObjectSelector& selector,
                               uint64_t cache_key) {
    selector.preview_cache_[cache_key] =
        std::make_unique<gfx::BackgroundBuffer>(8, 8);
  }

  static size_t PreviewCacheSize(const DungeonObjectSelector& selector) {
    return selector.preview_cache_.size();
  }

  static gfx::Bitmap* SeedInitializedPreviewCache(
      DungeonObjectSelector& selector, uint64_t cache_key) {
    auto preview = std::make_unique<gfx::BackgroundBuffer>(8, 8);
    preview->EnsureBitmapInitialized();
    gfx::Bitmap* bitmap = &preview->bitmap();
    selector.preview_cache_[cache_key] = std::move(preview);
    return bitmap;
  }
};

namespace {

size_t ActiveArenaSurfaceCount() {
  const auto& arena = gfx::Arena::Get();
  return arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
}

// Pins the cache-invalidation contract for DungeonObjectSelector's preview
// cache.
//
// The cache entry key covers object/subtype and compact room-header fields;
// room ID, entrance graphics, and palette-group swaps invalidate the complete
// cache before lookup. None of those fields capture the *contents* of the
// active palette group, so
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

TEST(DungeonObjectSelectorPaletteTest,
     InvalidationCancelsPendingCreateAndRetiresCachedTexture) {
  gfx::Arena& arena = gfx::Arena::Get();
  arena.ClearTextureQueue();
  const size_t active_surfaces_before = ActiveArenaSurfaceCount();
  DungeonObjectSelector selector;
  gfx::Bitmap* bitmap =
      DungeonObjectSelectorTestAccess::SeedInitializedPreviewCache(selector,
                                                                   0x1234);
  int texture_storage = 0;
  const gfx::TextureHandle texture = &texture_storage;
  bitmap->set_texture(texture);
  ASSERT_EQ(ActiveArenaSurfaceCount(), active_surfaces_before + 1);
  arena.QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE, bitmap);
  ASSERT_EQ(arena.texture_command_queue_size(), 1u);

  selector.InvalidatePreviewCache();

  EXPECT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 0u);
  EXPECT_EQ(ActiveArenaSurfaceCount(), active_surfaces_before);
  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 1u);

  ::testing::NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, DestroyTexture(texture)).Times(1);
  EXPECT_EQ(arena.DrainRetiredBitmaps(&renderer), 1u);
}

TEST(DungeonObjectSelectorPaletteTest,
     DestructionCancelsPendingCreateBeforePreviewOwnerDisappears) {
  gfx::Arena& arena = gfx::Arena::Get();
  arena.ClearTextureQueue();
  {
    DungeonObjectSelector selector;
    gfx::Bitmap* bitmap =
        DungeonObjectSelectorTestAccess::SeedInitializedPreviewCache(selector,
                                                                     0x1234);
    arena.QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE, bitmap);
    ASSERT_EQ(arena.texture_command_queue_size(), 1u);
  }

  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
}

TEST(DungeonObjectSelectorPaletteTest, InitialInvalidationCountIsZero) {
  // Defensive: a freshly-constructed selector must report no invalidations,
  // so a test asserting "+1" can rely on baseline 0 without an explicit
  // setup-phase reset.
  DungeonObjectSelector selector;
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 0u);
}

TEST(DungeonObjectSelectorPaletteTest,
     FloorHeaderChangesInvalidateRoomDependentPreviews) {
  DungeonObjectSelector selector;
  selector.set_current_room_id(7);
  zelda3::Room room;
  room.SetBlockset(2);
  room.SetPalette(3);
  room.set_floor1(4);
  room.set_floor2(5);

  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 1u);

  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 1u)
      << "An unchanged room context must keep its cached previews";

  room.set_floor1(6);
  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 2u);

  room.set_floor2(7);
  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 3u);
}

TEST(DungeonObjectSelectorPaletteTest,
     EntranceBlocksetChangeEvictsRoomGraphicsPreviews) {
  DungeonObjectSelector selector;
  selector.set_current_room_id(7);
  zelda3::Room room;
  room.SetBlockset(2);
  room.SetPalette(3);
  room.SetRenderEntranceBlockset(0x10);

  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 1u);

  DungeonObjectSelectorTestAccess::SeedPreviewCache(selector, 0x1234);
  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 1u)
      << "An unchanged entrance graphics context must retain its previews";

  room.SetRenderEntranceBlockset(0x11);
  DungeonObjectSelectorTestAccess::SynchronizePreviewCacheRoomContext(selector,
                                                                      room);
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 2u);
  EXPECT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 0u)
      << "A new entrance Main GFX group must evict thumbnails rendered from "
         "the prior room graphics buffer";
}

TEST(DungeonObjectSelectorPaletteTest,
     FloorObjectTooltipKeysIncludeTheirRoomHeaderSource) {
  zelda3::Room room;
  room.set_floor1(4);
  room.set_floor2(5);

  const uint32_t floor_one_key =
      DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
          0xC4, /*preview_size=*/2, &room);
  const uint32_t floor_two_key =
      DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
          0xDB, /*preview_size=*/2, &room);
  const uint32_t unrelated_key =
      DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
          0x34, /*preview_size=*/2, &room);

  room.set_floor1(6);
  EXPECT_NE(DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
                0xC4, /*preview_size=*/2, &room),
            floor_one_key);
  EXPECT_EQ(DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
                0xDB, /*preview_size=*/2, &room),
            floor_two_key);
  EXPECT_EQ(DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
                0x34, /*preview_size=*/2, &room),
            unrelated_key);

  room.set_floor2(7);
  EXPECT_NE(DungeonObjectSelectorTestAccess::MakeLayoutCacheKey(
                0xDB, /*preview_size=*/2, &room),
            floor_two_key);
}

TEST(DungeonObjectSelectorPaletteTest, ObjectPreviewsDefaultOn) {
  // ZScream/Hyrule Magic muscle memory expects the selector to show visual
  // object previews first. The renderer now culls off-screen grid entries, so
  // the safer default is to render thumbnails and fall back per item when room
  // graphics are unavailable.
  DungeonObjectSelector selector;
  EXPECT_TRUE(selector.object_previews_enabled_for_testing());
}

TEST(DungeonObjectSelectorCustomEditorTest,
     NewCustomObjectSessionOpensWorkspaceWindow) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);
  selector.SetTileEditorPanel(&panel);
  int open_window_count = 0;
  selector.SetOpenTileEditorWindowCallback([&open_window_count]() {
    ++open_window_count;
    return true;
  });

  const absl::Status status =
      DungeonObjectSelectorTestAccess::OpenNewCustomObjectEditor(
          selector, /*width=*/2, /*height=*/2, "custom_31_00.bin",
          /*object_id=*/0x31, /*room_id=*/0);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(open_window_count, 1);
  EXPECT_TRUE(panel.IsOpen());
}

TEST(DungeonObjectSelectorCustomEditorTest,
     NewCustomObjectSessionFailsClosedWhenWindowIsUnavailable) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);
  selector.SetTileEditorPanel(&panel);
  selector.SetOpenTileEditorWindowCallback([]() { return false; });

  const absl::Status status =
      DungeonObjectSelectorTestAccess::OpenNewCustomObjectEditor(
          selector, /*width=*/2, /*height=*/2, "custom_31_00.bin",
          /*object_id=*/0x31, /*room_id=*/0);

  EXPECT_TRUE(absl::IsNotFound(status));
  EXPECT_FALSE(panel.IsOpen());
}

TEST(DungeonObjectSelectorCustomEditorTest,
     NewCustomObjectSessionPreservesExistingUnappliedEdits) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForNewObject(/*width=*/2, /*height=*/2, "existing.bin",
                                    /*object_id=*/0x31,
                                    /*room_id=*/0, &rooms)
                  .ok());
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);
  selector.SetTileEditorPanel(&panel);
  int open_window_count = 0;
  selector.SetOpenTileEditorWindowCallback([&open_window_count]() {
    ++open_window_count;
    return true;
  });

  const absl::Status status =
      DungeonObjectSelectorTestAccess::OpenNewCustomObjectEditor(
          selector, /*width=*/1, /*height=*/1, "replacement.bin",
          /*object_id=*/0x32, /*room_id=*/1);

  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_EQ(open_window_count, 1);
  EXPECT_TRUE(panel.IsOpen());
}

TEST(DungeonObjectSelectorSizeTest,
     ProgrammaticSelectionUsesPersistedSizeSemantics) {
  DungeonObjectSelector selector;

  selector.SelectObject(0x01);
  EXPECT_EQ(selector.GetPreviewObject().size(), 2);

  selector.SelectObject(0x02, -2);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x02);
  EXPECT_EQ(selector.GetPreviewObject().size(), 2);

  selector.SelectObject(0x100);
  EXPECT_EQ(selector.GetPreviewObject().size(), 0);

  selector.SelectObject(0xF99);
  EXPECT_EQ(selector.GetPreviewObject().size(), 0x06);
}

TEST(DungeonObjectSelectorSizeTest, CanonicalHelpersRespectCodecBoundaries) {
  EXPECT_TRUE(zelda3::IsRoomObjectSizeEditable(0x000));
  EXPECT_TRUE(zelda3::IsRoomObjectSizeEditable(0x0F7));
  EXPECT_FALSE(zelda3::IsRoomObjectSizeEditable(0x0F8));
  EXPECT_FALSE(zelda3::IsRoomObjectSizeEditable(0x100));

  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0x0F7, 0xFF), 0x0F);
  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0x100, 0x0A), 0);
  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0x13F, 0x0A), 0);
  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0xF80, 0), 0);
  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0xF99, 0), 0x06);
  EXPECT_EQ(zelda3::CanonicalRoomObjectSize(0xFFF, 0), 0x0F);

  EXPECT_EQ(zelda3::DefaultRoomObjectSizeForPlacement(0x0F7), 2);
  EXPECT_EQ(zelda3::DefaultRoomObjectSizeForPlacement(0x100), 0);
  EXPECT_EQ(zelda3::DefaultRoomObjectSizeForPlacement(0xF99), 0x06);
}

TEST(DungeonObjectSelectorSizeTest,
     UnpersistableOracleSubtypeLeavesSelectionAndPreviewUnchanged) {
  DungeonObjectSelector selector;
  int callback_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++callback_count; });

  selector.SelectObject(0x31, 0x0C);
  ASSERT_TRUE(selector.IsObjectLoaded());
  ASSERT_EQ(selector.selected_object_id_for_testing(), 0x31);
  ASSERT_EQ(selector.GetPreviewObject().id_, 0x31);
  EXPECT_EQ(selector.GetPreviewObject().size(), 0x0C);
  EXPECT_EQ(callback_count, 1);

  selector.SelectObject(0x32, 0x12);

  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x31);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x31);
  EXPECT_EQ(selector.GetPreviewObject().size(), 0x0C);
  EXPECT_EQ(callback_count, 1);
}

TEST(DungeonObjectSelectorSizeTest,
     ChestCategoryUsesRepresentableType3ObjectIds) {
  DungeonObjectSelector selector;

  for (int object_id : {0xF99, 0xF9A, 0xFB1, 0xFB2, 0xFF5}) {
    EXPECT_TRUE(DungeonObjectSelector::IsRepresentableChestObjectId(object_id));
    EXPECT_TRUE(selector.matches_object_filter_for_testing(object_id, 3));
    EXPECT_EQ(selector.object_type_symbol_for_testing(object_id), "C");
  }

  for (int object_id : {0xF9, 0xFA, 0xF98, 0xFB0}) {
    EXPECT_FALSE(
        DungeonObjectSelector::IsRepresentableChestObjectId(object_id));
    EXPECT_FALSE(selector.matches_object_filter_for_testing(object_id, 3));
    EXPECT_NE(selector.object_type_symbol_for_testing(object_id), "C");
  }
}

}  // namespace
}  // namespace editor
}  // namespace yaze

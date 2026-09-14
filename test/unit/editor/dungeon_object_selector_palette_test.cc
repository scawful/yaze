#include "app/editor/dungeon/dungeon_object_selector.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "core/features.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_layer_manager.h"

namespace yaze {
namespace editor {

struct DungeonObjectSelectorTestAccess {
  static absl::Status OpenExistingCustomObjectEditor(
      DungeonObjectSelector& selector, int16_t object_id, int subtype,
      int room_id) {
    return selector.OpenExistingCustomObjectEditor(object_id, subtype, room_id);
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

  static void GetOrCreatePreview(DungeonObjectSelector& selector,
                                 const zelda3::RoomObject& object,
                                 gfx::BackgroundBuffer** preview) {
    selector.GetOrCreatePreview(object, preview);
  }

  static void SynchronizeCustomObjectGeneration(
      DungeonObjectSelector& selector) {
    selector.SynchronizeCustomObjectGeneration();
  }

  static absl::Status GetCustomObjectAssetStatus(
      DungeonObjectSelector& selector, int object_id, int subtype) {
    return selector.GetCustomObjectAssetStatus(object_id, subtype);
  }

  static size_t CustomAssetStatusCacheSize(
      const DungeonObjectSelector& selector) {
    return selector.custom_asset_status_cache_.size();
  }
};

namespace {

void StoreRomWord(std::vector<uint8_t>* data, uint32_t address, uint16_t word) {
  (*data)[address] = static_cast<uint8_t>(word & 0xFF);
  (*data)[address + 1] = static_cast<uint8_t>(word >> 8);
}

size_t ActiveArenaSurfaceCount() {
  const auto& arena = gfx::Arena::Get();
  return arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
}

class ScopedSelectorCustomObjectState {
 public:
  ScopedSelectorCustomObjectState()
      : previous_(zelda3::CustomObjectManager::Get().SnapshotState()),
        previous_enabled_(core::FeatureFlags::get().kEnableCustomObjects) {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("yaze_selector_custom_object_" + std::to_string(nonce));
    std::filesystem::create_directories(path_);
    zelda3::CustomObjectManager::Get().Initialize(path_.string());
    zelda3::CustomObjectManager::Get().ClearObjectFileMap();
    core::FeatureFlags::get().kEnableCustomObjects = true;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    const std::vector<uint8_t> bytes = {
        0x01, 0x00,  // count=1, jump=0
        0x10, 0x28,  // tile word
        0x00, 0x00,
    };
    std::ofstream output(path_ / "track_LR.bin", std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }

  ~ScopedSelectorCustomObjectState() {
    core::FeatureFlags::get().kEnableCustomObjects = previous_enabled_;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    zelda3::CustomObjectManager::Get().RestoreState(previous_);
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  bool WriteRawAsset(const std::string& filename,
                     const std::vector<uint8_t>& bytes) const {
    std::ofstream output(path_ / filename, std::ios::binary);
    if (!output.is_open()) {
      return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  zelda3::CustomObjectManager::State previous_;
  bool previous_enabled_;
  std::filesystem::path path_;
};

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
     PreviewBitmapUsesCanonicalDungeonCgramRows) {
  std::vector<uint8_t> rom_data(0x200000, 0);
  StoreRomWord(&rom_data, /*object 0x11F descriptor=*/0x842E, 0x0E9A);
  for (uint32_t address = 0x29EC; address < 0x29F4; address += 2) {
    // Tile 0x1EE, palette row 2. The preview graphics below use source color
    // 9, so the rendered SDL index must be 2 * 16 + 9 = 41.
    StoreRomWord(&rom_data, address, 0x09EE);
  }

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::move(rom_data)).ok());
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0];
  room.SetLoaded(true);
  auto& room_gfx =
      const_cast<std::array<uint8_t, 0x10000>&>(room.get_gfx_buffer());
  room_gfx.fill(9);

  gfx::SnesPalette hud_palette;
  for (int color = 0; color < 32; ++color) {
    hud_palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(0x0100 + color)));
  }
  gfx::SnesPalette dungeon_palette;
  for (int color = 0; color < 90; ++color) {
    dungeon_palette.AddColor(
        gfx::SnesColor(static_cast<uint16_t>(0x0200 + color)));
  }

  zelda3::GameData game_data;
  game_data.palette_groups.hud.AddPalette(hud_palette);
  DungeonObjectSelector selector(&rom);
  selector.SetGameData(&game_data);
  selector.set_rooms(&rooms);
  selector.set_current_room_id(0);
  selector.SetCurrentPaletteGroup(
      zelda3::BuildDungeonRenderPaletteGroupFromGameData(dungeon_palette,
                                                         &game_data));

  zelda3::RoomObject object(/*object_id=*/0x11F, /*x=*/0, /*y=*/0,
                            /*size=*/0, /*layer=*/0);
  gfx::BackgroundBuffer* preview = nullptr;
  DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, object,
                                                      &preview);

  ASSERT_NE(preview, nullptr);
  auto& bitmap = preview->bitmap();
  ASSERT_EQ(bitmap.palette().size(), 128u);
  EXPECT_EQ(bitmap.palette()[1].snes(), hud_palette[1].snes());
  EXPECT_EQ(bitmap.palette()[41].snes(), dungeon_palette[8].snes());
  EXPECT_NE(
      std::find(bitmap.mutable_data().begin(), bitmap.mutable_data().end(), 41),
      bitmap.mutable_data().end());
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

TEST(DungeonObjectSelectorLayoutTest,
     GridBalancesRemainingWidthWithoutChangingDensity) {
  constexpr float kAvailableWidth = 400.0f;
  constexpr float kPreferredItemSize = 60.0f;
  constexpr float kItemSpacing = 4.0f;

  const auto layout = ResolveDungeonObjectSelectorGridLayout(
      kAvailableWidth, kPreferredItemSize, kItemSpacing);

  EXPECT_EQ(layout.columns, 6);
  EXPECT_FLOAT_EQ(layout.item_size, kPreferredItemSize);
  const float occupied_width =
      layout.columns * layout.item_size + (layout.columns - 1) * kItemSpacing;
  EXPECT_NEAR(layout.leading_inset * 2.0f + occupied_width, kAvailableWidth,
              0.001f);
}

TEST(DungeonObjectSelectorLayoutTest,
     GridShrinksToOneUsableItemWhenSelectorIsExtremelyNarrow) {
  const auto layout = ResolveDungeonObjectSelectorGridLayout(
      /*available_width=*/28.0f, /*preferred_item_size=*/72.0f,
      /*item_spacing=*/4.0f);

  EXPECT_EQ(layout.columns, 1);
  EXPECT_FLOAT_EQ(layout.item_size, 28.0f);
  EXPECT_FLOAT_EQ(layout.leading_inset, 0.0f);
}

TEST(DungeonObjectSelectorLayoutTest,
     GridHonorsMinimumItemSizeWhenThereIsRoom) {
  const auto layout = ResolveDungeonObjectSelectorGridLayout(
      /*available_width=*/100.0f, /*preferred_item_size=*/10.0f,
      /*item_spacing=*/4.0f, /*min_item_size=*/32.0f);

  EXPECT_EQ(layout.columns, 2);
  EXPECT_FLOAT_EQ(layout.item_size, 32.0f);
  EXPECT_FLOAT_EQ(layout.leading_inset, 16.0f);
}

TEST(DungeonObjectSelectorLayoutTest,
     GridPreservesDensityAcrossResizeBreakpoints) {
  constexpr float kPreferredItemSize = 54.0f;
  constexpr float kItemSpacing = 4.0f;

  for (const float available_width :
       std::array{28.0f, 80.0f, 170.0f, 285.0f, 286.0f, 400.0f}) {
    SCOPED_TRACE(available_width);
    const auto layout = ResolveDungeonObjectSelectorGridLayout(
        available_width, kPreferredItemSize, kItemSpacing);
    const float occupied_width =
        layout.columns * layout.item_size + (layout.columns - 1) * kItemSpacing;

    EXPECT_GE(layout.columns, 1);
    EXPECT_GT(layout.item_size, 0.0f);
    EXPECT_NEAR(layout.leading_inset * 2.0f + occupied_width, available_width,
                0.001f);
    if (available_width >= kPreferredItemSize) {
      EXPECT_FLOAT_EQ(layout.item_size, kPreferredItemSize);
    }
  }
}

TEST(DungeonObjectSelectorLayoutTest,
     DensityOptionsRemainDistinctAtTheSamePanelWidth) {
  constexpr float kAvailableWidth = 400.0f;
  constexpr float kItemSpacing = 4.0f;

  const auto compact = ResolveDungeonObjectSelectorGridLayout(
      kAvailableWidth, /*preferred_item_size=*/54.0f, kItemSpacing);
  const auto medium = ResolveDungeonObjectSelectorGridLayout(
      kAvailableWidth, /*preferred_item_size=*/60.0f, kItemSpacing);
  const auto large = ResolveDungeonObjectSelectorGridLayout(
      kAvailableWidth, /*preferred_item_size=*/76.0f, kItemSpacing);

  EXPECT_FLOAT_EQ(compact.item_size, 54.0f);
  EXPECT_FLOAT_EQ(medium.item_size, 60.0f);
  EXPECT_FLOAT_EQ(large.item_size, 76.0f);
  EXPECT_NE(compact.item_size, medium.item_size);
  EXPECT_NE(medium.item_size, large.item_size);
  EXPECT_EQ(large.columns, 5);
  EXPECT_FLOAT_EQ(large.leading_inset, 2.0f);
}

TEST(DungeonObjectSelectorStreamFilterTest, AllAndUnknownFiltersFailOpen) {
  for (int object_id : {-1, 0x000, 0x0F8, 0x140, 0xF80, 0x1000}) {
    EXPECT_TRUE(MatchesDungeonObjectStreamFilter(object_id, 0));
    EXPECT_TRUE(MatchesDungeonObjectStreamFilter(object_id, 99));
  }
}

TEST(DungeonObjectSelectorStreamFilterTest, TypeOneUsesCanonicalCodecRange) {
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(-1, 1));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0x000, 1));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0x0F7, 1));
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0x0F8, 1));
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0x100, 1));
}

TEST(DungeonObjectSelectorStreamFilterTest, TypeTwoUsesCanonicalCodecRange) {
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0x0FF, 2));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0x100, 2));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0x13F, 2));
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0x140, 2));
}

TEST(DungeonObjectSelectorStreamFilterTest, TypeThreeUsesCanonicalCodecRange) {
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0xF7F, 3));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0xF80, 3));
  EXPECT_TRUE(MatchesDungeonObjectStreamFilter(0xFFF, 3));
  EXPECT_FALSE(MatchesDungeonObjectStreamFilter(0x1000, 3));
}

TEST(DungeonObjectSelectorPreviewFitTest,
     FitsWideSourceAndCentersItVertically) {
  const auto fit = ResolveDungeonObjectPreviewFit(
      /*source_width=*/32.0f, /*source_height=*/8.0f,
      /*box_width=*/40.0f, /*box_height=*/40.0f);

  EXPECT_TRUE(fit.valid);
  EXPECT_FLOAT_EQ(fit.x, 0.0f);
  EXPECT_FLOAT_EQ(fit.y, 15.0f);
  EXPECT_FLOAT_EQ(fit.width, 40.0f);
  EXPECT_FLOAT_EQ(fit.height, 10.0f);
}

TEST(DungeonObjectSelectorPreviewFitTest,
     FitsTallSourceAndCentersItHorizontally) {
  const auto fit = ResolveDungeonObjectPreviewFit(
      /*source_width=*/8.0f, /*source_height=*/32.0f,
      /*box_width=*/40.0f, /*box_height=*/40.0f);

  EXPECT_TRUE(fit.valid);
  EXPECT_FLOAT_EQ(fit.x, 15.0f);
  EXPECT_FLOAT_EQ(fit.y, 0.0f);
  EXPECT_FLOAT_EQ(fit.width, 10.0f);
  EXPECT_FLOAT_EQ(fit.height, 40.0f);
}

TEST(DungeonObjectSelectorPreviewFitTest, SquareSourceFillsSquareBox) {
  const auto fit = ResolveDungeonObjectPreviewFit(
      /*source_width=*/16.0f, /*source_height=*/16.0f,
      /*box_width=*/40.0f, /*box_height=*/40.0f);

  EXPECT_TRUE(fit.valid);
  EXPECT_FLOAT_EQ(fit.x, 0.0f);
  EXPECT_FLOAT_EQ(fit.y, 0.0f);
  EXPECT_FLOAT_EQ(fit.width, 40.0f);
  EXPECT_FLOAT_EQ(fit.height, 40.0f);
}

TEST(DungeonObjectSelectorPreviewFitTest,
     NonPositiveSourceOrBoxDimensionsAreInvalid) {
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(0.0f, 8.0f, 40.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, 0.0f, 40.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(-8.0f, 8.0f, 40.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, -8.0f, 40.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, 8.0f, 0.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, 8.0f, 40.0f, 0.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, 8.0f, -40.0f, 40.0f).valid);
  EXPECT_FALSE(ResolveDungeonObjectPreviewFit(8.0f, 8.0f, 40.0f, -40.0f).valid);
}

TEST(DungeonObjectSelectorCustomEditorTest,
     ExistingCustomObjectSessionOpensWorkspaceWindow) {
  ScopedSelectorCustomObjectState custom_state;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
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
      DungeonObjectSelectorTestAccess::OpenExistingCustomObjectEditor(
          selector, /*object_id=*/0x31, /*subtype=*/0, /*room_id=*/0);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(open_window_count, 1);
  EXPECT_TRUE(panel.IsOpen());
}

TEST(DungeonObjectSelectorCustomEditorTest,
     ExistingCustomObjectSessionFailsClosedWhenWindowIsUnavailable) {
  ScopedSelectorCustomObjectState custom_state;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);
  selector.SetTileEditorPanel(&panel);
  selector.SetOpenTileEditorWindowCallback([]() { return false; });

  const absl::Status status =
      DungeonObjectSelectorTestAccess::OpenExistingCustomObjectEditor(
          selector, /*object_id=*/0x31, /*subtype=*/0, /*room_id=*/0);

  EXPECT_TRUE(absl::IsNotFound(status));
  EXPECT_FALSE(panel.IsOpen());
}

TEST(DungeonObjectSelectorCustomEditorTest,
     OutOfRangeRuntimeSlotDoesNotOpenWorkspaceWindow) {
  ScopedSelectorCustomObjectState custom_state;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[1].SetLoaded(true);
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
      DungeonObjectSelectorTestAccess::OpenExistingCustomObjectEditor(
          selector, /*object_id=*/0x32, /*subtype=*/3, /*room_id=*/1);

  EXPECT_TRUE(absl::IsOutOfRange(status));
  EXPECT_EQ(open_window_count, 0);
  EXPECT_FALSE(panel.IsOpen());
}

TEST(DungeonObjectSelectorCustomRuntimeTest, FixedSlotsMatchOracleDispatch) {
  for (int subtype = 0; subtype < 16; ++subtype) {
    EXPECT_TRUE(IsDungeonCustomObjectRuntimeSlot(0x31, subtype));
  }
  EXPECT_FALSE(IsDungeonCustomObjectRuntimeSlot(0x31, 16));
  for (int subtype = 0; subtype < 3; ++subtype) {
    EXPECT_TRUE(IsDungeonCustomObjectRuntimeSlot(0x32, subtype));
  }
  EXPECT_FALSE(IsDungeonCustomObjectRuntimeSlot(0x32, 3));
  EXPECT_FALSE(IsDungeonCustomObjectRuntimeSlot(0x30, 0));

  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x31, 0), "Track horizontal");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x31, 13),
            "Sword House wall override");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x32, 2), "Ice chair");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x32, 3),
            "Unknown custom runtime slot");
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     MinecartManagementIsSeparateFromGraphicsOnlySlots) {
  EXPECT_TRUE(IsMinecartGraphicsRuntimeSlot(0x31, 0));
  EXPECT_TRUE(IsMinecartGraphicsRuntimeSlot(0x31, 12));
  EXPECT_TRUE(IsMinecartGraphicsRuntimeSlot(0x31, 14));
  EXPECT_FALSE(IsMinecartGraphicsRuntimeSlot(0x31, 13));
  EXPECT_FALSE(IsMinecartGraphicsRuntimeSlot(0x31, 15));
  EXPECT_FALSE(IsMinecartGraphicsRuntimeSlot(0x32, 0));
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
     UnsupportedOracleRuntimeSubtypeLeavesSelectionAndPreviewUnchanged) {
  DungeonObjectSelector selector;
  int callback_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++callback_count; });

  selector.SelectObject(0x01);
  ASSERT_TRUE(selector.IsObjectLoaded());
  ASSERT_EQ(selector.selected_object_id_for_testing(), 0x01);
  ASSERT_EQ(selector.GetPreviewObject().id_, 0x01);
  EXPECT_EQ(selector.GetPreviewObject().size(), 2);
  EXPECT_EQ(callback_count, 1);

  selector.SelectObject(0x32, 0x03);

  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x01);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x01);
  EXPECT_EQ(selector.GetPreviewObject().size(), 2);
  EXPECT_EQ(callback_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     EnabledCustomFamilyRequiresExplicitReadySubtype) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int callback_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++callback_count; });
  selector.SelectObject(0x01);
  ASSERT_EQ(callback_count, 1);

  selector.SelectObject(0x31);
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x01);
  EXPECT_EQ(callback_count, 1);

  selector.SelectObject(0x31, 0);
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x31);
  EXPECT_EQ(selector.GetPreviewObject().size(), 0);
  EXPECT_EQ(callback_count, 2);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     DisabledFeatureRejectsExplicitCustomSubtypeWithoutChangingSelection) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int callback_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++callback_count; });
  selector.SelectObject(0x01);
  ASSERT_EQ(callback_count, 1);

  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  selector.SelectObject(0x31, 0);

  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x01);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x01);
  EXPECT_EQ(callback_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     MissingAndMalformedAssetsLeaveSelectionAndCallbackUnchanged) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int callback_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++callback_count; });
  selector.SelectObject(0x01);
  ASSERT_EQ(callback_count, 1);

  selector.SelectObject(0x31, 1);
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x01);
  EXPECT_EQ(callback_count, 1);

  ASSERT_TRUE(
      custom_state.WriteRawAsset("track_UD.bin", {0x01, 0x00, 0x34, 0x12}));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);
  selector.SelectObject(0x31, 1);

  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x01);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x01);
  EXPECT_EQ(callback_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     MappingGenerationChangeClearsCachesAndMarksRoomsDirty) {
  ScopedSelectorCustomObjectState custom_state;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0];
  room.SetLoaded(true);
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);

  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);
  zelda3::RoomLayerManager layer_manager;
  (void)room.GetCompositeBitmap(layer_manager);
  ASSERT_FALSE(room.IsCompositeDirty());
  DungeonObjectSelectorTestAccess::SeedPreviewCache(selector, 0x1234);
  ASSERT_TRUE(DungeonObjectSelectorTestAccess::GetCustomObjectAssetStatus(
                  selector, 0x31, 0)
                  .ok());
  ASSERT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 1u);
  ASSERT_EQ(
      DungeonObjectSelectorTestAccess::CustomAssetStatusCacheSize(selector),
      1u);
  const size_t invalidations_before =
      selector.preview_cache_invalidations_for_testing();

  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"track_LR.bin"}}});
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(),
            invalidations_before + 1);
  EXPECT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 0u);
  EXPECT_EQ(
      DungeonObjectSelectorTestAccess::CustomAssetStatusCacheSize(selector),
      0u);
  EXPECT_TRUE(room.IsCompositeDirty());
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedCustomPlacementRefreshesAfterValidAssetReload) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });

  selector.SelectObject(0x31, 0);
  ASSERT_TRUE(selector.IsObjectLoaded());
  ASSERT_EQ(selected_count, 1);

  ASSERT_TRUE(custom_state.WriteRawAsset(
      "track_LR.bin", {0x02, 0x00, 0x10, 0x28, 0x11, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_TRUE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x31);
  EXPECT_EQ(selected_count, 2);
  EXPECT_EQ(invalidated_count, 0);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedTrackCornerAliasRefreshesAfterAssetReload) {
  ScopedSelectorCustomObjectState custom_state;
  ASSERT_TRUE(custom_state.WriteRawAsset("track_corner_TL.bin",
                                         {0x01, 0x00, 0x10, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"track_LR.bin", "track_UD.bin", "track_corner_TL.bin"}}});

  DungeonObjectSelector selector;
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });

  selector.SelectObject(0x100);
  ASSERT_TRUE(selector.IsObjectLoaded());
  ASSERT_EQ(selected_count, 1);

  ASSERT_TRUE(custom_state.WriteRawAsset(
      "track_corner_TL.bin", {0x02, 0x00, 0x10, 0x28, 0x11, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_TRUE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x100);
  EXPECT_EQ(selector.GetPreviewObject().id_, 0x100);
  EXPECT_EQ(selected_count, 2);
  EXPECT_EQ(invalidated_count, 0);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedCustomPlacementCancelsAfterAssetRemoval) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x31, 0);
  ASSERT_TRUE(selector.IsObjectLoaded());

  ASSERT_TRUE(std::filesystem::remove(custom_state.path() / "track_LR.bin"));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_FALSE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), -1);
  EXPECT_EQ(selected_count, 1);
  EXPECT_EQ(invalidated_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedCustomPlacementCancelsAfterSlotRemap) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int invalidated_count = 0;
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x31, 0);
  ASSERT_TRUE(selector.IsObjectLoaded());

  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"missing-remap.bin"}}});
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_FALSE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), -1);
  EXPECT_EQ(invalidated_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedCustomPlacementCancelsWhenFeatureIsDisabled) {
  ScopedSelectorCustomObjectState custom_state;
  DungeonObjectSelector selector;
  int invalidated_count = 0;
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x31, 0);
  ASSERT_TRUE(selector.IsObjectLoaded());

  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_FALSE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), -1);
  EXPECT_EQ(invalidated_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedVanillaFamilyPlacementCancelsWhenFeatureIsEnabled) {
  ScopedSelectorCustomObjectState custom_state;
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  DungeonObjectSelector selector;
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x31);
  ASSERT_TRUE(selector.IsObjectLoaded());

  core::FeatureFlags::get().kEnableCustomObjects = true;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_FALSE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), -1);
  EXPECT_EQ(selected_count, 1);
  EXPECT_EQ(invalidated_count, 1);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedVanillaFamilyPlacementSurvivesAssetReloadWhileFeatureStaysDisabled) {
  ScopedSelectorCustomObjectState custom_state;
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  DungeonObjectSelector selector;
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x31);
  ASSERT_TRUE(selector.IsObjectLoaded());

  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_TRUE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x31);
  EXPECT_EQ(selected_count, 1);
  EXPECT_EQ(invalidated_count, 0);
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

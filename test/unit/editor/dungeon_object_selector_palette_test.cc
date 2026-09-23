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
#include "imgui/imgui_internal.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/room_layer_manager.h"

namespace yaze {
namespace editor {

struct DungeonObjectSelectorTestAccess {
  static zelda3::RoomObject MakePreviewObject(
      const DungeonObjectSelector& selector, int object_id) {
    return selector.MakePreviewObject(object_id);
  }

  static void ShowCustomAssets(DungeonObjectSelector& selector) {
    selector.browser_mode_ = DungeonObjectSelector::BrowserMode::kCustomAssets;
  }

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
     PreviewCardConstructionLeavesRomTilesLazy) {
  std::vector<uint8_t> rom_data(0x200000, 0);
  StoreRomWord(&rom_data, 0x842E, 0x0E9A);
  for (uint32_t address = 0x29EC; address < 0x29F4; address += 2) {
    StoreRomWord(&rom_data, address, 0x09EE);
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::move(rom_data)).ok());
  DungeonObjectSelector selector(&rom);

  auto object =
      DungeonObjectSelectorTestAccess::MakePreviewObject(selector, 0x11F);
  EXPECT_EQ(object.id_, 0x11F);
  EXPECT_EQ(object.size_, zelda3::DefaultRoomObjectSizeForPlacement(0x11F));
  EXPECT_EQ(object.rom(), &rom);
  EXPECT_TRUE(object.mutable_tiles().empty())
      << "Constructing a card must not parse ROM tiles before a cache lookup";

  object.EnsureTilesLoaded();
  ASSERT_EQ(object.mutable_tiles().size(), 4u)
      << "The fixture must support a real load, not pass because ROM data is "
         "absent";
  for (const auto& tile : object.mutable_tiles()) {
    EXPECT_EQ(tile.id_, 0x1EE);
    EXPECT_EQ(tile.palette_, 2);
  }
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonObjectSelectorPaletteTest,
     LazyPreviewCardsReuseCorrectCachedBitmap) {
  std::vector<uint8_t> rom_data(0x200000, 0);
  StoreRomWord(&rom_data, 0x842E, 0x0E9A);
  for (uint32_t address = 0x29EC; address < 0x29F4; address += 2) {
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
  DungeonObjectSelector selector(&rom);
  selector.set_rooms(&rooms);
  selector.set_current_room_id(0);

  auto first_card =
      DungeonObjectSelectorTestAccess::MakePreviewObject(selector, 0x11F);
  gfx::BackgroundBuffer* first_preview = nullptr;
  DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, first_card,
                                                      &first_preview);
  ASSERT_NE(first_preview, nullptr);
  const auto expected_pixels = first_preview->bitmap().mutable_data();
  EXPECT_NE(std::find(expected_pixels.begin(), expected_pixels.end(), 41),
            expected_pixels.end());
  EXPECT_TRUE(first_card.mutable_tiles().empty());
  const auto invalidations = selector.preview_cache_invalidations_for_testing();

  for (int frame = 0; frame < 3; ++frame) {
    auto card =
        DungeonObjectSelectorTestAccess::MakePreviewObject(selector, 0x11F);
    EXPECT_TRUE(card.mutable_tiles().empty());
    gfx::BackgroundBuffer* cached_preview = nullptr;
    DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, card,
                                                        &cached_preview);
    ASSERT_EQ(cached_preview, first_preview);
    EXPECT_EQ(cached_preview->bitmap().mutable_data(), expected_pixels);
    EXPECT_TRUE(card.mutable_tiles().empty());
    EXPECT_EQ(DungeonObjectSelectorTestAccess::PreviewCacheSize(selector), 1u);
    EXPECT_EQ(selector.preview_cache_invalidations_for_testing(),
              invalidations);
  }
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonObjectSelectorPaletteTest,
     ReloadAssetsButtonWorksWithCustomObjectsDisabled) {
  ScopedSelectorCustomObjectState custom_state;
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  struct ScopedContext {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    ScopedContext() { ImGui::SetCurrentContext(context); }
    ~ScopedContext() {
      ImGui::DestroyContext(context);
      ImGui::SetCurrentContext(previous);
    }
  } imgui_context;
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(800, 600);
  io.DeltaTime = 1.0f / 60.0f;
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  DungeonObjectSelector selector;
  DungeonObjectSelectorTestAccess::ShowCustomAssets(selector);
  bool found_reload_action = false;
  ImVec2 reload_center;
  auto frame = [&]() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(30, 30));
    ImGui::SetNextWindowSize(ImVec2(640, 400));
    ImGui::Begin(
        "SelectorAssetRefreshTest", nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    const ImGuiID toolbar_id = ImGui::GetID("##CustomObjectToolbar");
    selector.DrawObjectAssetBrowser();
    if (const ImGuiTable* toolbar = ImGui::TableFindByID(toolbar_id)) {
      found_reload_action = toolbar->ColumnsCount == 2;
      if (found_reload_action) {
        const auto& action_column = toolbar->Columns[1];
        reload_center =
            ImVec2((action_column.WorkMinX + action_column.WorkMaxX) * 0.5f,
                   (toolbar->RowPosY1 + toolbar->RowPosY2) * 0.5f);
      }
    }
    ImGui::End();
    ImGui::Render();
  };
  frame();
  ASSERT_TRUE(found_reload_action)
      << "Disabling custom-object rendering must not hide asset refresh";
  auto& manager = zelda3::CustomObjectManager::Get();
  const uint64_t generation = manager.asset_generation();
  const auto invalidations = selector.preview_cache_invalidations_for_testing();

  io.AddMousePosEvent(reload_center.x, reload_center.y);
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frame();

  EXPECT_NE(manager.asset_generation(), generation)
      << "Clicking the visible Reload Assets button must request a refresh";
  EXPECT_GT(selector.preview_cache_invalidations_for_testing(), invalidations);
  EXPECT_FALSE(core::FeatureFlags::get().kEnableCustomObjects);
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
     GraphicsReloadRefreshesPixelsWithoutChangingRoomHeader) {
  for (int refresh_mode : {0, 1, 2}) {
    SCOPED_TRACE(refresh_mode);
    std::vector<uint8_t> rom_data(0x200000, 0);
    StoreRomWord(&rom_data, 0x842E, 0x0E9A);
    for (uint32_t address = 0x29EC; address < 0x29F4; address += 2) {
      StoreRomWord(&rom_data, address, 0x09C0);
    }
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(std::move(rom_data)).ok());
    zelda3::GameData game_data;
    game_data.graphics_buffer.assign(223 * 4096, 1);
    DungeonRoomStore rooms(&rom);
    auto& room = rooms[0];
    room.SetLoaded(true);
    room.SetGameData(&game_data);
    room.LoadRoomGraphics();
    room.CopyRoomGraphicsToBuffer();
    const auto original_blocks = room.blocks();

    DungeonObjectSelector selector(&rom);
    selector.SetGameData(&game_data);
    selector.set_rooms(&rooms);
    selector.set_current_room_id(0);
    zelda3::RoomObject object(0x11F, 0, 0, 0, 0);
    gfx::BackgroundBuffer* preview = nullptr;
    DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, object,
                                                        &preview);
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->bitmap().at(0), 33);
    const auto invalidations =
        selector.preview_cache_invalidations_for_testing();
    DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, object,
                                                        &preview);
    EXPECT_EQ(selector.preview_cache_invalidations_for_testing(),
              invalidations);

    // Editing an existing sheet preserves every header/cache-key value. Use
    // the real room-copy path, rather than mutating the private graphics buffer.
    std::fill(game_data.graphics_buffer.begin(),
              game_data.graphics_buffer.end(), 2);
    if (refresh_mode == 1) {
      // A moved-in room occupies the same address and has matching headers.
      // A per-instance reload count alone would collide with the cached room.
      room = zelda3::Room(0, &rom, &game_data);
      room.SetLoaded(true);
      room.LoadRoomGraphics();
      room.CopyRoomGraphicsToBuffer();
    } else if (refresh_mode == 2) {
      // This copies the common animated frame, then exits on the intentionally
      // invalid selected-sheet table. The changed frame still needs publishing.
      room.LoadAnimatedGraphics();
    } else {
      room.CopyRoomGraphicsToBuffer();
    }
    ASSERT_EQ(room.blocks(), original_blocks);
    DungeonObjectSelectorTestAccess::GetOrCreatePreview(selector, object,
                                                        &preview);
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->bitmap().at(0), 34);
    EXPECT_EQ(selector.preview_cache_invalidations_for_testing(),
              invalidations + 1);
    EXPECT_FALSE(rom.dirty());
  }
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
  for (int subtype = 0; subtype < 2; ++subtype) {
    EXPECT_TRUE(IsDungeonCustomObjectRuntimeSlot(0x54, subtype));
  }
  EXPECT_FALSE(IsDungeonCustomObjectRuntimeSlot(0x54, 2));
  EXPECT_FALSE(IsDungeonCustomObjectRuntimeSlot(0x30, 0));

  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x31, 0), "Track horizontal");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x31, 13),
            "Sword House wall object");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x32, 2), "Ice chair");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x54, 0), "Kydreeok body");
  EXPECT_EQ(GetDungeonCustomObjectSlotName(0x54, 1), "Manhandla body");
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
     QueuedWallCornerDoesNotRefreshAfterTrackAssetReload) {
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
  EXPECT_EQ(selected_count, 1);
  EXPECT_EQ(invalidated_count, 0);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedExplicitWallOverrideRefreshesAfterAssetReload) {
  ScopedSelectorCustomObjectState custom_state;
  ASSERT_TRUE(custom_state.WriteRawAsset("wall_corner.bin",
                                         {0x01, 0x00, 0x10, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x100, {"wall_corner.bin"}}});

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
      "wall_corner.bin", {0x02, 0x00, 0x10, 0x28, 0x11, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_TRUE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x100);
  EXPECT_EQ(selected_count, 2);
  EXPECT_EQ(invalidated_count, 0);
}

TEST(DungeonObjectSelectorCustomRuntimeTest,
     QueuedExplicitWallOverrideRefreshesToVanillaWhenFeatureIsDisabled) {
  ScopedSelectorCustomObjectState custom_state;
  ASSERT_TRUE(custom_state.WriteRawAsset("wall_corner.bin",
                                         {0x01, 0x00, 0x10, 0x28, 0x00, 0x00}));
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x100, {"wall_corner.bin"}}});

  DungeonObjectSelector selector;
  int selected_count = 0;
  int invalidated_count = 0;
  selector.SetObjectSelectedCallback(
      [&](const zelda3::RoomObject&) { ++selected_count; });
  selector.SetPlacementInvalidatedCallback([&]() { ++invalidated_count; });
  selector.SelectObject(0x100);
  ASSERT_TRUE(selector.IsObjectLoaded());

  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  DungeonObjectSelectorTestAccess::SynchronizeCustomObjectGeneration(selector);

  EXPECT_TRUE(selector.IsObjectLoaded());
  EXPECT_EQ(selector.selected_object_id_for_testing(), 0x100);
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

TEST(DungeonObjectCategoryTest, CategoriesMatchNamedObjectFamilies) {
  struct CategoryWitness {
    int object_id;
    const char* name_fragment;
    const char* category;
  };
  // Independent witnesses from the three canonical name arrays, not the
  // selector's former guessed ID ranges. Categories describe browsing only,
  // not collision, interaction, or compositing behavior.
  for (const auto& witness : {
           CategoryWitness{0x001, "Wall", "Walls"},
           CategoryWitness{0x020, "Diagonal wall", "Walls"},
           CategoryWitness{0x061, "Wall", "Walls"},
           CategoryWitness{0x100, "Corner", "Walls"},
           CategoryWitness{0x13C, "Sanctuary wall", "Walls"},
           CategoryWitness{0xFA2, "Deep corner", "Walls"},
           CategoryWitness{0x033, "Carpet", "Floors"},
           CategoryWitness{0x049, "Floor tiles", "Floors"},
           CategoryWitness{0x079, "Water edge", "Floors"},
           CategoryWitness{0x0C4, "Floor 1", "Floors"},
           CategoryWitness{0x0C8, "Water floor", "Floors"},
           CategoryWitness{0x0D1, "Icy floor", "Floors"},
           CategoryWitness{0x0D2, "Icy floor", "Floors"},
           CategoryWitness{0x0E3, "Conveyor", "Floors"},
           CategoryWitness{0x0E7, "Heavy current water", "Floors"},
           CategoryWitness{0xFC7, "Bombable floor", "Floors"},
           CategoryWitness{0xFE6, "Pit", "Floors"},
           CategoryWitness{0xFF8, "Triforce floor", "Floors"},
           CategoryWitness{0x035, "Hole in wall", "Doors"},
           CategoryWitness{0xFF4, "Boss entrance", "Doors"},
           CategoryWitness{0xFF6, "Ganon door", "Doors"},
           CategoryWitness{0x03A, "Wall decors", "Decorations"},
           CategoryWitness{0x120, "Small torch", "Decorations"},
           CategoryWitness{0x123, "Table", "Decorations"},
           CategoryWitness{0x129, "Fireplace", "Decorations"},
           CategoryWitness{0xFD6, "Bar corner", "Decorations"},
           CategoryWitness{0xF94, "Unused", "Special"},
           CategoryWitness{0x0D3, "logic", "Special"},
           CategoryWitness{0x0F7, "Nothing", "Special"},
       }) {
    SCOPED_TRACE(witness.object_id);
    EXPECT_NE(
        zelda3::GetObjectName(witness.object_id).find(witness.name_fragment),
        std::string::npos);
    const auto actual =
        zelda3::ObjectCategories::GetObjectCategory(witness.object_id);
    ASSERT_TRUE(actual.ok()) << actual.status();
    EXPECT_EQ(*actual, witness.category);
  }
}

TEST(DungeonObjectCategoryTest,
     StairsChestsAndDoorwaysUseExactStoredObjectIds) {
  const std::vector<int> stairs = {
      0x021, 0x12D, 0x12E, 0x12F, 0x130, 0x131, 0x132, 0x133, 0x135, 0x136,
      0x138, 0x139, 0x13A, 0x13B, 0xF9B, 0xF9C, 0xF9D, 0xF9E, 0xF9F, 0xFA0,
      0xFA1, 0xFA6, 0xFA7, 0xFA8, 0xFA9, 0xFB3, 0xFB4, 0xFB5, 0xFB6};
  for (const int id : stairs) {
    const auto name = zelda3::GetObjectName(id);
    EXPECT_TRUE(name.find("stairs") != std::string::npos ||
                name.find("Ladder") != std::string::npos)
        << id << ": " << name;
  }
  for (const auto& [category, expected] :
       std::vector<std::pair<std::string, std::vector<int>>>{
           {"Stairs", stairs},
           {"Chests", {0xF99, 0xF9A, 0xFB1, 0xFB2, 0xFF5}},
           // These are room tile objects. Ordinary doors are separate Door
           // records and must not be confused with diagonal-wall IDs17..1E.
           {"Doors", {0x035, 0xFF4, 0xFF6}}}) {
    SCOPED_TRACE(category);
    auto actual = zelda3::ObjectCategories::GetObjectsInCategory(category);
    ASSERT_TRUE(actual.ok()) << actual.status();
    std::sort(actual->begin(), actual->end());
    EXPECT_EQ(*actual, expected);
  }
}

TEST(DungeonObjectCategoryTest, CoversAll440RepresentableIdsExactlyOnce) {
  std::array<int, 0x1000> counts{};
  size_t entry_count = 0;
  for (const auto& category : zelda3::ObjectCategories::GetObjectCategories()) {
    SCOPED_TRACE(category.name);
    for (const int id : category.object_ids) {
      SCOPED_TRACE(id);
      const bool representable = (id >= 0 && id <= 0xF7) ||
                                 (id >= 0x100 && id <= 0x13F) ||
                                 (id >= 0xF80 && id <= 0xFFF);
      EXPECT_TRUE(representable);
      if (id >= 0 && id < static_cast<int>(counts.size())) {
        ++counts[id];
      }
      ++entry_count;
    }
  }
  EXPECT_EQ(entry_count, 440u);
  for (int id = 0; id < static_cast<int>(counts.size()); ++id) {
    const bool representable =
        id <= 0xF7 || (id >= 0x100 && id <= 0x13F) || id >= 0xF80;
    EXPECT_EQ(counts[id], representable ? 1 : 0) << "object " << id;
  }
  for (const int invalid : {-1, 0xF8, 0xF9, 0xFA, 0x140, 0x200, 0x1000}) {
    EXPECT_FALSE(zelda3::ObjectCategories::GetObjectCategory(invalid).ok())
        << invalid;
  }
}

TEST(DungeonObjectCategoryTest, SelectorFiltersUseSharedCategories) {
  DungeonObjectSelector selector;
  constexpr std::array<const char*, 7> filters = {
      "All", "Walls", "Floors", "Chests", "Doors", "Decorations", "Stairs"};
  for (const auto& category : zelda3::ObjectCategories::GetObjectCategories()) {
    SCOPED_TRACE(category.name);
    for (const int id : category.object_ids) {
      SCOPED_TRACE(id);
      EXPECT_TRUE(selector.matches_object_filter_for_testing(id, 0));
      for (int filter = 1; filter < static_cast<int>(filters.size());
           ++filter) {
        EXPECT_EQ(selector.matches_object_filter_for_testing(id, filter),
                  category.name == filters[filter])
            << filters[filter];
      }
    }
  }
}

TEST(DungeonObjectCategoryTest, FallbackSymbolsDescribeActualObjectFamilies) {
  DungeonObjectSelector selector;
  for (const auto& [id, expected] :
       std::vector<std::pair<int, std::string>>{{0x001, "|"},
                                                {0x020, "|"},
                                                {0x100, "|"},
                                                {0xFA2, "|"},
                                                {0x0C4, "_"},
                                                {0x0D1, "_"},
                                                {0x033, "_"},
                                                {0x129, "~"},
                                                {0x03A, "~"},
                                                {0xFD6, "~"},
                                                {0x021, "^"},
                                                {0x12D, "^"},
                                                {0xF9B, "^"},
                                                {0xFB5, "^"},
                                                {0x035, "D"},
                                                {0xFF4, "D"},
                                                {0xFF6, "D"},
                                                {0xF99, "C"},
                                                {0xFB1, "C"}}) {
    EXPECT_EQ(selector.object_type_symbol_for_testing(id), expected)
        << "object " << id;
  }
}

// ---------------------------------------------------------------------------
// Routine symbology tests (post-PR #233 integration)
//
// These exercise GetSymbologyForObject() directly — a free function in the
// zelda3 namespace — so they do not require an ImGui context or a ROM file.
// They are placed here because they pin the badge and tooltip contract that
// the selector exposes through DrawObjectAssetBrowser.
// ---------------------------------------------------------------------------

TEST(DungeonObjectRoutineSymbologyTest, Extensible4x4BadgeIsRightwards) {
  // Object 0x33 (Carpet) maps to kRightwards4x4_1to16 (routine 16).
  // The routine is extensible (repeats rightwards) with a 4x4 base pattern.
  const auto sym = zelda3::GetSymbologyForObject(0x33);
  EXPECT_EQ(sym.badge, ">") << "Rightwards category glyph must be '>'";
  EXPECT_NE(sym.family.find("4x4"), std::string::npos)
      << "Family string must encode the 4x4 base dimensions";
  EXPECT_NE(sym.family.find("Rightwards"), std::string::npos)
      << "Family string must name the Rightwards family";
  EXPECT_EQ(sym.base_width, 4);
  EXPECT_EQ(sym.base_height, 4);
  EXPECT_FALSE(sym.dual_layer)
      << "Rightwards4x4_1to16 does not write to both BG layers";
}

TEST(DungeonObjectRoutineSymbologyTest, CornerBothBGBadgeIsL2) {
  // Objects 0x108-0x10F map to kCorner4x4_BothBG (routine 35):
  //   Corner category, base 4x4, draws_to_both_bgs=true.
  // Badge = 'L' (Corner glyph) + '2' (BothBG suffix) = "L2".
  for (int obj_id = 0x108; obj_id <= 0x10F; ++obj_id) {
    SCOPED_TRACE(obj_id);
    const auto sym =
        zelda3::GetSymbologyForObject(static_cast<int16_t>(obj_id));
    EXPECT_EQ(sym.badge, "L2") << "Corner-BothBG badge must be 'L2'";
    EXPECT_TRUE(sym.dual_layer) << "kCorner4x4_BothBG writes to both BG layers";
    EXPECT_NE(sym.family.find("Corner"), std::string::npos)
        << "Family must name the Corner family";
    EXPECT_NE(sym.family.find("BothBG"), std::string::npos)
        << "Family must include the BothBG qualifier";
    EXPECT_NE(sym.family.find("4x4"), std::string::npos)
        << "Family must encode the 4x4 base dimensions";
    EXPECT_EQ(sym.base_width, 4);
    EXPECT_EQ(sym.base_height, 4);
  }
}

TEST(DungeonObjectRoutineSymbologyTest, BothBGNonCornerBadgeHasTwoSuffix) {
  // Objects 0x63-0x64 map to kDownwards4x2_1to16_BothBG (routine 9):
  //   Downwards category, base 4x2, draws_to_both_bgs=true.
  // Badge = 'v' (Downwards glyph) + '2' (BothBG suffix) = "v2".
  for (int obj_id = 0x63; obj_id <= 0x64; ++obj_id) {
    SCOPED_TRACE(obj_id);
    const auto sym =
        zelda3::GetSymbologyForObject(static_cast<int16_t>(obj_id));
    EXPECT_EQ(sym.badge, "v2") << "Downwards-BothBG badge must be 'v2'";
    EXPECT_TRUE(sym.dual_layer);
    EXPECT_NE(sym.family.find("Downwards"), std::string::npos);
    EXPECT_NE(sym.family.find("BothBG"), std::string::npos);
    EXPECT_EQ(sym.base_width, 4);
    EXPECT_EQ(sym.base_height, 2);
  }
}

TEST(DungeonObjectRoutineSymbologyTest,
     NamedSpecialsBadgesAreDistinctAndStable) {
  // The four named specials earn their own single-character badge that is
  // deliberately decoupled from the direction glyph.
  //
  //  0xF99 -> kChest (routine 39)       => "C" / "Chest"
  //  0xF98 -> kBigKeyLock (routine 92)  => "K" / "Big key lock"
  //  0xFC7 -> kBombableFloor (93)       => "B" / "Bombable floor"
  //  0xF8D -> kPrisonCell (97)          => "P" / "Prison cell"
  struct NamedCase {
    int16_t object_id;
    const char* expected_badge;
    const char* expected_family;
  };
  for (const auto& tc : {
           NamedCase{0xF99, "C", "Chest"},
           NamedCase{0xF98, "K", "Big key lock"},
           NamedCase{0xFC7, "B", "Bombable floor"},
           NamedCase{0xF8D, "P", "Prison cell"},
       }) {
    SCOPED_TRACE(tc.object_id);
    const auto sym = zelda3::GetSymbologyForObject(tc.object_id);
    EXPECT_EQ(sym.badge, tc.expected_badge);
    EXPECT_EQ(sym.family, tc.expected_family);
    EXPECT_FALSE(sym.dual_layer)
        << "Named specials do not write to both BG layers";
  }
}

TEST(DungeonObjectRoutineSymbologyTest, ChestGroupingCategoryVsRoutine) {
  // The "Chests" category groups objects that are related to chests in the
  // game world — but the draw *routine* badge tracks how each object is
  // rendered, which is a separate dimension.
  //
  // Primary chest objects (0xF99, 0xF9A) use the Chest draw routine → "C".
  // Platform/decor variants (0xFB1, 0xFB2, 0xFF5) use fixed-size Special
  // routines — "S" with dimensions — because they are rendered as a fixed
  // 4x3 or 2x2 block rather than through the chest-slot dispatch.
  //
  // This test pins both truths so a future registry change can't silently
  // move a chest object to the wrong routine.
  const auto chest_ids_or =
      zelda3::ObjectCategories::GetObjectsInCategory("Chests");
  ASSERT_TRUE(chest_ids_or.ok()) << chest_ids_or.status();
  EXPECT_EQ(chest_ids_or->size(), 5u) << "Chests category must have 5 members";

  // Primary slot objects: use the Chest draw routine.
  for (const int obj_id : {0xF99, 0xF9A}) {
    SCOPED_TRACE(obj_id);
    const auto sym =
        zelda3::GetSymbologyForObject(static_cast<int16_t>(obj_id));
    EXPECT_EQ(sym.badge, "C")
        << "Primary chest objects must have the Chest routine badge";
    EXPECT_EQ(sym.family, "Chest");
  }

  // Platform/decor variants: use fixed-size Special routines, NOT kChest.
  for (const int obj_id : {0xFB1, 0xFB2, 0xFF5}) {
    SCOPED_TRACE(obj_id);
    const auto sym =
        zelda3::GetSymbologyForObject(static_cast<int16_t>(obj_id));
    EXPECT_NE(sym.badge, "C")
        << "Chest platform/decor variants use non-Chest render routines";
    EXPECT_NE(sym.family, "Chest")
        << "Chest platform/decor variants use non-Chest render routines";
    // They are Special-category routines with fixed dimensions.
    EXPECT_EQ(sym.badge, "S");
    EXPECT_NE(sym.family.find("Special"), std::string::npos);
  }
}

TEST(DungeonObjectRoutineSymbologyTest,
     RoutineBadgeAndCategorySymbolAreSeparateSystems) {
  // The fallback tile label (GetObjectTypeSymbol, category-based: "|","_",…)
  // and the routine-overlay badge (GetSymbologyForObject, routine-based:
  // ">","v","L",…) are intentionally distinct systems.  An object can share
  // the same letter in both (e.g. "C" for Chests in both systems) but the
  // symbols for non-chest directional families never coincide.
  //
  // This test pins that contract for the selector's two presentation paths:
  //   fallback path:   object_type_symbol_for_testing() (category symbol)
  //   badge path:      GetSymbologyForObject().badge    (routine symbol)
  DungeonObjectSelector selector;

  // Walls use "|" in the category system but ">" or "v" or "L" in routine.
  {
    const std::string category_sym =
        selector.object_type_symbol_for_testing(0x001);
    const auto routine_sym = zelda3::GetSymbologyForObject(0x001);
    EXPECT_EQ(category_sym, "|");
    EXPECT_NE(routine_sym.badge, "|")
        << "Routine badge must not duplicate the category symbol for walls";
  }
  // Floors use "_" in category; 0x033 (Carpet) is Rightwards in routine.
  {
    const std::string category_sym =
        selector.object_type_symbol_for_testing(0x033);
    const auto routine_sym = zelda3::GetSymbologyForObject(0x033);
    EXPECT_EQ(category_sym, "_");
    EXPECT_NE(routine_sym.badge, "_")
        << "Routine badge must not duplicate the category symbol for floors";
  }
  // Chests: both systems happen to return "C" — that is intentional.
  {
    const std::string category_sym =
        selector.object_type_symbol_for_testing(0xF99);
    const auto routine_sym = zelda3::GetSymbologyForObject(0xF99);
    EXPECT_EQ(category_sym, "C");
    EXPECT_EQ(routine_sym.badge, "C")
        << "Chest badge must be 'C' in both systems";
  }
}

TEST(DungeonObjectRoutineSymbologyTest, UnmappedObjectBadgeIsQuestionMark) {
  // Any object ID not in the registry's object→routine mapping returns
  // badge "?" so the card overlay code can suppress it for visual cleanliness.
  //
  // 0x0140 is just above the Type 2 codec range (0x100-0x13F) and has no
  // individual entry in the registry mapping, so it returns "Unmapped".
  // (0x0F8 is NOT a valid choice here: it maps to the Chest routine, 39.)
  const auto sym = zelda3::GetSymbologyForObject(0x0140);
  EXPECT_EQ(sym.badge, "?");
  EXPECT_EQ(sym.family, "Unmapped");
}

}  // namespace
}  // namespace editor
}  // namespace yaze

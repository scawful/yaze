#include "app/editor/overworld/maps/map_refresh_coordinator.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "app/editor/overworld/maps/map_texture_coordinator.h"
#include "app/editor/overworld/overworld_undo_actions.h"

#include <array>
#include <memory>

#include "absl/status/status.h"
#include "app/gfx/backend/sdl2_renderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {
namespace {

// ---------------------------------------------------------------------------
// Test fixture for MapRefreshCoordinator.
//
// The coordinator operates entirely through its MapRefreshContext struct.
// We construct a lightweight context with:
//   - A real `maps_bmp` array (160 default Bitmaps).
//   - A dummy Rom + Overworld (no ROM loaded, maps vector empty).
//   - Scalar state backing for current_world, current_map, etc.
//
// Methods that need a fully loaded Overworld (e.g. RefreshChildMap) are
// NOT exercised here -- those belong in integration tests with real ROMs.
// We focus on bounds checking, deferred vs immediate logic, and cache
// invalidation.
// ---------------------------------------------------------------------------
class MapRefreshCoordinatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);

    ctx_.rom = &rom_;
    ctx_.overworld = overworld_.get();
    ctx_.maps_bmp = &maps_bmp_;
    ctx_.tile16_blockset = &tile16_blockset_;
    ctx_.current_gfx_bmp = &current_gfx_bmp_;
    ctx_.current_graphics_set = &current_graphics_set_;
    ctx_.palette = &palette_;
    ctx_.current_world = &current_world_;
    ctx_.current_map = &current_map_;
    ctx_.current_blockset = &current_blockset_;
    ctx_.game_state = &game_state_;
    ctx_.map_blockset_loaded = &map_blockset_loaded_;
    ctx_.status = &status_;

    coordinator_ = std::make_unique<MapRefreshCoordinator>(ctx_);
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;

  std::array<gfx::Bitmap, zelda3::kExpandedMapCount> maps_bmp_{};
  gfx::Tilemap tile16_blockset_{};
  gfx::Bitmap current_gfx_bmp_;
  gfx::BitmapTable current_graphics_set_;
  gfx::SnesPalette palette_;

  int current_world_ = 0;
  int current_map_ = 0;
  int current_blockset_ = 0;
  int game_state_ = 0;
  bool map_blockset_loaded_ = false;
  absl::Status status_;

  MapRefreshContext ctx_{};
  std::unique_ptr<MapRefreshCoordinator> coordinator_;

  void PopulateSingleMapForRefresh(int map_index) {
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        overworld_->overworld_maps());
    maps.clear();
    for (int i = 0; i <= map_index; ++i) {
      maps.emplace_back(i, &rom_);
    }

    auto* tiles = overworld_->mutable_map_tiles();
    tiles->light_world.assign(0x200, std::vector<uint16_t>(0x200, 0));
    tiles->dark_world.assign(0x200, std::vector<uint16_t>(0x200, 0));
    tiles->special_world.assign(0x200, std::vector<uint16_t>(0x200, 0));
  }
};

TEST_F(MapRefreshCoordinatorTest, ChildSelectionRefreshesFromEditedParent) {
  ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rom_[zelda3::OverworldCustomASMHasBeenApplied] = 3;
  PopulateSingleMapForRefresh(0x39);
  for (int child : {0x30, 0x31, 0x38, 0x39}) {
    overworld_->mutable_overworld_map(child)->SetAsLargeMap(
        0x30, (child - 0x30) / 8 * 2 + (child - 0x30) % 8);
    overworld_->mutable_overworld_map(child)->set_area_palette(0x0E);
    overworld_->mutable_overworld_map(child)->set_area_graphics(0x10);
  }
  auto* parent = overworld_->mutable_overworld_map(0x30);
  parent->set_area_palette(0x1C);
  parent->set_area_graphics(0x38);
  current_map_ = 0x31;
  coordinator_->RefreshMapProperties();
  for (int child : {0x30, 0x31, 0x38, 0x39}) {
    EXPECT_EQ(overworld_->overworld_map(child)->area_palette(), 0x1C) << child;
    EXPECT_EQ(overworld_->overworld_map(child)->area_graphics(), 0x38) << child;
  }
}

TEST_F(MapRefreshCoordinatorTest,
       PaletteUndoRefreshesOriginalAreaAfterNavigation) {
  ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rom_[zelda3::OverworldCustomASMHasBeenApplied] = 3;
  PopulateSingleMapForRefresh(0x39);
  for (int child : {0x30, 0x31, 0x38, 0x39}) {
    auto* map = overworld_->mutable_overworld_map(child);
    map->SetAsLargeMap(0x30, (child - 0x30) / 8 * 2 + (child - 0x30) % 8);
    map->set_area_palette(0x0E);
  }
  auto* unrelated = overworld_->mutable_overworld_map(0x00);
  unrelated->SetAsSmallMap(0x00);
  unrelated->set_area_palette(0x05);
  MapPropertiesSystem properties(overworld_.get(), &rom_);
  std::vector<int> palette_targets;
  properties.SetRefreshCallbacks(
      [this](int map_id) { coordinator_->RefreshMapProperties(map_id); },
      nullptr,
      [&](int map_id) {
        palette_targets.push_back(map_id);
        return absl::OkStatus();
      });
  const OverworldPropertyEdit before{0x31, OverworldPropertyField::kAreaPalette,
                                     0, 0x0E};
  auto after = before;
  after.value = 0x1C;
  current_map_ = 0x31;
  ASSERT_TRUE(properties.ApplyPropertyEditDirect(after).ok());
  for (int child : {0x30, 0x31, 0x38, 0x39})
    EXPECT_EQ(overworld_->overworld_map(child)->area_palette(), 0x1C);
  OverworldMapPropertyEditAction action(
      before, after,
      [&](const OverworldPropertyEdit& edit) {
        return properties.ApplyPropertyEditDirect(edit);
      },
      "Palette");
  current_map_ = 0x00;
  ASSERT_TRUE(action.Undo().ok());
  for (int child : {0x30, 0x31, 0x38, 0x39})
    EXPECT_EQ(overworld_->overworld_map(child)->area_palette(), 0x0E) << child;
  EXPECT_EQ(unrelated->area_palette(), 0x05);
  EXPECT_EQ(current_map_, 0x00);
  ASSERT_TRUE(action.Redo().ok());
  for (int child : {0x30, 0x31, 0x38, 0x39})
    EXPECT_EQ(overworld_->overworld_map(child)->area_palette(), 0x1C) << child;
  EXPECT_EQ(unrelated->area_palette(), 0x05);
  EXPECT_EQ(current_map_, 0x00);
  EXPECT_EQ(palette_targets, (std::vector<int>{0x30, 0x30, 0x30}));
}

// ===========================================================================
// ForceRefreshGraphics
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest, DefinitionEditsInvalidateAllWorlds) {
  coordinator_->InvalidateTile16Definitions();
  for (int id = 0; id < zelda3::kNumOverworldMaps; ++id) {
    EXPECT_TRUE(maps_bmp_[id].modified()) << id;
  }
}

TEST_F(MapRefreshCoordinatorTest, ForceRefreshGraphicsValidIndexMarksModified) {
  ASSERT_FALSE(maps_bmp_[5].modified());
  coordinator_->ForceRefreshGraphics(5);
  EXPECT_TRUE(maps_bmp_[5].modified());
  // Blockset cache should be invalidated.
  EXPECT_EQ(current_blockset_, 0xFF);
}

TEST_F(MapRefreshCoordinatorTest,
       ForceRefreshGraphicsNegativeIndexDoesNotCrash) {
  coordinator_->ForceRefreshGraphics(-1);
  // No crash, no state change.
  EXPECT_EQ(current_blockset_, 0);
}

TEST_F(MapRefreshCoordinatorTest, ForceRefreshGraphicsOutOfBoundsDoesNotCrash) {
  coordinator_->ForceRefreshGraphics(zelda3::kNumOverworldMaps);
  EXPECT_EQ(current_blockset_, 0);
}

TEST_F(MapRefreshCoordinatorTest, ForceRefreshGraphicsBoundaryIndex159Works) {
  coordinator_->ForceRefreshGraphics(159);
  EXPECT_TRUE(maps_bmp_[159].modified());
  EXPECT_EQ(current_blockset_, 0xFF);
}

// ===========================================================================
// RefreshOverworldMapOnDemand -- deferred path
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandDeferredPathSetsModified) {
  // If map_index is in a different world than current, it should be deferred
  // (just marks modified and returns).
  current_world_ = 0;  // Light world: maps 0-63
  current_map_ = 0;

  // Map 100 is in world 1 (dark world: 64-127). Not current world, not current
  // map => deferred.
  ASSERT_FALSE(maps_bmp_[100].modified());
  coordinator_->RefreshOverworldMapOnDemand(100);
  EXPECT_TRUE(maps_bmp_[100].modified());
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandCurrentMapNotDeferred) {
  // When map IS the current map, it should NOT take the deferred path.
  // However, the non-deferred path calls RefreshChildMapOnDemand which
  // accesses overworld maps that don't exist -- this should not crash
  // because the Bitmap is default-constructed (modified() is false,
  // so needs_graphics_rebuild is false, skipping the heavy path).
  current_world_ = 0;
  current_map_ = 5;
  maps_bmp_[5].set_modified(false);

  // Should not crash even though Overworld has no loaded maps.
  coordinator_->RefreshOverworldMapOnDemand(5);
  // Only the deferred path marks the bitmap modified.
  EXPECT_FALSE(maps_bmp_[5].modified());
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandSameWorldNotDeferred) {
  // Map 10 is in the same world (0) as current. Even though it's not the
  // current_map, it's in the same world so it takes the non-deferred path.
  current_world_ = 0;
  current_map_ = 0;
  maps_bmp_[10].set_modified(false);

  coordinator_->RefreshOverworldMapOnDemand(10);
  // The non-deferred path enters RefreshChildMapOnDemand, which returns early
  // without loaded maps. Only the deferred path marks the bitmap modified.
  EXPECT_FALSE(maps_bmp_[10].modified());
}

// ===========================================================================
// InvalidateGraphicsCache
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest, InvalidateGraphicsCacheAllClearsGraphicsSet) {
  // Add some entries to the graphics set.
  current_graphics_set_[0] = std::make_unique<gfx::Bitmap>();
  current_graphics_set_[1] = std::make_unique<gfx::Bitmap>();
  ASSERT_EQ(current_graphics_set_.size(), 2u);

  coordinator_->InvalidateGraphicsCache(-1);  // -1 = invalidate all
  EXPECT_TRUE(current_graphics_set_.empty());
}

TEST_F(MapRefreshCoordinatorTest,
       InvalidateGraphicsCacheSpecificErasesOneEntry) {
  current_graphics_set_[0] = std::make_unique<gfx::Bitmap>();
  current_graphics_set_[5] = std::make_unique<gfx::Bitmap>();
  ASSERT_EQ(current_graphics_set_.size(), 2u);

  coordinator_->InvalidateGraphicsCache(0);
  EXPECT_EQ(current_graphics_set_.size(), 1u);
  EXPECT_FALSE(current_graphics_set_.contains(0));
  EXPECT_TRUE(current_graphics_set_.contains(5));
}

// ===========================================================================
// RefreshMapPalette and RefreshChildMapOnDemand
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       RefreshMapPaletteFailsWhenCurrentMapIsNotLoaded) {
  const absl::Status status = coordinator_->RefreshMapPalette();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshChildMapOnDemandRestoresCurrentGameStateBeforeGraphicsLoad) {
  std::vector<uint8_t> rom_data(0x200000, 0x00);
  rom_data[zelda3::OverworldCustomASMHasBeenApplied] = 0xFF;
  for (int i = 0; i < 64; ++i) {
    rom_data[zelda3::kOverworldMapParentId + i] = static_cast<uint8_t>(i);
  }

  const auto version_constants =
      zelda3::kVersionConstantsMap.at(zelda3_version::US);
  rom_data[version_constants.kSpriteBlocksetPointer + (1 * 4)] = 0x10;
  rom_data[version_constants.kSpriteBlocksetPointer + (2 * 4)] = 0x20;
  rom_.LoadFromData(rom_data);

  PopulateSingleMapForRefresh(0);
  auto* map = overworld_->mutable_overworld_map(0);
  ASSERT_NE(map, nullptr);
  map->set_sprite_graphics(0, 1);
  map->set_sprite_graphics(2, 2);
  map->Destroy();

  game_state_ = 2;
  maps_bmp_[0].set_modified(true);

  coordinator_->RefreshChildMapOnDemand(0);

  EXPECT_EQ(map->game_state(), 2);
  EXPECT_EQ(map->static_graphics(12), 0x20 + 0x73);

  // A later on-demand rebuild must keep the coordinator's selected state.
  // Previously the model's default state 0 silently restored spriteset 1.
  overworld_->InvalidateMapCache(0);
  ASSERT_TRUE(overworld_->EnsureMapBuilt(0).ok());
  EXPECT_EQ(map->game_state(), 2);
  EXPECT_EQ(map->static_graphics(12), 0x20 + 0x73);
}

// Exercise the real model -> editor bitmap -> refresh boundary. A built map's
// cached pixels intentionally remain old after a paint until the refresh runs.
class OverworldPaintRefreshTest : public MapRefreshCoordinatorTest {
 protected:
  void SetUp() override {
    MapRefreshCoordinatorTest::SetUp();
    std::vector<uint8_t> data(0x200000, 0);
    data[zelda3::OverworldCustomASMHasBeenApplied] = 0xFF;
    for (int i = 0; i < 64; ++i) {
      data[zelda3::kOverworldMapParentId + i] = i;
      data[zelda3::kOverworldScreenSize + i] = 1;
    }
    data[zelda3::kOverlayPointers + 1] = 0x80;
    data[0x70000] = 0x60;  // Empty vanilla overlay (RTS at $0E:8000).
    ASSERT_TRUE(rom_.LoadFromData(data).ok());
    PopulateSingleMapForRefresh(0);
    const gfx::TileInfo old_info(0, 0, false, false, false);
    const gfx::TileInfo new_info(0, 1, false, false, false);
    *overworld_->mutable_tiles16() = {
        gfx::Tile16(old_info, old_info, old_info, old_info),
        gfx::Tile16(new_info, new_info, new_info, new_info)};
    auto* map = overworld_->mutable_overworld_map(0);
    ASSERT_TRUE(map->BuildMap(2, 0, 0, *overworld_->mutable_tiles16(),
                              overworld_->GetMapTiles(0))
                    .ok());
    ASSERT_TRUE(map->is_built());
    ASSERT_EQ(map->bitmap_data()[0], 0);
    maps_bmp_[0].Create(512, 512, 8, map->bitmap_data());
    maps_bmp_[0].SetPalette(map->current_palette());
    maps_bmp_[0].set_modified(false);

    MapTextureContext texture_context;
    texture_context.overworld = overworld_.get();
    texture_context.maps_bmp = &maps_bmp_;
    texture_context.current_map = &current_map_;
    texture_context.current_world = &current_world_;
    texture_context.refresh_map_on_demand = [this](int map_id) {
      ++refresh_count_;
      coordinator_->RefreshOverworldMapOnDemand(map_id);
    };
    textures_ =
        std::make_unique<OverworldMapTextureCoordinator>(texture_context);
  }

  void TearDown() override {
    // Deferred commands borrow the fixture's bitmaps.
    gfx::Arena::Get().ClearTextureQueue();
  }

  int refresh_count_ = 0;
  std::unique_ptr<OverworldMapTextureCoordinator> textures_;
};

TEST_F(OverworldPaintRefreshTest,
       FreshMapTexturePreservesIndexedPixelsAndColors) {
  auto* map = overworld_->mutable_overworld_map(0);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      overworld_->GetMapTiles(0)[x][y] = (x + y) % 2;
    }
  }
  ASSERT_TRUE(map->BuildBitmap(overworld_->GetMapTiles(0)).ok());
  *map->mutable_current_palette() =
      gfx::SnesPalette(std::vector<gfx::SnesColor>(256));
  (*map->mutable_current_palette())[0] = gfx::SnesColor(248, 0, 0);
  (*map->mutable_current_palette())[16] = gfx::SnesColor(0, 0, 248);

  // Exercise first display and recreation, not a pre-created test bitmap.
  for (int pass = 0; pass < 2; ++pass) {
    SCOPED_TRACE(pass);
    textures_->ResetMapBitmaps();
    textures_->EnsureMapTexture(0);
    auto& bitmap = maps_bmp_[0];
    ASSERT_TRUE(bitmap.is_active());
    EXPECT_EQ(bitmap.depth(), 8);
    EXPECT_TRUE(bitmap.IsIndexed());
    EXPECT_TRUE(bitmap.ValidateDataSurfaceSync());

    // Use the production SDL upload and a windowless software renderer. Checking
    // the indexed model alone misses palette/row-stride errors at this boundary.
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> target(
        SDL_CreateRGBSurfaceWithFormat(0, 512, 512, 32, SDL_PIXELFORMAT_RGBA32),
        SDL_FreeSurface);
    ASSERT_NE(target, nullptr) << SDL_GetError();
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateSoftwareRenderer(target.get()), SDL_DestroyRenderer);
    ASSERT_NE(renderer, nullptr) << SDL_GetError();
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
        SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STREAMING, 512, 512),
        SDL_DestroyTexture);
    ASSERT_NE(texture, nullptr) << SDL_GetError();
    gfx::SDL2Renderer uploader;
    uploader.UpdateTexture(texture.get(), bitmap);
    ASSERT_EQ(SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr),
              0)
        << SDL_GetError();
    std::vector<uint8_t> rendered(512 * 512 * 4);
    ASSERT_EQ(
        SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_RGBA32,
                             rendered.data(), 512 * 4),
        0)
        << SDL_GetError();
    size_t mismatched_pixels = 0;
    for (size_t i = 0; i < map->bitmap_data().size(); ++i) {
      const auto color = map->current_palette()[map->bitmap_data()[i]].rgb();
      if (rendered[i * 4] != static_cast<uint8_t>(color.x) ||
          rendered[i * 4 + 1] != static_cast<uint8_t>(color.y) ||
          rendered[i * 4 + 2] != static_cast<uint8_t>(color.z) ||
          rendered[i * 4 + 3] != 255) {
        ++mismatched_pixels;
      }
    }
    EXPECT_EQ(mismatched_pixels, 0u);
    gfx::Arena::Get().ClearTextureQueue();
  }
}

TEST_F(OverworldPaintRefreshTest,
       EnsureTexturePreservesPaintUntilModelRefresh) {
  overworld_->GetMapTiles(0)[0][0] = 1;
  maps_bmp_[0].WriteToPixel(0, 0x10);
  maps_bmp_[0].set_modified(true);

  textures_->EnsureMapTexture(0);

  EXPECT_EQ(maps_bmp_[0].vector()[0], 0x10);
  EXPECT_TRUE(maps_bmp_[0].modified());
  EXPECT_EQ(overworld_->overworld_map(0)->bitmap_data()[0], 0);

  textures_->ProcessDeferredTextures();

  EXPECT_EQ(refresh_count_, 1);
  EXPECT_EQ(overworld_->GetMapTiles(0)[0][0], 1);
  EXPECT_EQ(overworld_->overworld_map(0)->bitmap_data()[0], 0x10);
  EXPECT_EQ(maps_bmp_[0].vector()[0], 0x10);
  EXPECT_FALSE(maps_bmp_[0].modified());
  textures_->EnsureMapTexture(0);
  textures_->ProcessDeferredTextures();
  EXPECT_EQ(refresh_count_, 1);
  EXPECT_EQ(maps_bmp_[0].vector()[0], 0x10);
}

TEST_F(OverworldPaintRefreshTest,
       EnsureTextureDoesNotConsumeContentInvalidation) {
  // Property/tile-array edits may invalidate without drawing replacement pixels.
  overworld_->GetMapTiles(0)[0][0] = 1;
  maps_bmp_[0].set_modified(true);
  textures_->EnsureMapTexture(0);
  ASSERT_TRUE(maps_bmp_[0].modified());
  textures_->ProcessDeferredTextures();
  EXPECT_EQ(refresh_count_, 1);
  EXPECT_EQ(overworld_->overworld_map(0)->bitmap_data()[0], 0x10);
  EXPECT_EQ(maps_bmp_[0].vector()[0], 0x10);
}

TEST_F(OverworldPaintRefreshTest, RebuiltModelStillSynchronizesCleanBitmap) {
  overworld_->GetMapTiles(0)[0][0] = 1;
  overworld_->mutable_overworld_map(0)->SetNotBuilt();
  textures_->EnsureMapTexture(0);
  EXPECT_TRUE(overworld_->overworld_map(0)->is_built());
  EXPECT_EQ(overworld_->overworld_map(0)->bitmap_data()[0], 0x10);
  EXPECT_EQ(maps_bmp_[0].vector()[0], 0x10);
  EXPECT_FALSE(maps_bmp_[0].modified());
}

TEST_F(OverworldPaintRefreshTest, GraphicsPreviewSurvivesEditRefreshAndClears) {
  zelda3::PreviewTile8 pixels{};
  pixels.fill(13);
  overworld_->SetGraphicsPreview({{0, {{0, pixels}}}});
  coordinator_->InvalidateTile16Definitions();
  coordinator_->RefreshOverworldMapOnDemand(0);
  ASSERT_TRUE(status_.ok()) << status_;
  EXPECT_EQ(maps_bmp_[0].vector()[0], 13);

  // Exercise the same invalidation/rebuild used after definition undo/redo.
  for (int tile : {1, 0, 1}) {
    overworld_->GetMapTiles(0)[0][0] = tile;
    coordinator_->InvalidateTile16Definitions();
    coordinator_->RefreshOverworldMapOnDemand(0);
    ASSERT_TRUE(status_.ok()) << status_;
    EXPECT_EQ(maps_bmp_[0].vector()[0], 13 + tile * 16);
    EXPECT_EQ(overworld_->overworld_map(0)->current_graphics()[0], 13);
  }
  overworld_->SetGraphicsPreview({});
  coordinator_->InvalidateTile16Definitions();
  coordinator_->RefreshOverworldMapOnDemand(0);
  EXPECT_EQ(maps_bmp_[0].vector()[0], 16);
  EXPECT_EQ(overworld_->overworld_map(0)->current_graphics()[0], 0);
}

}  // namespace
}  // namespace yaze::editor

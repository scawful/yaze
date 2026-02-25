#include "app/editor/overworld/map_refresh_coordinator.h"

#include <array>
#include <memory>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_palette.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"

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

  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps> maps_bmp_{};
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
};

// ===========================================================================
// ForceRefreshGraphics
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       ForceRefreshGraphicsValidIndexMarksModified) {
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

TEST_F(MapRefreshCoordinatorTest,
       ForceRefreshGraphicsOutOfBoundsDoesNotCrash) {
  coordinator_->ForceRefreshGraphics(zelda3::kNumOverworldMaps);
  EXPECT_EQ(current_blockset_, 0);
}

TEST_F(MapRefreshCoordinatorTest,
       ForceRefreshGraphicsBoundaryIndex159Works) {
  coordinator_->ForceRefreshGraphics(159);
  EXPECT_TRUE(maps_bmp_[159].modified());
  EXPECT_EQ(current_blockset_, 0xFF);
}

// ===========================================================================
// RefreshOverworldMapOnDemand -- bounds checking
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandNegativeIndexNoCrash) {
  coordinator_->RefreshOverworldMapOnDemand(-1);
  // Negative index is rejected silently.
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandExcessiveIndexNoCrash) {
  coordinator_->RefreshOverworldMapOnDemand(zelda3::kNumOverworldMaps);
  // Out-of-bounds index is rejected silently.
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
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshOverworldMapOnDemandSameWorldNotDeferred) {
  // Map 10 is in the same world (0) as current. Even though it's not the
  // current_map, it's in the same world so it takes the non-deferred path.
  current_world_ = 0;
  current_map_ = 0;
  maps_bmp_[10].set_modified(false);

  coordinator_->RefreshOverworldMapOnDemand(10);
  // Should not crash. The non-deferred path enters RefreshChildMapOnDemand
  // which checks needs_graphics_rebuild (false) and skips heavy work.
}

// ===========================================================================
// InvalidateGraphicsCache
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       InvalidateGraphicsCacheAllClearsGraphicsSet) {
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
// RefreshOverworldMap delegates to RefreshOverworldMapOnDemand
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest, RefreshOverworldMapDelegatesToOnDemand) {
  // RefreshOverworldMap() calls RefreshOverworldMapOnDemand(*ctx_.current_map).
  // With current_map=5 and current_world=0, map 5 is both the current map
  // and in the current world, so it takes the non-deferred path into
  // RefreshChildMapOnDemand. With no loaded Overworld maps, the null-check
  // in RefreshChildMapOnDemand returns early -- verify no crash.
  current_world_ = 0;
  current_map_ = 5;
  coordinator_->RefreshOverworldMap();
  // Just verify no crash. The deferred path is tested separately.
}

TEST_F(MapRefreshCoordinatorTest,
       RefreshMapPaletteFailsWhenCurrentMapIsNotLoaded) {
  const absl::Status status = coordinator_->RefreshMapPalette();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

// ===========================================================================
// UpdateBlocksetWithPendingTileChanges -- early returns
// ===========================================================================

TEST_F(MapRefreshCoordinatorTest,
       UpdateBlocksetNotLoadedReturnsImmediately) {
  map_blockset_loaded_ = false;
  // Should not crash -- exits immediately.
  coordinator_->UpdateBlocksetWithPendingTileChanges();
}

}  // namespace
}  // namespace yaze::editor

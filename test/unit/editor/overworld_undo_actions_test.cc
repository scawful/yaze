#include "app/editor/overworld/overworld_undo_actions.h"

#include <functional>
#include <memory>
#include <vector>

#include "app/editor/core/undo_manager.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

class OverworldTilePaintActionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    for (int world = 0; world < 3; ++world) {
      overworld_->GetMapTiles(world).assign(256, std::vector<uint16_t>(256, 0));
    }
  }

  std::function<void(int)> RefreshCallback() {
    return [this](int map) {
      refreshed_maps_.push_back(map);
    };
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  std::vector<int> refreshed_maps_;
};

TEST_F(OverworldTilePaintActionTest,
       UndoRedoRefreshEveryScreenAcrossBothSeams) {
  for (int world = 0; world < 3; ++world) {
    SCOPED_TRACE(world);
    auto& tiles = overworld_->GetMapTiles(world);
    const std::vector<OverworldTileChange> changes = {
        {31, 31, 1, 11}, {32, 31, 2, 12}, {31, 32, 3, 13}, {32, 32, 4, 14}};
    for (const auto& change : changes)
      tiles[change.x][change.y] = change.new_tile_id;

    int legacy_refreshes = 0;
    OverworldTilePaintAction action(
        world * 0x40, world, changes, overworld_.get(),
        [&] { ++legacy_refreshes; }, RefreshCallback());
    const std::vector<int> expected_maps = {world * 0x40, world * 0x40 + 1,
                                            world * 0x40 + 8, world * 0x40 + 9};

    refreshed_maps_.clear();
    ASSERT_TRUE(action.Undo().ok());
    for (const auto& change : changes)
      EXPECT_EQ(tiles[change.x][change.y], change.old_tile_id);
    EXPECT_EQ(refreshed_maps_, expected_maps);

    refreshed_maps_.clear();
    ASSERT_TRUE(action.Redo().ok());
    for (const auto& change : changes)
      EXPECT_EQ(tiles[change.x][change.y], change.new_tile_id);
    EXPECT_EQ(refreshed_maps_, expected_maps);
    EXPECT_EQ(legacy_refreshes, 0);
  }
}

TEST_F(OverworldTilePaintActionTest,
       RevisitedTilesKeepFirstOldAndLastNewValue) {
  auto& tiles = overworld_->GetMapTiles(0);
  tiles[31][31] = 30;
  tiles[32][31] = 40;
  OverworldTilePaintAction action(
      0, 0,
      {{31, 31, 1, 10}, {32, 31, 2, 20}, {31, 31, 10, 30}, {32, 31, 20, 40}},
      overworld_.get(), std::function<void()>{}, RefreshCallback());

  ASSERT_EQ(action.tile_changes().size(), 2u);
  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(tiles[31][31], 1);
  EXPECT_EQ(tiles[32][31], 2);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0, 1}));

  refreshed_maps_.clear();
  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(tiles[31][31], 30);
  EXPECT_EQ(tiles[32][31], 40);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0, 1}));
}

TEST_F(OverworldTilePaintActionTest,
       MergedActionsRefreshUnionOfChangedScreens) {
  auto& tiles = overworld_->GetMapTiles(0);
  tiles[31][31] = 11;
  tiles[32][31] = 22;
  tiles[31][32] = 13;
  tiles[32][32] = 14;
  UndoManager history;
  history.Push(std::make_unique<OverworldTilePaintAction>(
      0, 0, std::vector<OverworldTileChange>{{31, 31, 1, 11}, {32, 31, 2, 12}},
      overworld_.get(), std::function<void()>{}, RefreshCallback()));
  history.Push(std::make_unique<OverworldTilePaintAction>(
      8, 0,
      std::vector<OverworldTileChange>{
          {31, 32, 3, 13}, {32, 32, 4, 14}, {32, 31, 12, 22}},
      overworld_.get(), std::function<void()>{}, RefreshCallback()));

  ASSERT_EQ(history.UndoStackSize(), 1u);
  ASSERT_TRUE(history.Undo().ok());
  EXPECT_EQ(tiles[31][31], 1);
  EXPECT_EQ(tiles[32][31], 2);
  EXPECT_EQ(tiles[31][32], 3);
  EXPECT_EQ(tiles[32][32], 4);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0, 1, 8, 9}));

  refreshed_maps_.clear();
  ASSERT_TRUE(history.Redo().ok());
  EXPECT_EQ(tiles[31][31], 11);
  EXPECT_EQ(tiles[32][31], 22);
  EXPECT_EQ(tiles[31][32], 13);
  EXPECT_EQ(tiles[32][32], 14);
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0, 1, 8, 9}));
}

TEST_F(OverworldTilePaintActionTest, RefreshesLastAllocatedSpecialWorldScreen) {
  OverworldTilePaintAction action(0x9F, 2, {{255, 127, 1, 2}}, overworld_.get(),
                                  std::function<void()>{}, RefreshCallback());
  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(refreshed_maps_, (std::vector<int>{0x9F}));
  EXPECT_EQ(overworld_->GetMapTiles(2)[255][127], 1);
}

TEST_F(OverworldTilePaintActionTest, LegacyRefreshCallbackRemainsSupported) {
  int refreshes = 0;
  OverworldTilePaintAction action(0, 0, {{1, 2, 3, 4}}, overworld_.get(),
                                  [&] { ++refreshes; });
  ASSERT_TRUE(action.Undo().ok());
  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(refreshes, 2);
}

TEST_F(OverworldTilePaintActionTest, DifferentWorldsAndModelsDoNotMerge) {
  OverworldTilePaintAction first(0, 0, {}, overworld_.get(), {});
  OverworldTilePaintAction other_world(0x40, 1, {}, overworld_.get(), {});
  auto other_overworld = std::make_unique<zelda3::Overworld>(&rom_);
  OverworldTilePaintAction other_model(0, 0, {}, other_overworld.get(), {});
  EXPECT_FALSE(other_world.CanMergeWith(first));
  EXPECT_FALSE(other_model.CanMergeWith(first));
}

}  // namespace
}  // namespace yaze::editor

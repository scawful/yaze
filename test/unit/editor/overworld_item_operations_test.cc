#include "app/editor/overworld/entity/entity_operations.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {
namespace {

std::vector<zelda3::OverworldMap> BuildOverworldMaps(Rom* rom) {
  std::vector<zelda3::OverworldMap> maps;
  maps.reserve(zelda3::kNumOverworldMaps);
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    maps.emplace_back(i, rom);
    maps.back().SetAsSmallMap(i);
  }
  return maps;
}

class OverworldItemOperationsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<uint8_t> rom_data(0x200000, 0x00);
    ASSERT_TRUE(rom_.LoadFromData(rom_data).ok());
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

TEST_F(OverworldItemOperationsTest,
       ItemInsertionValuesPreserveAllParentQuadrantsAndWorlds) {
  auto maps = BuildOverworldMaps(&rom_);
  for (int world = 0; world < 3; ++world) {
    const int parent = world * 64 + 0x12;
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
      const int dx = quadrant % 2;
      const int dy = quadrant / 2;
      const int screen = parent + dx + dy * 8;
      maps[screen].SetAsLargeMap(parent, quadrant);
      const ImVec2 position(1024 + dx * 512 + 31, 1024 + dy * 512 + 47);
      auto item = BuildOverworldItemForInsertion(maps, position, screen, 0x2A);
      ASSERT_TRUE(item.ok()) << item.status();
      EXPECT_EQ(item->room_map_id_, parent);
      EXPECT_EQ(item->x_, 1040 + dx * 512);
      EXPECT_EQ(item->y_, 1056 + dy * 512);
      EXPECT_EQ(item->game_x_, 1 + dx * 32);
      EXPECT_EQ(item->game_y_, 2 + dy * 32);
      EXPECT_EQ(item->id_, 0x2A);
    }
  }
  EXPECT_TRUE(overworld_->all_items().empty());
}

TEST_F(OverworldItemOperationsTest,
       SpriteInsertionValuesPreserveAllParentQuadrantsAndWorlds) {
  auto maps = BuildOverworldMaps(&rom_);
  for (int world = 0; world < 3; ++world) {
    const int parent = world * 64 + 0x12;
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
      const int dx = quadrant % 2;
      const int dy = quadrant / 2;
      const int screen = parent + dx + dy * 8;
      maps[screen].SetAsLargeMap(parent, quadrant);
      const ImVec2 position(1024 + dx * 512 + 31, 1024 + dy * 512 + 47);
      auto sprite =
          BuildOverworldSpriteForInsertion(maps, position, screen, 0x2A);
      ASSERT_TRUE(sprite.ok()) << sprite.status();
      EXPECT_EQ(sprite->map_id(), parent);
      EXPECT_EQ(sprite->x(), 1040 + dx * 512);
      EXPECT_EQ(sprite->y(), 1056 + dy * 512);
      EXPECT_EQ(sprite->nx(), 1 + dx * 32);
      EXPECT_EQ(sprite->ny(), 2 + dy * 32);
      EXPECT_EQ(sprite->id(), 0x2A);
    }
  }
  EXPECT_TRUE(overworld_->sprites(0).empty());
}

TEST_F(OverworldItemOperationsTest,
       ParentQuadrantInsertionValuesSurviveItemSaveLoad) {
  auto maps = BuildOverworldMaps(&rom_);
  std::vector<zelda3::OverworldItem> items;
  // Vanilla saves support Light and Dark World items. Special World is covered
  // by value construction above without claiming expanded-ROM save support.
  for (int world = 0; world < 2; ++world) {
    const int parent = world * 64 + 0x12;
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
      const int dx = quadrant % 2;
      const int dy = quadrant / 2;
      const int screen = parent + dx + dy * 8;
      maps[screen].SetAsLargeMap(parent, quadrant);
      auto item = BuildOverworldItemForInsertion(
          maps, ImVec2(1040 + dx * 512, 1056 + dy * 512), screen,
          static_cast<uint8_t>(0x10 + world * 4 + quadrant));
      ASSERT_TRUE(item.ok()) << item.status();
      items.push_back(*item);
    }
  }

  ASSERT_TRUE(zelda3::SaveItems(&rom_, items).ok());
  auto loaded = zelda3::LoadItems(&rom_, maps);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), items.size());
  for (const auto& expected : items) {
    auto actual = std::find_if(
        loaded->begin(), loaded->end(),
        [&](const auto& item) { return item.id_ == expected.id_; });
    ASSERT_NE(actual, loaded->end());
    EXPECT_EQ(actual->room_map_id_, expected.room_map_id_);
    EXPECT_EQ(actual->x_, expected.x_);
    EXPECT_EQ(actual->y_, expected.y_);
    EXPECT_EQ(actual->game_x_, expected.game_x_);
    EXPECT_EQ(actual->game_y_, expected.game_y_);
  }
}

TEST_F(OverworldItemOperationsTest,
       EntityInsertionValuesRejectInvalidScreenParentAndCoordinates) {
  auto maps = BuildOverworldMaps(&rom_);
  const auto expect_rejected = [&](ImVec2 position, int screen) {
    const auto item = BuildOverworldItemForInsertion(maps, position, screen, 0);
    const auto sprite =
        BuildOverworldSpriteForInsertion(maps, position, screen, 0);
    EXPECT_EQ(item.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(sprite.status().code(), absl::StatusCode::kInvalidArgument);
  };

  expect_rejected(ImVec2(0, 0), -1);
  expect_rejected(ImVec2(0, 0), zelda3::kNumOverworldMaps);
  expect_rejected(ImVec2(528, 16), 0);  // Position belongs to screen 1.
  expect_rejected(ImVec2(-1, 0), 0);
  expect_rejected(ImVec2(4096, 0), 0);
  expect_rejected(ImVec2(0, 4096), 0);
  expect_rejected(ImVec2(std::numeric_limits<float>::quiet_NaN(), 0), 0);
  expect_rejected(ImVec2(0, std::numeric_limits<float>::infinity()), 0);
  expect_rejected(ImVec2(0, 2048), 0x80);  // Outside allocated Special World.

  for (const int parent : {-1, 64, zelda3::kNumOverworldMaps, 256}) {
    maps[0].SetParent(parent);
    expect_rejected(ImVec2(16, 16), 0);
  }
  maps[0].SetParent(0);
  maps[1].SetParent(0);
  maps[0].SetParent(1);  // Parent chain/cycle is not an area root.
  expect_rejected(ImVec2(528, 16), 1);
  maps[0].SetParent(0);
  maps[2].SetParent(0);
  expect_rejected(ImVec2(1024, 16), 2);  // Parent-local X would be 64.
  maps[16].SetParent(0);
  expect_rejected(ImVec2(16, 1024), 16);  // Parent-local Y would be 64.
  maps[1].SetParent(1);
  maps[0].SetParent(1);
  expect_rejected(ImVec2(16, 16), 0);  // Negative parent-relative X.
}

TEST_F(OverworldItemOperationsTest,
       EntityInsertionValuesAllowSelfParentSentinelAndRejectMissingMaps) {
  auto maps = BuildOverworldMaps(&rom_);
  maps[0x9F].SetParent(0xFF);
  auto item = BuildOverworldItemForInsertion(maps, ImVec2(4095, 2047), 0x9F, 0);
  ASSERT_TRUE(item.ok()) << item.status();
  EXPECT_EQ(item->room_map_id_, 0x9F);
  EXPECT_EQ(item->game_x_, 31);
  EXPECT_EQ(item->game_y_, 31);

  maps.clear();
  EXPECT_EQ(
      BuildOverworldItemForInsertion(maps, ImVec2(0, 0), 0, 0).status().code(),
      absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(BuildOverworldSpriteForInsertion(maps, ImVec2(0, 0), 0, 0)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(OverworldItemOperationsTest, InsertionStillRequiresLoadedModel) {
  ASSERT_FALSE(overworld_->is_loaded());
  EXPECT_EQ(InsertItem(overworld_.get(), ImVec2(16, 16), 0).status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(
      InsertSprite(overworld_.get(), ImVec2(16, 16), 0, 0).status().code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_TRUE(overworld_->all_items().empty());
  EXPECT_TRUE(overworld_->sprites(0).empty());
}

TEST_F(OverworldItemOperationsTest, RemoveItemErasesMatchingEntry) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x00, 16, 32, false);
  items->emplace_back(0x20, 0x00, 48, 64, false);
  items->emplace_back(0x30, 0x00, 80, 96, false);
  ASSERT_EQ(items->size(), 3u);

  const auto* middle_item = &items->at(1);
  const auto status = RemoveItem(overworld_.get(), middle_item);
  ASSERT_TRUE(status.ok()) << status.message();

  ASSERT_EQ(items->size(), 2u);
  EXPECT_EQ(items->at(0).id_, 0x10);
  EXPECT_EQ(items->at(1).id_, 0x30);
}

TEST_F(OverworldItemOperationsTest, RemoveItemRejectsNullArguments) {
  auto status = RemoveItem(nullptr, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);

  status = RemoveItem(overworld_.get(), nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(OverworldItemOperationsTest,
       RemoveItemReturnsNotFoundWhenPointerMissing) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x44, 0x00, 16, 16, false);
  ASSERT_EQ(items->size(), 1u);

  zelda3::OverworldItem external_item(0x55, 0x00, 32, 32, false);
  const auto status = RemoveItem(overworld_.get(), &external_item);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(items->size(), 1u);
}

TEST_F(OverworldItemOperationsTest, RemoveItemByIdentityErasesMatchingEntry) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x00, 16, 32, false);
  items->emplace_back(0x20, 0x00, 48, 64, false);
  items->emplace_back(0x30, 0x00, 80, 96, false);
  ASSERT_EQ(items->size(), 3u);

  const zelda3::OverworldItem snapshot = items->at(1);
  const auto status = RemoveItemByIdentity(overworld_.get(), snapshot);
  ASSERT_TRUE(status.ok()) << status.message();

  ASSERT_EQ(items->size(), 2u);
  EXPECT_EQ(items->at(0).id_, 0x10);
  EXPECT_EQ(items->at(1).id_, 0x30);
}

TEST_F(OverworldItemOperationsTest,
       RemoveItemByIdentityReturnsNotFoundWhenNoMatchingItemExists) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x44, 0x00, 16, 16, false);
  ASSERT_EQ(items->size(), 1u);

  zelda3::OverworldItem missing(0x44, 0x00, 32, 16, false);
  const auto status = RemoveItemByIdentity(overworld_.get(), missing);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(items->size(), 1u);
}

TEST_F(OverworldItemOperationsTest,
       FindItemByIdentityReturnsLiveItemWhenPresent) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x12, 0x00, 32, 48, false);
  items->emplace_back(0x34, 0x00, 64, 80, false);

  const zelda3::OverworldItem snapshot = items->at(1);
  auto* live = FindItemByIdentity(overworld_.get(), snapshot);
  ASSERT_NE(live, nullptr);
  EXPECT_EQ(live->id_, 0x34);
  EXPECT_EQ(live->x_, 64);
  EXPECT_EQ(live->y_, 80);
}

TEST_F(OverworldItemOperationsTest,
       DuplicateItemByIdentityClonesAndOffsetsItem) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x27, 0x00, 32, 48, false);
  ASSERT_EQ(items->size(), 1u);

  const zelda3::OverworldItem source = items->front();
  auto duplicate_or =
      DuplicateItemByIdentity(overworld_.get(), source, /*offset_x=*/16,
                              /*offset_y=*/-16);
  ASSERT_TRUE(duplicate_or.ok()) << duplicate_or.status().message();
  ASSERT_EQ(items->size(), 2u);

  auto* duplicated = duplicate_or.value();
  EXPECT_EQ(duplicated->id_, source.id_);
  EXPECT_EQ(duplicated->room_map_id_, source.room_map_id_);
  EXPECT_EQ(duplicated->x_, 48);
  EXPECT_EQ(duplicated->y_, 32);
  EXPECT_FALSE(duplicated->deleted);
}

TEST_F(OverworldItemOperationsTest, NudgeItemMovesAndClampsToBounds) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x55, 0x00, 0, 0, false);
  auto* item = &items->back();

  ASSERT_TRUE(NudgeItem(item, -64, -64).ok());
  EXPECT_EQ(item->x_, 0);
  EXPECT_EQ(item->y_, 0);

  ASSERT_TRUE(NudgeItem(item, 5000, 5000).ok());
  EXPECT_EQ(item->x_, 4080);
  EXPECT_EQ(item->y_, 4080);
}

TEST_F(OverworldItemOperationsTest,
       FindNearestItemForSelectionPrefersSameMapCandidates) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x01, 24, 24, false);    // Different map, closer
  items->emplace_back(0x20, 0x00, 320, 320, false);  // Same map, farther
  items->emplace_back(0x30, 0x00, 40, 48, false);    // Same map, nearest

  zelda3::OverworldItem deleted_anchor(0x44, 0x00, 16, 16, false);
  auto* selected =
      FindNearestItemForSelection(overworld_.get(), deleted_anchor);
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->id_, 0x30);
  EXPECT_EQ(selected->room_map_id_, 0x00);
}

TEST_F(OverworldItemOperationsTest,
       FindNearestItemForSelectionFallsBackToClosestCrossMap) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x02, 300, 300, false);
  items->emplace_back(0x20, 0x03, 48, 48, false);

  zelda3::OverworldItem deleted_anchor(0x44, 0x00, 16, 16, false);
  auto* selected =
      FindNearestItemForSelection(overworld_.get(), deleted_anchor);
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->id_, 0x20);
  EXPECT_EQ(selected->room_map_id_, 0x03);
}

TEST_F(OverworldItemOperationsTest,
       FindNearestItemForSelectionSkipsDeletedAndHandlesEmpty) {
  zelda3::OverworldItem deleted_anchor(0x44, 0x00, 16, 16, false);
  EXPECT_EQ(FindNearestItemForSelection(overworld_.get(), deleted_anchor),
            nullptr);

  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x00, 32, 32, false);
  items->back().deleted = true;
  EXPECT_EQ(FindNearestItemForSelection(overworld_.get(), deleted_anchor),
            nullptr);
}

TEST_F(OverworldItemOperationsTest,
       IdentityDeleteProtectsAgainstStaleSelectionDoubleDelete) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x10, 0x00, 16, 16, false);
  items->emplace_back(0x20, 0x00, 32, 16, false);
  items->emplace_back(0x30, 0x00, 48, 16, false);
  ASSERT_EQ(items->size(), 3u);

  const auto* selected_ptr = &items->at(1);
  const zelda3::OverworldItem selected_snapshot = items->at(1);
  ASSERT_TRUE(RemoveItem(overworld_.get(), selected_ptr).ok());
  ASSERT_EQ(items->size(), 2u);

  const auto second_delete =
      RemoveItemByIdentity(overworld_.get(), selected_snapshot);
  EXPECT_FALSE(second_delete.ok());
  EXPECT_EQ(second_delete.code(), absl::StatusCode::kNotFound);
  ASSERT_EQ(items->size(), 2u);
  EXPECT_EQ(items->at(0).id_, 0x10);
  EXPECT_EQ(items->at(1).id_, 0x30);
}

TEST_F(OverworldItemOperationsTest,
       DeleteThenSaveLoadRoundTripPreservesOnlyRemainingItems) {
  auto* items = overworld_->mutable_all_items();
  items->emplace_back(0x11, 0x00, 16, 16, false);
  items->emplace_back(0x22, 0x00, 32, 16, false);
  items->emplace_back(0x33, 0x00, 48, 16, false);
  ASSERT_EQ(items->size(), 3u);

  const zelda3::OverworldItem to_delete = items->at(1);
  ASSERT_TRUE(RemoveItemByIdentity(overworld_.get(), to_delete).ok());
  ASSERT_EQ(items->size(), 2u);

  ASSERT_TRUE(zelda3::SaveItems(&rom_, *items).ok());
  auto maps = BuildOverworldMaps(&rom_);
  auto loaded_or = zelda3::LoadItems(&rom_, maps);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status().message();
  const auto& loaded = loaded_or.value();

  ASSERT_EQ(loaded.size(), 2u);
  EXPECT_TRUE(std::any_of(
      loaded.begin(), loaded.end(),
      [](const zelda3::OverworldItem& item) { return item.id_ == 0x11; }));
  EXPECT_TRUE(std::any_of(
      loaded.begin(), loaded.end(),
      [](const zelda3::OverworldItem& item) { return item.id_ == 0x33; }));
  EXPECT_FALSE(std::any_of(
      loaded.begin(), loaded.end(),
      [](const zelda3::OverworldItem& item) { return item.id_ == 0x22; }));
}

}  // namespace
}  // namespace yaze::editor

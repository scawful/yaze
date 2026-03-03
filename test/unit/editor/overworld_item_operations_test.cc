#include "app/editor/overworld/entity_operations.h"

#include <algorithm>
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

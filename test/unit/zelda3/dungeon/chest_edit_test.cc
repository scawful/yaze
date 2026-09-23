#include "zelda3/dungeon/chest_edit.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

RoomObject Object(int16_t id, uint8_t layer = 0) {
  return RoomObject(id, 8, 8, CanonicalRoomObjectSize(id, 0), layer);
}

void ExpectContents(const std::vector<chest_data>& actual,
                    std::initializer_list<chest_data> expected) {
  ASSERT_EQ(actual.size(), expected.size());
  size_t index = 0;
  for (const auto& chest : expected) {
    EXPECT_EQ(actual[index].id, chest.id) << index;
    EXPECT_EQ(actual[index].size, chest.size) << index;
    ++index;
  }
}

TEST(ChestEditTest, FindsChestOrdinalsInEncodedOrder) {
  const std::vector<RoomObject> objects = {Object(0xFB1, 2), Object(0xF99, 0),
                                           Object(0x21, 1), Object(0xF99, 1),
                                           Object(0xF99, 0)};
  EXPECT_EQ(ChestIndexForObject(objects, 0), 3u);
  EXPECT_EQ(ChestIndexForObject(objects, 1), 0u);
  EXPECT_EQ(ChestIndexForObject(objects, 3), 2u);
  EXPECT_EQ(ChestIndexForObject(objects, 4), 1u);
  EXPECT_FALSE(ChestIndexForObject(objects, 2));
  EXPECT_FALSE(ChestIndexForObject(objects, objects.size()));
}

TEST(ChestEditTest, IgnoresFixedOpenMinigameAndSpecialTableObjects) {
  std::vector<RoomObject> objects = {Object(0xF9A), Object(0xFB2),
                                     Object(0xFF5), Object(0xF99),
                                     Object(0xFB1)};
  objects[3].set_options(ObjectOption::Torch);
  objects[4].set_options(ObjectOption::Block);
  for (size_t index = 0; index < objects.size(); ++index) {
    EXPECT_FALSE(ChestIndexForObject(objects, index));
  }
  EXPECT_TRUE(ValidateChestObjectMapping(objects, {}).ok());
}

TEST(ChestEditTest, AddsSmallAndBigChestsWithDefaultReward) {
  const auto result = PlanChestObjectEdit(
      {}, {}, {Object(0xFB1, 2), Object(0xF99)}, {std::nullopt, std::nullopt});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x34, false}, {0x34, true}});
}

TEST(ChestEditTest, InsertionBeforeExistingChestPreservesExistingReward) {
  const std::vector<RoomObject> before = {Object(0xFB1, 2)};
  const auto result = PlanChestObjectEdit(
      before, {{0xE7, true}}, {before[0], Object(0xF99)}, {0, std::nullopt});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x34, false}, {0xE7, true}});
}

TEST(ChestEditTest, DeletesOnlyMatchingChestAndPreservesOthers) {
  const std::vector<RoomObject> before = {Object(0xFB1, 2), Object(0xF99),
                                          Object(0xF99, 1)};
  const auto result =
      PlanChestObjectEdit(before, {{0x11, false}, {0x22, false}, {0x33, true}},
                          {before[0], before[2]}, {0, 2});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x22, false}, {0x33, true}});
}

TEST(ChestEditTest, DeletingAllChestsProducesEmptyContents) {
  const auto result =
      PlanChestObjectEdit({Object(0xF99)}, {{0x34, false}}, {}, {});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->empty());
}

TEST(ChestEditTest, ChangingChestTypePreservesUnknownReward) {
  for (const bool big : {false, true}) {
    const auto result =
        PlanChestObjectEdit({Object(big ? 0xF99 : 0xFB1)}, {{0xFE, !big}},
                            {Object(big ? 0xFB1 : 0xF99)}, {0});
    ASSERT_TRUE(result.ok()) << result.status();
    ExpectContents(*result, {{0xFE, big}});
  }
}

TEST(ChestEditTest, ReorderingIdenticalObjectsUsesExplicitOrigins) {
  const std::vector<RoomObject> before = {Object(0xF99), Object(0xF99)};
  const auto result = PlanChestObjectEdit(
      before, {{0x11, false}, {0x22, false}}, before, {1, 0});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x22, false}, {0x11, false}});
}

TEST(ChestEditTest, ChangingStoredListPermutesContentsByIdentity) {
  const std::vector<RoomObject> before = {Object(0xF99), Object(0xFB1, 1)};
  auto after = before;
  after[0].layer_ = RoomObject::BG3;
  const auto result =
      PlanChestObjectEdit(before, {{0x11, false}, {0x22, true}}, after, {0, 1});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x22, true}, {0x11, false}});
}

TEST(ChestEditTest, DuplicateCopiesSourceRewardRatherThanEqualNeighbor) {
  const std::vector<RoomObject> before = {Object(0xF99), Object(0xF99)};
  const auto result =
      PlanChestObjectEdit(before, {{0x11, false}, {0x22, false}},
                          {before[0], before[1], before[0]}, {0, 1, 1});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x11, false}, {0x22, false}, {0x22, false}});
}

TEST(ChestEditTest, ClipboardOverridesSurviveNewOriginsAndEncodedOrder) {
  const auto result = PlanChestObjectEdit(
      {}, {}, {Object(0xFB1, 2), Object(0xF99)}, {std::nullopt, std::nullopt},
      {chest_data{0xEF, true}, chest_data{0xFF, false}});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0xFF, false}, {0xEF, true}});
}

TEST(ChestEditTest, OverrideCanReplaceExistingReward) {
  const std::vector<RoomObject> objects = {Object(0xF99)};
  const auto result = PlanChestObjectEdit(objects, {{0x34, false}}, objects,
                                          {0}, {chest_data{0x03, false}});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x03, false}});
}

TEST(ChestEditTest, OrdinaryObjectBecomingChestGetsDefaultContents) {
  const auto result =
      PlanChestObjectEdit({Object(0x21)}, {}, {Object(0xF99)}, {0});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x34, false}});
}

TEST(ChestEditTest, ChestBecomingOpenDecorationDropsItsRecord) {
  const auto result = PlanChestObjectEdit({Object(0xF99)}, {{0x34, false}},
                                          {Object(0xF9A)}, {0});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->empty());
}

TEST(ChestEditTest, RejectsMissingAndExtraSourceIndices) {
  EXPECT_FALSE(PlanChestObjectEdit({}, {}, {Object(0xF99)}, {}).ok());
  EXPECT_FALSE(PlanChestObjectEdit({}, {}, {}, {std::nullopt}).ok());
}

TEST(ChestEditTest, RejectsInvalidOriginEvenForOrdinaryObjects) {
  EXPECT_FALSE(PlanChestObjectEdit({}, {}, {Object(0x21)}, {0}).ok());
}

TEST(ChestEditTest, RejectsUnalignedOrWrongTypeOverrides) {
  EXPECT_FALSE(
      PlanChestObjectEdit({}, {}, {}, {}, {chest_data{0x34, false}}).ok());
  EXPECT_FALSE(PlanChestObjectEdit({}, {}, {Object(0xF99)}, {std::nullopt},
                                   {chest_data{0x34, true}})
                   .ok());
  EXPECT_FALSE(PlanChestObjectEdit({}, {}, {Object(0x21)}, {std::nullopt},
                                   {chest_data{0x34, false}})
                   .ok());
}

TEST(ChestEditTest, RejectsInvalidChestOrLockObjectList) {
  for (const int16_t id : {int16_t{0xF99}, int16_t{0xF98}}) {
    EXPECT_FALSE(
        PlanChestObjectEdit({}, {}, {Object(id, 3)}, {std::nullopt}).ok());
  }
}

TEST(ChestEditTest, RejectsStructuralChangesWithMissingOrOrphanedRecords) {
  const std::vector<RoomObject> before = {Object(0xF99)};
  for (const std::vector<chest_data>& chests :
       {std::vector<chest_data>{},
        std::vector<chest_data>{{0x11, false}, {0x22, false}}}) {
    EXPECT_FALSE(PlanChestObjectEdit(before, chests, {}, {}).ok());
    EXPECT_FALSE(
        PlanChestObjectEdit(before, chests, {before[0], before[0]}, {0, 0})
            .ok());
  }
}

TEST(ChestEditTest, RejectsStructuralChangesWithMismatchedRecordType) {
  EXPECT_FALSE(
      PlanChestObjectEdit({Object(0xF99)}, {{0x34, true}}, {}, {}).ok());
  EXPECT_FALSE(
      ValidateChestObjectMapping({Object(0xF99)}, {{0x34, true}}).ok());
}

TEST(ChestEditTest, OrdinaryEditsPreserveExistingMappingMismatch) {
  const std::vector<RoomObject> before = {Object(0xF99), Object(0x21)};
  const std::vector<RoomObject> after = {before[0]};
  for (const std::vector<chest_data>& chests :
       {std::vector<chest_data>{}, std::vector<chest_data>{{0xFE, true}},
        std::vector<chest_data>{{0x11, false}, {0x22, false}}}) {
    const auto result = PlanChestObjectEdit(before, chests, after, {0});
    ASSERT_TRUE(result.ok()) << result.status();
    ASSERT_EQ(result->size(), chests.size());
    for (size_t index = 0; index < chests.size(); ++index) {
      EXPECT_EQ((*result)[index].id, chests[index].id);
      EXPECT_EQ((*result)[index].size, chests[index].size);
    }
  }
}

TEST(ChestEditTest, MovingChestWithoutChangingOrdinalPreservesMissingRecords) {
  const std::vector<RoomObject> before = {Object(0xF99)};
  auto after = before;
  after[0].set_x(16);
  const auto result = PlanChestObjectEdit(before, {}, after, {0});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->empty());
}

TEST(ChestEditTest, AcceptsSixCombinedChestsAndLocks) {
  std::vector<RoomObject> after = {Object(0xF99), Object(0xFB1)};
  for (size_t index = 0; index < 4; ++index) {
    after.push_back(Object(0xF98));
  }
  const auto result = PlanChestObjectEdit(
      {}, {}, after, std::vector<std::optional<size_t>>(after.size()));
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x34, false}, {0x34, true}});
}

TEST(ChestEditTest, RejectsSeventhChestOrSharedLockSlot) {
  for (const int16_t id : {int16_t{0xF99}, int16_t{0xF98}}) {
    std::vector<RoomObject> after(6, Object(0xF99));
    after.push_back(Object(id));
    const auto result = PlanChestObjectEdit(
        {}, {}, after, std::vector<std::optional<size_t>>(after.size()));
    EXPECT_EQ(result.status().code(), absl::StatusCode::kResourceExhausted);
  }
}

TEST(ChestEditTest, RejectsChestAfterLockInEncodedOrder) {
  // Vector order looks safe, but list 1 is encoded after the lock in list 0.
  const std::vector<RoomObject> after = {Object(0xF99, 1), Object(0xF98)};
  const auto result =
      PlanChestObjectEdit({}, {}, after, {std::nullopt, std::nullopt});
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST(ChestEditTest, AcceptsChestBeforeLockDespiteEditorVectorOrder) {
  const std::vector<RoomObject> after = {Object(0xF98, 2), Object(0xF99)};
  const auto result =
      PlanChestObjectEdit({}, {}, after, {std::nullopt, std::nullopt});
  ASSERT_TRUE(result.ok()) << result.status();
  ExpectContents(*result, {{0x34, false}});
}

TEST(ChestEditTest, ExcludesDecorationsAndSpecialObjectsFromSharedLimit) {
  std::vector<RoomObject> after(6, Object(0xF99));
  after.push_back(Object(0xF9A));
  after.push_back(Object(0xFB2));
  after.push_back(Object(0xFF5));
  after.push_back(Object(0xF98));
  after.back().set_options(ObjectOption::Torch);
  const auto result = PlanChestObjectEdit(
      {}, {}, after, std::vector<std::optional<size_t>>(after.size()));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->size(), 6u);
}

TEST(ChestEditTest, LockOnlyAdditionCannotBypassSharedSlotLimit) {
  const std::vector<RoomObject> before(6, Object(0xF98));
  auto after = before;
  after.push_back(Object(0xF98));
  const auto result =
      PlanChestObjectEdit(before, {}, after, {0, 1, 2, 3, 4, 5, std::nullopt});
  EXPECT_EQ(result.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST(ChestEditTest, LockReorderingCannotBypassChestOrderingCheck) {
  const std::vector<RoomObject> before = {Object(0xF99), Object(0xF98)};
  const auto result = PlanChestObjectEdit(before, {{0x34, false}},
                                          {before[1], before[0]}, {1, 0});
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST(ChestEditTest, MovingLockAcrossObjectListsCannotBypassOrderingCheck) {
  const std::vector<RoomObject> before = {Object(0xF99, 1), Object(0xF98, 2)};
  auto after = before;
  after[1].layer_ = RoomObject::BG1;
  const auto result =
      PlanChestObjectEdit(before, {{0x34, false}}, after, {0, 1});
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
}

TEST(ChestEditTest, UnrelatedWallEditPreservesExistingLockOrdering) {
  const std::vector<RoomObject> before = {Object(0xF98), Object(0xF99),
                                          Object(0x21)};
  const auto result =
      PlanChestObjectEdit(before, {}, {before[0], before[1]}, {0, 1});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->empty());
}

}  // namespace
}  // namespace yaze::zelda3

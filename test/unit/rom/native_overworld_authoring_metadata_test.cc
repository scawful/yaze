#include "rom/native_overworld_authoring_metadata.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "core/source_artifact_publisher.h"

namespace yaze::rom {
namespace {

NativeOverworldAuthoringMetadata Placements() {
  NativeOverworldAuthoringMetadata data;
  data.next_placement_id = 0x6003;
  // Source order is significant; these witnesses are structural examples,
  // not proof that a production sprite stream has been bound.
  data.placements = {
      {0x5001, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{1, 0xB0, 0, {14, 23, 0x52}}},
      {0x5002, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{1, 0xB0, 1, {21, 15, 0x52}}},
      {0x5005, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{1, 0xB0, 2, {12, 40, 0x52}}},
      {0x5003, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{1, 0xB0, 3, {43, 13, 0x52}}},
      {0x5004, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{1, 0xB0, 4, {45, 37, 0x52}}},
      {0x6001, NativePlacementState::kPending,
       NativeAuthoringDoorPlacement{0xB0, 21, 9, 0x0B, std::nullopt}},
      {0x6002, NativePlacementState::kPending,
       NativeAuthoringReturnPlacement{0xB0, 22, 11, 0x83, std::nullopt}}};
  return data;
}

NativeOverworldAuthoringMetadata BoundEndpointPlacements() {
  auto data = Placements();
  data.schema_version = 2;
  data.placements[5].state = NativePlacementState::kBound;
  data.placements[5].value =
      NativeAuthoringDoorPlacement{0xB0, 22, 8, 0x0B, 53};
  data.placements[6].state = NativePlacementState::kBound;
  data.placements[6].value =
      NativeAuthoringReturnPlacement{0xB0, 22, 11, 0x83, 41};
  return data;
}

TEST(NativeOverworldAuthoringMetadataTest,
     Schema2BoundEndpointsMatchPythonGolden) {
  const auto data = BoundEndpointPlacements();
  const auto encoded = EncodeNativeOverworldAuthoringMetadata(data, 192);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_EQ(
      core::ComputeSourceArtifactSha256(
          {reinterpret_cast<const char*>(encoded->data()), encoded->size()}),
      "b61a938b1dbccf52ea0d29f222b340f0cabea30f4a8214023ce3af133759e278");
  EXPECT_EQ((*encoded)[8], 2);
  EXPECT_EQ((*encoded)[32 * 6 + 7], 3);
  EXPECT_EQ((*encoded)[32 * 6 + 14], 53);  // Decimal trigger row, not $53.
  EXPECT_EQ((*encoded)[32 * 7 + 14], 41);  // Decimal return row, not $41.
  const auto decoded = DecodeNativeOverworldAuthoringMetadata(*encoded, 192);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, data);
  const auto reencoded = EncodeNativeOverworldAuthoringMetadata(*decoded, 192);
  ASSERT_TRUE(reencoded.ok()) << reencoded.status();
  EXPECT_EQ(*reencoded, *encoded);

  const auto version1 =
      EncodeNativeOverworldAuthoringMetadata(Placements(), 192);
  ASSERT_TRUE(version1.ok());
  EXPECT_TRUE(std::equal(version1->begin() + 32, version1->begin() + 32 * 6,
                         encoded->begin() + 32));
}

TEST(NativeOverworldAuthoringMetadataTest,
     Schema2RequiresExplicitVersionAndBothBoundEndpointFields) {
  auto data = BoundEndpointPlacements();
  data.schema_version = 1;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  for (uint16_t schema : {0, 3, 0xFFFF}) {
    data.schema_version = schema;
    EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  }
  for (int missing = 0; missing < 4; ++missing) {
    SCOPED_TRACE(missing);
    data = BoundEndpointPlacements();
    auto& door =
        std::get<NativeAuthoringDoorPlacement>(data.placements[5].value);
    auto& back =
        std::get<NativeAuthoringReturnPlacement>(data.placements[6].value);
    if (missing == 0)
      door.entrance_id.reset();
    if (missing == 1)
      door.trigger_row.reset();
    if (missing == 2)
      back.room_id.reset();
    if (missing == 3)
      back.return_row.reset();
    EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  }
  data = BoundEndpointPlacements();
  auto& door = std::get<NativeAuthoringDoorPlacement>(data.placements[5].value);
  auto& back =
      std::get<NativeAuthoringReturnPlacement>(data.placements[6].value);
  door.entrance_id = 0;
  door.trigger_row = 0;
  back.room_id = 0;
  back.return_row = 0;
  const auto zero = EncodeNativeOverworldAuthoringMetadata(data, 192);
  ASSERT_TRUE(zero.ok()) << zero.status();
  EXPECT_EQ((*zero)[32 * 6 + 7], 3);
  EXPECT_EQ((*zero)[32 * 7 + 7], 3);
  const auto decoded = DecodeNativeOverworldAuthoringMetadata(*zero, 192);
  ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(*decoded, data);
}

TEST(NativeOverworldAuthoringMetadataTest,
     Schema2LiveEndpointRowsHaveOneOwnerPerKind) {
  for (size_t source : {5, 6}) {
    SCOPED_TRACE(source);
    auto data = BoundEndpointPlacements();
    auto duplicate = data.placements[source];
    duplicate.id = data.next_placement_id++;
    data.placements.push_back(duplicate);
    EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
    data.placements.back().state = NativePlacementState::kPending;
    EXPECT_TRUE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
    data.placements.back().state = NativePlacementState::kDeleted;
    EXPECT_TRUE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
    data.placements[source].state = NativePlacementState::kDeleted;
    data.placements.back().state = NativePlacementState::kBound;
    EXPECT_TRUE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  }
  auto data = BoundEndpointPlacements();
  std::get<NativeAuthoringDoorPlacement>(data.placements[5].value).trigger_row =
      41;
  EXPECT_TRUE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     LoadedSchemaIsPreservedWithoutImplicitUpgrade) {
  const auto original = Placements();
  EXPECT_EQ(original.schema_version, 1);
  const auto version1 = EncodeNativeOverworldAuthoringMetadata(original, 192);
  ASSERT_TRUE(version1.ok());
  auto version2 = original;
  version2.schema_version = 2;
  const auto upgraded = EncodeNativeOverworldAuthoringMetadata(version2, 192);
  ASSERT_TRUE(upgraded.ok());
  auto expected = *version1;
  expected[8] = 2;
  EXPECT_EQ(*upgraded, expected);
  for (const auto& bytes : {*version1, *upgraded}) {
    const auto decoded = DecodeNativeOverworldAuthoringMetadata(bytes, 192);
    ASSERT_TRUE(decoded.ok());
    const auto encoded = EncodeNativeOverworldAuthoringMetadata(*decoded, 192);
    ASSERT_TRUE(encoded.ok());
    EXPECT_EQ(*encoded, bytes);
  }
  EXPECT_FALSE(
      ValidateNativeAuthoringSpriteTransition(original, version2, 192).ok());
  EXPECT_FALSE(
      ValidateNativeAuthoringSpriteTransition(version2, original, 192).ok());
  auto moved = BoundEndpointPlacements();
  const auto before = moved;
  std::get<NativeAuthoringSpritePlacement>(moved.placements[0].value)
      .packed[1]++;
  EXPECT_TRUE(ValidateNativeAuthoringSpriteTransition(before, moved, 192).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     RoundTripPreservesOrderAndOptionals) {
  auto data = Placements();
  const auto encoded = EncodeNativeOverworldAuthoringMetadata(data, 192);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_EQ(
      core::ComputeSourceArtifactSha256(
          {reinterpret_cast<const char*>(encoded->data()), encoded->size()}),
      "f4a8e2cd3c1fede254883fc646b0faa6d9c93389cb85b71c74e8fe58a7597781");
  EXPECT_EQ((*encoded)[32], 1);  // ID5001, little endian.
  EXPECT_EQ((*encoded)[33], 0x50);
  EXPECT_EQ((*encoded)[32 * 6 + 7], 1);  // Door target, no runtime row.
  const auto decoded = DecodeNativeOverworldAuthoringMetadata(*encoded, 192);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, data);
  // Present zero and absent are different values, including row zero.
  auto& door = std::get<NativeAuthoringDoorPlacement>(data.placements[5].value);
  door.entrance_id = 0;
  door.trigger_row = 0;
  auto& back =
      std::get<NativeAuthoringReturnPlacement>(data.placements[6].value);
  back.room_id.reset();
  back.return_row = 0;
  const auto zero = EncodeNativeOverworldAuthoringMetadata(data, 192);
  ASSERT_TRUE(zero.ok()) << zero.status();
  EXPECT_EQ((*zero)[32 * 6 + 7], 3);
  EXPECT_EQ((*zero)[32 * 7 + 7], 2);
  ASSERT_TRUE(DecodeNativeOverworldAuthoringMetadata(*zero, 192).ok());
  EXPECT_EQ(*DecodeNativeOverworldAuthoringMetadata(*zero, 192), data);
}

TEST(NativeOverworldAuthoringMetadataTest,
     SpriteTransitionsKeepIdentityAndPermitUndo) {
  const auto before = Placements();
  auto moved = before;
  std::get<NativeAuthoringSpritePlacement>(moved.placements[0].value)
      .packed[1]++;
  EXPECT_TRUE(ValidateNativeAuthoringSpriteTransition(before, moved, 192).ok());
  auto deleted = before;
  deleted.placements[1].state = NativePlacementState::kDeleted;
  EXPECT_TRUE(
      ValidateNativeAuthoringSpriteTransition(before, deleted, 192).ok());
  EXPECT_TRUE(
      ValidateNativeAuthoringSpriteTransition(deleted, before, 192).ok());
  auto added = before;
  added.placements.push_back(
      {0x6003, NativePlacementState::kDeleted,
       NativeAuthoringSpritePlacement{1, 0xB0, 5, {1, 1, 0x52}}});
  added.next_placement_id = 0x6004;
  EXPECT_TRUE(ValidateNativeAuthoringSpriteTransition(before, added, 192).ok());
  auto redone = added;
  redone.placements.back().state = NativePlacementState::kBound;
  EXPECT_TRUE(ValidateNativeAuthoringSpriteTransition(added, redone, 192).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     SpriteTransitionsRefuseUnownedMetadataReplacement) {
  auto before = Placements();
  before.next_placement_id = 0x7000;
  for (int variant = 0; variant < 8; ++variant) {
    SCOPED_TRACE(variant);
    auto after = before;
    if (variant == 0)
      after.placements.pop_back();
    if (variant == 1)
      std::swap(after.placements[0], after.placements[1]);
    if (variant == 2)
      std::get<NativeAuthoringDoorPlacement>(after.placements[5].value)
          .entrance_id = 0;
    if (variant == 3)
      after.next_placement_id = 0x6003;
    if (variant == 4) {
      after.placements.push_back(
          {0x6003, NativePlacementState::kDeleted,
           NativeAuthoringSpritePlacement{1, 0xB0, 5, {1, 1, 0x52}}});
    }
    if (variant == 5) {
      after.placements.push_back({0x7000, NativePlacementState::kPending,
                                  NativeAuthoringDoorPlacement{
                                      0xB0, 1, 1, std::nullopt, std::nullopt}});
      after.next_placement_id = 0x7001;
    }
    if (variant == 6) {
      after.placements[1].state = NativePlacementState::kDeleted;
      std::get<NativeAuthoringSpritePlacement>(after.placements[1].value)
          .ordinal = 0;
    }
    if (variant == 7)
      std::get<NativeAuthoringSpritePlacement>(after.placements[0].value)
          .phase = 0;
    ASSERT_TRUE(EncodeNativeOverworldAuthoringMetadata(after, 192).ok());
    EXPECT_FALSE(
        ValidateNativeAuthoringSpriteTransition(before, after, 192).ok());
  }
}

TEST(NativeOverworldAuthoringMetadataTest, TombstoneAllowsReusedLiveOrdinal) {
  auto data = Placements();
  data.placements[1].state = NativePlacementState::kDeleted;
  std::get<NativeAuthoringSpritePlacement>(data.placements[2].value).ordinal =
      1;
  const auto encoded = EncodeNativeOverworldAuthoringMetadata(data, 192);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_EQ(*DecodeNativeOverworldAuthoringMetadata(*encoded, 192), data);
  data.placements[1].state = NativePlacementState::kBound;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
}

TEST(NativeOverworldAuthoringMetadataTest, BindingIncludesPhaseAndOwner) {
  NativeOverworldAuthoringMetadata data;
  data.next_placement_id = 5;
  for (uint8_t phase = 0; phase < 3; ++phase)
    data.placements.push_back(
        {static_cast<uint32_t>(phase + 1), NativePlacementState::kBound,
         NativeAuthoringSpritePlacement{phase, 0x9F, 127, {0, 0, 0}}});
  data.placements.push_back(
      {4, NativePlacementState::kBound,
       NativeAuthoringSpritePlacement{0, 0x80, 127, {0, 0, 0}}});
  const auto encoded = EncodeNativeOverworldAuthoringMetadata(data, 160);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  const auto decoded = DecodeNativeOverworldAuthoringMetadata(*encoded, 160);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, data);
}

TEST(NativeOverworldAuthoringMetadataTest, RejectsDuplicateAndReusedIdentity) {
  auto data = Placements();
  data.placements[1].id = data.placements[0].id;
  data.placements[1].state = NativePlacementState::kDeleted;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  data = Placements();
  data.placements[0].id = 0;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  data = Placements();
  data.placements.back().state = NativePlacementState::kDeleted;
  data.next_placement_id = data.placements.back().id;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  data = Placements();
  data.next_placement_id = 0;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
}

TEST(NativeOverworldAuthoringMetadataTest, FullCapacityIncludesTombstones) {
  NativeOverworldAuthoringMetadata data;
  for (uint32_t i = 1; i <= kNativeAuthoringMetadataSlots; ++i) {
    data.placements.push_back(
        {i, NativePlacementState::kDeleted,
         NativeAuthoringSpritePlacement{2, 0x9F, 127, {0xFE, 0xFF, 0xFF}}});
  }
  data.next_placement_id = std::numeric_limits<uint32_t>::max();
  const auto encoded = EncodeNativeOverworldAuthoringMetadata(data, 160);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  EXPECT_EQ(*DecodeNativeOverworldAuthoringMetadata(*encoded, 160), data);
  data.placements.push_back(
      {256, NativePlacementState::kPending, NativeAuthoringDoorPlacement{}});
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 160).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     RejectsInvalidHeaderAndReservedBytes) {
  const auto encoded =
      EncodeNativeOverworldAuthoringMetadata(Placements(), 192);
  ASSERT_TRUE(encoded.ok());
  for (size_t offset : std::array<size_t, 12>{
           0, 8, 10, 12, 14, 20, 31, 32 + 13, 32 * 6 + 11, 32 * 7 + 31, 32 * 8,
           kNativeAuthoringMetadataSize - 1}) {
    SCOPED_TRACE(offset);
    auto bad = *encoded;
    bad[offset] ^= 0x80;
    EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(bad, 192).ok());
  }
  EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(
                   std::span<const uint8_t>(*encoded).first(8191), 192)
                   .ok());
  EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(*encoded, 161).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     RejectsInvalidKindsStatesAndBindings) {
  const auto encoded =
      EncodeNativeOverworldAuthoringMetadata(Placements(), 192);
  ASSERT_TRUE(encoded.ok());
  for (const auto [offset, value] :
       std::array<std::pair<size_t, uint8_t>, 13>{{{32 + 4, 0},
                                                   {32 + 4, 4},
                                                   {32 + 5, 0},
                                                   {32 + 5, 2},
                                                   {32 + 5, 4},
                                                   {32 + 6, 3},
                                                   {32 + 7, 1},
                                                   {32 + 8, 192},
                                                   {32 + 9, 128},
                                                   {32 + 10, 0xFF},
                                                   {32 * 6 + 5, 1},
                                                   {32 * 6 + 6, 0},
                                                   {32 * 6 + 7, 4}}}) {
    auto bad = *encoded;
    bad[offset] = value;
    EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(bad, 192).ok())
        << offset;
  }
  EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(*encoded, 160).ok());
}

TEST(NativeOverworldAuthoringMetadataTest,
     RejectsInvalidPendingEndpointFields) {
  const auto encoded =
      EncodeNativeOverworldAuthoringMetadata(Placements(), 192);
  ASSERT_TRUE(encoded.ok());
  auto rejects = [&](size_t offset, uint8_t value) {
    auto bad = *encoded;
    bad[offset] = value;
    EXPECT_FALSE(DecodeNativeOverworldAuthoringMetadata(bad, 192).ok());
  };
  rejects(32 * 6 + 9, 32);
  rejects(32 * 6 + 10, 32);
  rejects(32 * 6 + 13, 1);  // Entrance ID cannot exceed FF.
  rejects(32 * 6 + 7, 0);   // Nonzero target without presence.
  rejects(32 * 6 + 14, 1);  // Nonzero row without presence.
  auto data = Placements();
  std::get<NativeAuthoringDoorPlacement>(data.placements[5].value).trigger_row =
      256;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  std::get<NativeAuthoringDoorPlacement>(data.placements[5].value).trigger_row =
      255;
  std::get<NativeAuthoringReturnPlacement>(data.placements[6].value)
      .return_row = 0x4F;
  EXPECT_FALSE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
  std::get<NativeAuthoringReturnPlacement>(data.placements[6].value)
      .return_row = 0x4E;
  EXPECT_TRUE(EncodeNativeOverworldAuthoringMetadata(data, 192).ok());
}

}  // namespace
}  // namespace yaze::rom

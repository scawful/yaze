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

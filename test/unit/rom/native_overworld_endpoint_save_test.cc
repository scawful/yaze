#include <gtest/gtest.h>

#include <set>

#include "absl/cleanup/cleanup.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "framework/rom_save_fault.h"
#include "rom/native_overworld_endpoint_io.h"
#include "rom/rom.h"
#include "rom/write_fence.h"
#include "unique_temp_path.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"

namespace yaze {
namespace {
using test::native_overworld::NativeDirectoryFixture;

NativeDirectoryFixture EndpointFixture() {
  NativeDirectoryFixture fixture;
  fixture.SeedNativeSpritePrefix();
  fixture.bytes[zelda3::kOverworldEntranceExpandedFlagPos] = 0xB8;
  return fixture;
}

rom::NativeOverworldEndpointEdits EndpointBatch() {
  rom::NativeOverworldEndpointEdits edits;
  edits.entrances.push_back({0, {}, {0xA0, 0x492, 0x0B}});
  rom::NativeOverworldReturnRow row;
  row.room = 0x22;
  row.map = 0xA0;
  row.player_x = 0x140;
  row.player_y = 0x980;
  row.camera_x = 0x123;
  row.camera_y = 0x234;
  row.scroll_mod_x = 3;
  row.door_type_1 = 0x5678;
  edits.returns.push_back({0, {}, row});
  return edits;
}

rom::NativeOverworldAuthoringMetadata EndpointMetadata() {
  rom::NativeOverworldAuthoringMetadata metadata;
  metadata.next_placement_id = 3;
  metadata.placements = {
      {1, rom::NativePlacementState::kPending,
       rom::NativeAuthoringDoorPlacement{0xA0, 9, 9, 0x0B, std::nullopt}},
      {2, rom::NativePlacementState::kPending,
       rom::NativeAuthoringReturnPlacement{0xA0, 20, 24, 0x22, std::nullopt}}};
  return metadata;
}

rom::NativeOverworldAuthoringMetadata BindEndpointMetadata(
    rom::NativeOverworldAuthoringMetadata metadata) {
  metadata.schema_version = 2;
  for (auto& placement : metadata.placements) {
    if (auto* door =
            std::get_if<rom::NativeAuthoringDoorPlacement>(&placement.value)) {
      placement.state = rom::NativePlacementState::kBound;
      door->trigger_row = 0;
    } else if (auto* back = std::get_if<rom::NativeAuthoringReturnPlacement>(
                   &placement.value)) {
      placement.state = rom::NativePlacementState::kBound;
      back->return_row = 0;
    }
  }
  return metadata;
}

TEST(NativeEndpointSaveTest, BindingRequiresSelectedRowsAndSurvivesFileRetry) {
  auto fixture = EndpointFixture();
  fixture.UpgradeToRevision4();
  const auto pending = EndpointMetadata();
  fixture.AddAuthoringMetadata(pending);
  const auto bound = BindEndpointMetadata(pending);
  const auto edits = EndpointBatch();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                             {}, &pending, &bound)
                   .ok());
  auto only_door = edits;
  only_door.returns.clear();
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                             {}, &pending, &bound, nullptr,
                                             &only_door)
                   .ok());
  EXPECT_EQ(document.vector(), fixture.bytes);
  ASSERT_TRUE(document
                  .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                            {}, &pending, &bound, nullptr,
                                            &edits)
                  .ok());
  ASSERT_TRUE(
      rom::ValidateNativeEndpointPlacementWitness(document.vector(), bound)
          .ok());
  const auto path = test::UniqueTempPath("native_endpoint_binding", ".sfc");
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove(path); });
  test::native_overworld::WriteBytes(path, fixture.bytes);
  const auto candidate = document.vector();
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(document.SaveToFile({.filename = path.string()}).ok());
  }
  EXPECT_EQ(test::native_overworld::ReadBytes(path), fixture.bytes);
  EXPECT_EQ(document.vector(), candidate);
  ASSERT_TRUE(document.SaveToFile({.filename = path.string()}).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  EXPECT_EQ(reopened.vector(), candidate);
  const auto metadata_pc = fixture.Entry(412).pc_address;
  const auto decoded = rom::DecodeNativeOverworldAuthoringMetadata(
      {reopened.data() + metadata_pc, rom::kNativeAuthoringMetadataSize}, 192);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(*decoded, bound);
  EXPECT_TRUE(
      rom::ValidateNativeEndpointPlacementWitness(reopened.vector(), *decoded)
          .ok());
}

TEST(NativeEndpointSaveTest, BoundWitnessRefusesStaleRowsAndUnownedChanges) {
  auto fixture = EndpointFixture();
  fixture.UpgradeToRevision4();
  const auto pending = EndpointMetadata();
  fixture.AddAuthoringMetadata(pending);
  const auto bound = BindEndpointMetadata(pending);
  const auto edits = EndpointBatch();
  const auto sealed = rom::SealNativeOverworldTerrain(
      fixture.bytes, fixture.bytes, {}, {}, &pending, &bound, nullptr, &edits);
  ASSERT_TRUE(sealed.ok()) << sealed.status();
  auto wrong = bound;
  std::get<rom::NativeAuthoringDoorPlacement>(wrong.placements[0].value)
      .entrance_id = 0x0C;
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(fixture.bytes, fixture.bytes, {},
                                               {}, &pending, &wrong, nullptr,
                                               &edits)
                   .ok());
  auto moved = edits;
  moved.entrances[0].expected = edits.entrances[0].value;
  moved.entrances[0].value.position += 2;
  moved.returns.clear();
  // Existing bindings remain enforced even if the caller supplies no metadata.
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(
                   *sealed, *sealed, {}, {}, nullptr, nullptr, nullptr, &moved)
                   .ok());
  auto repaired = bound;
  std::get<rom::NativeAuthoringDoorPlacement>(repaired.placements[0].value).x++;
  EXPECT_TRUE(rom::SealNativeOverworldTerrain(*sealed, *sealed, {}, {}, &bound,
                                              &repaired, nullptr, &moved)
                  .ok());
  auto stale = *sealed;
  stale[zelda3::kOverworldEntranceEntranceId] ^= 1;
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(stale, stale).ok());
  auto downgraded = pending;
  EXPECT_FALSE(rom::ValidateNativeAuthoringEndpointTransition(bound, downgraded,
                                                              192, moved)
                   .ok());
  auto unrequested_upgrade = pending;
  unrequested_upgrade.schema_version = 2;
  EXPECT_FALSE(rom::ValidateNativeAuthoringEndpointTransition(
                   pending, unrequested_upgrade, 192, edits)
                   .ok());
}

TEST(NativeEndpointSaveTest,
     ChildPageBindingsRebaseAndKeepSubTileReturnPixels) {
  auto fixture = EndpointFixture();
  fixture.UpgradeToRevision4();
  auto pending = fixture.SeedCanonicalPlacementRows();
  auto bound = BindEndpointMetadata(pending);
  auto& door =
      std::get<rom::NativeAuthoringDoorPlacement>(bound.placements[5].value);
  door.map = 0xB9;
  door.x = 2;
  door.y = 3;
  auto& back =
      std::get<rom::NativeAuthoringReturnPlacement>(bound.placements[6].value);
  back.map = 0xB9;
  back.x = 2;
  back.y = 3;
  back.room_id = 0x22;
  auto edits = EndpointBatch();
  edits.entrances[0].value = {0xB0, (35 << 7) | (34 << 1), 0x0B};
  edits.returns[0].value.map = 0xB0;
  edits.returns[0].value.player_x = 0x22B;
  edits.returns[0].value.player_y = 0xE37;
  const auto sealed = rom::SealNativeOverworldTerrain(
      fixture.bytes, fixture.bytes, {}, {}, &pending, &bound, nullptr, &edits);
  ASSERT_TRUE(sealed.ok()) << sealed.status();
  EXPECT_EQ((*sealed)[zelda3::OWExitXPlayer], 0x2B);
  EXPECT_EQ((*sealed)[zelda3::OWExitYPlayer], 0x37);
  EXPECT_TRUE(rom::ValidateNativeEndpointPlacementWitness(*sealed, bound).ok());
  door.map = 0xB1;  // Same parent, wrong physical page for this packed row.
  EXPECT_FALSE(
      rom::ValidateNativeEndpointPlacementWitness(*sealed, bound).ok());
}

TEST(NativeEndpointSaveTest,
     SelectedRowsPublishAtomicallyAndPreserveOtherBytes) {
  auto fixture = EndpointFixture();
  const auto before = fixture.bytes;
  const auto edits = EndpointBatch();
  const auto plan = rom::PlanNativeOverworldEndpointSave(before, edits);
  ASSERT_TRUE(plan.ok()) << plan.status();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(before).ok());
  ASSERT_TRUE(document
                  .CommitNativeTerrainImage(before, before, {}, {}, nullptr,
                                            nullptr, nullptr, &edits)
                  .ok());
  std::set<size_t> allowed;
  for (const auto& write : *plan)
    for (size_t i = 0; i < write.bytes.size(); ++i)
      allowed.insert(write.address + i);
  for (size_t i = 0; i < before.size(); ++i)
    if (!allowed.count(i))
      ASSERT_EQ(document.vector()[i], before[i]) << i;
  auto entrances = zelda3::LoadEntrances(&document, {});
  ASSERT_TRUE(entrances.ok()) << entrances.status();
  EXPECT_EQ((*entrances)[0].map_id_, 0xA0);
  EXPECT_EQ((*entrances)[0].map_pos_, 0x492);
  auto returns = zelda3::LoadExits(&document);
  ASSERT_TRUE(returns.ok()) << returns.status();
  EXPECT_EQ((*returns)[0].room_id_, 0x22);
  EXPECT_EQ((*returns)[0].map_id_, 0xA0);
  EXPECT_EQ((*returns)[0].x_camera_, 0x123);
  EXPECT_EQ((*returns)[0].door_type_1_, 0x5678);
  // Retrying an old plan after publication cannot silently overwrite it.
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(document.vector(),
                                             document.vector(), {}, {}, nullptr,
                                             nullptr, nullptr, &edits)
                   .ok());
}

TEST(NativeEndpointSaveTest, StaleReturnRefusesWholeBatchWithoutMutation) {
  auto fixture = EndpointFixture();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  auto edits = EndpointBatch();
  edits.returns[0].expected.camera_y = 1;
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                             {}, nullptr, nullptr, nullptr,
                                             &edits)
                   .ok());
  EXPECT_EQ(document.vector(), fixture.bytes);
}

TEST(NativeEndpointSaveTest, FileFailureRetryAndFreshReopenKeepSelectedRows) {
  const auto fixture = EndpointFixture();
  const auto path = test::UniqueTempPath("native_endpoint", ".sfc");
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove(path); });
  test::native_overworld::WriteBytes(path, fixture.bytes);
  Rom document;
  ASSERT_TRUE(document.LoadFromFile(path.string()).ok());
  const auto edits = EndpointBatch();
  ASSERT_TRUE(document
                  .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                            {}, nullptr, nullptr, nullptr,
                                            &edits)
                  .ok());
  const auto pending = document.vector();
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(document.SaveToFile({.filename = path.string()}).ok());
  }
  EXPECT_EQ(test::native_overworld::ReadBytes(path), fixture.bytes);
  EXPECT_EQ(document.vector(), pending);
  ASSERT_TRUE(document.SaveToFile({.filename = path.string()}).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  EXPECT_EQ(reopened.vector(), document.vector());
  auto returns = zelda3::LoadExits(&reopened);
  ASSERT_TRUE(returns.ok()) << returns.status();
  EXPECT_EQ((*returns)[0].map_id_, 0xA0);
  EXPECT_EQ((*returns)[0].y_player_, 0x980);
}

TEST(NativeEndpointSaveTest, DuplicateRowsAndInvalidCoordinatesAreRefused) {
  const auto fixture = EndpointFixture();
  auto edits = EndpointBatch();
  edits.entrances.push_back(edits.entrances[0]);
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  edits = EndpointBatch();
  edits.returns.push_back(edits.returns[0]);
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  edits = EndpointBatch();
  edits.entrances[0].value.position |= 1;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  edits = EndpointBatch();
  edits.returns[0].value.player_x = 0xFFFF;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  edits = EndpointBatch();
  edits.entrances[0].value.map_word = 192;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
}

TEST(NativeEndpointSaveTest, CapacityAndExpandedHoleFlagFollowRuntimeTables) {
  auto fixture = EndpointFixture();
  auto edits = EndpointBatch();
  edits.returns.clear();
  edits.entrances[0].row = 129;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  fixture.bytes[zelda3::kOverworldEntranceExpandedFlagPos] = 0;
  edits.entrances[0].row = 255;
  edits.entrances[0].value.map_word |= 0x8000;
  const auto plan = rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->size(), 3);
  EXPECT_EQ((*plan)[0].address, zelda3::kOverworldEntranceMapExpanded + 510);
  EXPECT_EQ((*plan)[0].bytes, (std::vector<uint8_t>{0xA0, 0x80}));
  edits.entrances[0].row = 256;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
}

TEST(NativeEndpointSaveTest, SmallWideTallAndLargeBoundsUseProposedGeometry) {
  auto fixture = EndpointFixture();
  for (uint8_t shape = 0; shape < 4; ++shape) {
    auto final = fixture.bytes;
    final[fixture.Entry(30).pc_address + 0xA0] = shape;
    const uint16_t width = shape == 1 || shape == 2 ? 64 : 32;
    const uint16_t height = shape == 1 || shape == 3 ? 64 : 32;
    auto edits = EndpointBatch();
    edits.entrances[0].value.position = ((height - 1) * 64 + width - 1) * 2;
    edits.returns[0].value.player_x = width * 16 - 1;
    edits.returns[0].value.player_y = 0x800 + height * 16 - 1;
    EXPECT_TRUE(
        rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits, final).ok())
        << static_cast<int>(shape);
    edits.returns[0].value.player_y++;
    EXPECT_FALSE(
        rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits, final).ok());
    edits.returns.clear();
    if (width == 32) {
      edits.entrances[0].value.position = 32 * 2;
      EXPECT_FALSE(
          rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits, final)
              .ok());
    }
    if (height == 32) {
      edits.entrances[0].value.position = 32 * 64 * 2;
      EXPECT_FALSE(
          rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits, final)
              .ok());
    }
  }
  auto final = fixture.bytes;
  final[fixture.Entry(7).pc_address + 0xA0] = 0x98;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes,
                                                    EndpointBatch(), final)
                   .ok());
  auto former_child = fixture;
  for (int map : {0x98, 0x99, 0xA0, 0xA1}) {
    former_child.bytes[former_child.Entry(7).pc_address + map] = 0x98;
    former_child.bytes[former_child.Entry(30).pc_address + map] = 1;
  }
  former_child.Seal();
  ASSERT_EQ(rom::InspectLyraLayout(former_child.bytes.data(),
                                   former_child.bytes.size())
                .presence,
            rom::LyraLayoutPresence::kValid);
  final = former_child.bytes;
  for (int map : {0x98, 0x99, 0xA0, 0xA1}) {
    final[former_child.Entry(7).pc_address + map] = map;
    final[former_child.Entry(30).pc_address + map] = 0;
  }
  EXPECT_TRUE(rom::PlanNativeOverworldEndpointSave(former_child.bytes,
                                                   EndpointBatch(), final)
                  .ok());
}

TEST(NativeEndpointSaveTest, DeletedRowsPreserveInactivePayload) {
  const auto fixture = EndpointFixture();
  auto edits = EndpointBatch();
  edits.entrances[0].value = {0xFFFF, 0xFFFF, 0xFF};
  auto& row = edits.returns[0].value;
  row.map = 0xFF;
  row.room = row.player_x = row.player_y = 0xFFFF;
  EXPECT_TRUE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  row.room = 0x22;
  EXPECT_TRUE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
  row.player_x = 0x140;
  row.map = 0xA0;
  EXPECT_FALSE(rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits).ok());
}

TEST(NativeEndpointSaveTest, SpecialReturnAuxiliaryBytesNeedDestinationChange) {
  auto fixture = EndpointFixture();
  fixture.bytes[zelda3::OWExitDoorPosition] = 0x55;
  auto edits = EndpointBatch();
  edits.entrances.clear();
  edits.returns[0].value.room = 0x180;
  auto plan = rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->back().address, zelda3::OWExitDoorPosition);
  EXPECT_EQ(plan->back().bytes, (std::vector<uint8_t>{0xA0}));
  // A scroll-only edit must preserve a deliberately different auxiliary map.
  fixture.Word(zelda3::OWExitRoomId, 0x180);
  fixture.bytes[zelda3::OWExitMapId] = 0xA0;
  edits.returns[0].expected.room = 0x180;
  edits.returns[0].expected.map = 0xA0;
  plan = rom::PlanNativeOverworldEndpointSave(fixture.bytes, edits);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->size(), 13);
}

TEST(NativeEndpointSaveTest, DirectoryAliasingAndChildAreaAreRefused) {
  auto fixture = EndpointFixture();
  fixture.bytes[fixture.Entry(7).pc_address + 0xA0] = 0x98;
  fixture.Seal();
  EXPECT_FALSE(
      rom::PlanNativeOverworldEndpointSave(fixture.bytes, EndpointBatch())
          .ok());
  fixture = EndpointFixture();
  fixture.Entry(100).pc_address = zelda3::kOverworldEntranceMap;
  fixture.Entry(100).snes_address =
      NativeDirectoryFixture::Snes(zelda3::kOverworldEntranceMap);
  fixture.Seal();
  EXPECT_FALSE(
      rom::PlanNativeOverworldEndpointSave(fixture.bytes, EndpointBatch())
          .ok());
}

TEST(NativeEndpointSaveTest, RawWritesReadOnlyAndWriteFenceRemainEnforced) {
  const auto fixture = EndpointFixture();
  const auto edits = EndpointBatch();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  auto raw = fixture.bytes;
  raw[zelda3::kOverworldEntranceMap] = 0xA0;
  EXPECT_FALSE(document.CommitNativeTerrainImage(fixture.bytes, raw).ok());
  {
    ScopedRomReadOnly read_only(document);
    EXPECT_FALSE(document
                     .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                               {}, nullptr, nullptr, nullptr,
                                               &edits)
                     .ok());
  }
  {
    rom::WriteFence fence;
    ASSERT_TRUE(fence.Allow(0, zelda3::OWExitRoomId, "before returns").ok());
    rom::ScopedWriteFence guard(&document, &fence);
    EXPECT_FALSE(document
                     .CommitNativeTerrainImage(fixture.bytes, fixture.bytes, {},
                                               {}, nullptr, nullptr, nullptr,
                                               &edits)
                     .ok());
  }
  EXPECT_EQ(document.vector(), fixture.bytes);
}
}  // namespace
}  // namespace yaze

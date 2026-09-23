#include "app/editor/dungeon/dungeon_room_edit.h"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {
namespace {

struct MetadataCase {
  RoomMetadataField field;
  int maximum;
  bool object_header;
  bool changes_rendering;
};

constexpr std::array<MetadataCase, 16> kMetadataCases = {{
    {RoomMetadataField::kLayout, 7, true, true},
    {RoomMetadataField::kBlockset, 0x51, false, true},
    {RoomMetadataField::kFloor1, 0xF, true, true},
    {RoomMetadataField::kFloor2, 0xF, true, true},
    {RoomMetadataField::kPalette, 0x47, false, true},
    {RoomMetadataField::kSpriteset, 0x4F, false, true},
    {RoomMetadataField::kMessage, 0xFFF, false, false},
    {RoomMetadataField::kBg2, 8, false, true},
    {RoomMetadataField::kEffect, 7, false, true},
    {RoomMetadataField::kCollision, 4, false, false},
    {RoomMetadataField::kTag1, 0x3F, false, true},
    {RoomMetadataField::kTag2, 0x3F, false, true},
    {RoomMetadataField::kHolewarp, 0xFF, false, false},
    {RoomMetadataField::kStaircaseRoom, 0xFF, false, false},
    {RoomMetadataField::kStaircasePlane, 2, false, false},
    {RoomMetadataField::kPitPlane, 2, false, false},
}};

void ExpectMetadataDirtyDomain(const zelda3::Room& room, bool object_header) {
  EXPECT_EQ(room.header_dirty(), !object_header);
  EXPECT_EQ(room.object_stream_header_dirty(), object_header);
  EXPECT_FALSE(room.object_stream_dirty());
  EXPECT_FALSE(room.sprites_dirty());
  EXPECT_FALSE(room.chests_dirty());
  EXPECT_FALSE(room.pot_items_dirty());
  EXPECT_FALSE(room.torches_dirty());
  EXPECT_FALSE(room.blocks_dirty());
  EXPECT_FALSE(room.custom_collision_dirty());
  EXPECT_FALSE(room.water_fill_dirty());
}

class DungeonRoomMetadataFieldTest
    : public ::testing::TestWithParam<MetadataCase> {};

TEST_P(DungeonRoomMetadataFieldTest, EditAndRestoreOnlyDirtyTheirSaveDomain) {
  zelda3::Room room;
  room.ClearSaveDirtyState();
  const auto original = room.CaptureMetadataSnapshot();
  const uint64_t original_revision = room.composite_source_revision();
  const auto& param = GetParam();

  ASSERT_TRUE(ApplyRoomMetadataEdit(room, {param.field, param.maximum}).ok());
  const auto edited = room.CaptureMetadataSnapshot();
  EXPECT_NE(edited, original);
  ExpectMetadataDirtyDomain(room, param.object_header);
  EXPECT_EQ(room.composite_source_revision() != original_revision,
            param.changes_rendering);

  // Restoring after a save must mark the correct storage domain dirty again.
  room.ClearSaveDirtyState();
  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  ExpectMetadataDirtyDomain(room, param.object_header);

  room.ClearSaveDirtyState();
  room.RestoreMetadataSnapshot(edited);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), edited);
  ExpectMetadataDirtyDomain(room, param.object_header);
}

TEST_P(DungeonRoomMetadataFieldTest,
       NoOpLeavesHistoryInputsAndDirtyStateAlone) {
  zelda3::Room room;
  const auto original = room.CaptureMetadataSnapshot();
  const uint64_t original_revision = room.composite_source_revision();

  ASSERT_TRUE(ApplyRoomMetadataEdit(room, {GetParam().field, 0}).ok());
  room.RestoreMetadataSnapshot(original);

  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  EXPECT_FALSE(room.HasUnsavedChanges());
  EXPECT_EQ(room.composite_source_revision(), original_revision);
}

TEST_P(DungeonRoomMetadataFieldTest, RejectsInvalidValuesBeforeMutation) {
  zelda3::Room room;
  const auto original = room.CaptureMetadataSnapshot();
  const uint64_t original_revision = room.composite_source_revision();
  for (int value :
       {-1, GetParam().maximum + 1, std::numeric_limits<int>::max()}) {
    SCOPED_TRACE(value);
    const auto status = ApplyRoomMetadataEdit(room, {GetParam().field, value});
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
    EXPECT_FALSE(room.HasUnsavedChanges());
    EXPECT_EQ(room.composite_source_revision(), original_revision);
  }
}

INSTANTIATE_TEST_SUITE_P(AllFields, DungeonRoomMetadataFieldTest,
                         ::testing::ValuesIn(kMetadataCases));

TEST(DungeonRoomMetadataTest, AppliesEachFieldToItsCorrespondingProperty) {
  zelda3::Room room;
  const std::array<RoomMetadataEdit, 15> edits = {{
      {RoomMetadataField::kLayout, 6},
      {RoomMetadataField::kBlockset, 0x42},
      {RoomMetadataField::kFloor1, 9},
      {RoomMetadataField::kFloor2, 10},
      {RoomMetadataField::kPalette, 0x40},
      {RoomMetadataField::kSpriteset, 0x4F},
      {RoomMetadataField::kMessage, 0xABC},
      {RoomMetadataField::kBg2, 6},
      {RoomMetadataField::kEffect, 4},
      {RoomMetadataField::kCollision, 2},
      {RoomMetadataField::kTag1, 0x1B},
      {RoomMetadataField::kTag2, 0x33},
      {RoomMetadataField::kHolewarp, 0xEE},
      {RoomMetadataField::kStaircaseRoom, 0xFA, 3},
      {RoomMetadataField::kStaircasePlane, 2, 3},
  }};
  for (const auto& edit : edits) {
    ASSERT_TRUE(ApplyRoomMetadataEdit(room, edit).ok());
  }
  EXPECT_EQ(room.layout_id(), 6);
  EXPECT_EQ(room.blockset(), 0x42);
  EXPECT_EQ(room.floor1(), 9);
  EXPECT_EQ(room.floor2(), 10);
  EXPECT_EQ(room.palette(), 0x40);
  EXPECT_EQ(room.spriteset(), 0x4F);
  EXPECT_EQ(room.message_id(), 0xABC);
  EXPECT_EQ(static_cast<int>(room.bg2()), 6);
  EXPECT_EQ(room.layer2_mode(), 6);
  EXPECT_EQ(room.layer_merging(), zelda3::LayerMerge06);
  EXPECT_EQ(static_cast<int>(room.effect()), 4);
  EXPECT_EQ(static_cast<int>(room.collision()), 2);
  EXPECT_EQ(static_cast<int>(room.tag1()), 0x1B);
  EXPECT_EQ(static_cast<int>(room.tag2()), 0x33);
  EXPECT_EQ(room.holewarp(), 0xEE);
  EXPECT_EQ(room.staircase_room(3), 0xFA);
  EXPECT_EQ(room.staircase_plane(3), 2);
  EXPECT_EQ(room.staircase_room(0), 0);
  EXPECT_EQ(room.staircase_plane(0), 0);
}

TEST(DungeonRoomMetadataTest, RejectsUnknownFieldAndInvalidSlots) {
  zelda3::Room room;
  const auto original = room.CaptureMetadataSnapshot();
  const std::array<RoomMetadataEdit, 6> invalid_edits = {{
      {static_cast<RoomMetadataField>(999), 0},
      {RoomMetadataField::kPalette, 1, 1},
      {RoomMetadataField::kStaircaseRoom, 1, -1},
      {RoomMetadataField::kStaircaseRoom, 1, 4},
      {RoomMetadataField::kStaircasePlane, 1, -1},
      {RoomMetadataField::kStaircasePlane, 1, 4},
  }};
  for (const auto& edit : invalid_edits) {
    EXPECT_EQ(ApplyRoomMetadataEdit(room, edit).code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
    EXPECT_FALSE(room.HasUnsavedChanges());
  }
}

TEST(DungeonRoomMetadataTest, StaircaseSlotsRemainIndependent) {
  zelda3::Room room;
  const auto original = room.CaptureMetadataSnapshot();
  for (int index = 0; index < 4; ++index) {
    ASSERT_TRUE(ApplyRoomMetadataEdit(room, {RoomMetadataField::kStaircaseRoom,
                                             0xFC + index, index})
                    .ok());
    ASSERT_TRUE(ApplyRoomMetadataEdit(room, {RoomMetadataField::kStaircasePlane,
                                             index % 3, index})
                    .ok());
  }
  const auto edited = room.CaptureMetadataSnapshot();
  EXPECT_EQ(edited.staircase_rooms,
            (std::array<uint8_t, 4>{0xFC, 0xFD, 0xFE, 0xFF}));
  EXPECT_EQ(edited.staircase_planes, (std::array<uint8_t, 4>{0, 1, 2, 0}));
  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  room.RestoreMetadataSnapshot(edited);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), edited);
}

TEST(DungeonRoomMetadataTest, RestoresDarkRoomHiddenModeAndRenderStateExactly) {
  zelda3::Room room;
  room.SetLayer2Mode(5);
  room.SetBg2(background2::DarkRoom);
  const auto original = room.CaptureMetadataSnapshot();
  ASSERT_EQ(original.layer2_mode, 5);
  ASSERT_TRUE(original.is_dark);
  ASSERT_TRUE(original.is_light);
  ASSERT_EQ(original.layer_merging, zelda3::LayerMerge08);
  ASSERT_TRUE(ApplyRoomMetadataEdit(room, {RoomMetadataField::kBg2, 2}).ok());
  EXPECT_EQ(room.layer2_mode(), 2);
  const auto edited = room.CaptureMetadataSnapshot();

  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  room.RestoreMetadataSnapshot(edited);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), edited);
}

TEST(DungeonRoomMetadataTest, UnrelatedEditAndRestorePreserveUnknownValues) {
  zelda3::Room room;
  room.SetPalette(0xF4);
  room.SetBlockset(0xF5);
  room.SetSpriteset(0xF6);
  room.SetMessageId(0xFEDC);
  room.SetEffect(static_cast<zelda3::EffectKey>(0xF7));
  room.SetCollision(static_cast<zelda3::CollisionKey>(7));
  room.SetTag1(static_cast<zelda3::TagKey>(0xFA));
  room.SetTag2(static_cast<zelda3::TagKey>(0xFB));
  const auto original = room.CaptureMetadataSnapshot();
  room.ClearSaveDirtyState();

  ASSERT_TRUE(
      ApplyRoomMetadataEdit(room, {RoomMetadataField::kFloor2, 4}).ok());
  auto expected = original;
  expected.floor2 = 4;
  EXPECT_EQ(room.CaptureMetadataSnapshot(), expected);
  EXPECT_FALSE(room.header_dirty());
  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);

  // Undo must also restore unknown values after the field itself was edited.
  ASSERT_TRUE(
      ApplyRoomMetadataEdit(room, {RoomMetadataField::kPalette, 3}).ok());
  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
}

TEST(DungeonRoomMetadataTest,
     RestorePreservesUnrelatedDirtyDomainsAndEntities) {
  zelda3::Room room;
  const auto original = room.CaptureMetadataSnapshot();
  ASSERT_TRUE(
      ApplyRoomMetadataEdit(room, {RoomMetadataField::kPalette, 3}).ok());
  room.MarkSpritesDirty();
  room.MarkChestsDirty();
  room.MarkPotItemsDirty();
  room.MarkTorchesDirty();
  room.MarkBlocksDirty();
  room.MarkObjectStreamDirty();
  room.MarkCustomCollisionDirty();
  room.MarkWaterFillDirty();
  room.GetPotItems().push_back({0x2345, 7});
  room.SetLoaded(true);

  room.RestoreMetadataSnapshot(original);

  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(room.sprites_dirty());
  EXPECT_TRUE(room.chests_dirty());
  EXPECT_TRUE(room.pot_items_dirty());
  EXPECT_TRUE(room.torches_dirty());
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_TRUE(room.object_stream_dirty());
  EXPECT_TRUE(room.custom_collision_dirty());
  EXPECT_TRUE(room.water_fill_dirty());
  EXPECT_TRUE(room.IsLoaded());
  ASSERT_EQ(room.GetPotItems().size(), 1);
  EXPECT_EQ(room.GetPotItems()[0].position, 0x2345);
  EXPECT_EQ(room.GetPotItems()[0].item, 7);
}

TEST(DungeonRoomMetadataTest,
     EditUndoSavePreservesRawRomHeaderBitsAndUnknowns) {
  constexpr int kHeaderTablePc = 0x8000;
  constexpr int kHeaderPc = 0x9000;
  std::vector<uint8_t> data(0x200000, 0);
  const uint32_t table_snes = PcToSnes(kHeaderTablePc);
  const uint32_t header_snes = PcToSnes(kHeaderPc);
  for (int byte = 0; byte < 3; ++byte) {
    data[zelda3::kRoomHeaderPointer + byte] =
        static_cast<uint8_t>(table_snes >> (byte * 8));
  }
  data[zelda3::kRoomHeaderPointerBank] =
      static_cast<uint8_t>(header_snes >> 16);
  data[kHeaderTablePc] = static_cast<uint8_t>(header_snes);
  data[kHeaderTablePc + 1] = static_cast<uint8_t>(header_snes >> 8);
  // Dark room, hidden BG2 mode 5, unknown collision 7 and reserved bit 1.
  const std::array<uint8_t, 14> header = {0xBF, 0xFE, 0xFC, 0xFD, 0xF9,
                                          0xFA, 0xFB, 0xE7, 0xFE, 0xE0,
                                          0xE1, 0xE2, 0xE3, 0xE4};
  std::copy(header.begin(), header.end(), data.begin() + kHeaderPc);
  data[zelda3::kMessagesIdDungeon] = 0xEF;
  data[zelda3::kMessagesIdDungeon + 1] = 0xBE;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  const auto original = room.CaptureMetadataSnapshot();
  const auto original_rom = rom.vector();

  ASSERT_TRUE(ApplyRoomMetadataEdit(room, {RoomMetadataField::kBg2, 3}).ok());
  ASSERT_TRUE(ApplyRoomMetadataEdit(room, {RoomMetadataField::kTag1, 4}).ok());
  EXPECT_EQ(rom.vector(), original_rom);
  ASSERT_TRUE(room.SaveRoomHeader().ok());
  EXPECT_NE(rom.vector(), original_rom);

  room.RestoreMetadataSnapshot(original);
  EXPECT_EQ(room.CaptureMetadataSnapshot(), original);
  ASSERT_TRUE(room.SaveRoomHeader().ok());
  EXPECT_EQ(rom.vector(), original_rom);
  auto reopened = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  EXPECT_EQ(reopened.CaptureMetadataSnapshot(), original);
}

}  // namespace
}  // namespace yaze::editor

#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "core/features.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mocks/mock_rom.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

namespace {

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) prev =
      core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = prev; }
};

void ConfigureMinimalDungeonSave() {
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSaveEntrances = false;
  d.kSavePalettes = false;
}

constexpr int kChestDataPc = 0x110000;
constexpr int kRoom0ObjectPc = 0x100000;
constexpr int kRoom1ObjectPc = 0x100100;
constexpr int kRoom2ObjectPc = 0x100200;
constexpr int kPotRoom0Pc = 0x008000;
constexpr int kPotRoom1Pc = 0x008020;
constexpr int kPotRoom2Pc = 0x008040;
constexpr int kRoom0SpritePc = 0x049000;
constexpr int kRoom1SpritePc = 0x049020;
constexpr int kRoom2SpritePc = 0x049040;
constexpr int kHeaderTablePc = 0x0F6000;
constexpr int kRoom0HeaderPc = 0x114000;
constexpr int kRoom1HeaderPc = 0x114020;
constexpr int kRoom2HeaderPc = 0x114040;

void WriteLongPointer(Rom& rom, int addr, uint32_t snes_addr) {
  rom.mutable_data()[addr + 0] = snes_addr & 0xFF;
  rom.mutable_data()[addr + 1] = (snes_addr >> 8) & 0xFF;
  rom.mutable_data()[addr + 2] = (snes_addr >> 16) & 0xFF;
}

void SetupChestTable(Rom& rom) {
  WriteLongPointer(rom, zelda3::kChestsDataPointer1, PcToSnes(kChestDataPc));
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x00;
  rom.mutable_data()[zelda3::kChestsLengthPointer + 1] = 0x00;
  std::fill_n(rom.mutable_data() + kChestDataPc, 0x100, 0x00);
}

void WriteEmptyObjectStream(Rom& rom, int room_data_pc) {
  rom.mutable_data()[room_data_pc + 0] = 0x00;
  rom.mutable_data()[room_data_pc + 1] = 0x00;
  const std::vector<uint8_t> empty = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  ASSERT_TRUE(rom.WriteVector(room_data_pc + 2, empty).ok());
}

void SetupRoomObjectPointers(Rom& rom) {
  WriteLongPointer(rom, zelda3::kRoomObjectPointer, PcToSnes(0x0F8000));

  WriteLongPointer(rom, 0x0F8000 + 0, PcToSnes(kRoom0ObjectPc));
  WriteLongPointer(rom, 0x0F8000 + 3, PcToSnes(kRoom1ObjectPc));
  WriteLongPointer(rom, 0x0F8000 + 6, PcToSnes(kRoom2ObjectPc));

  WriteEmptyObjectStream(rom, kRoom0ObjectPc);
  WriteEmptyObjectStream(rom, kRoom1ObjectPc);
  WriteEmptyObjectStream(rom, kRoom2ObjectPc);
}

void SetupSpritePointers(Rom& rom) {
  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0SpritePc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1SpritePc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2SpritePc));

  rom.mutable_data()[zelda3::kRoomsSpritePointer + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 5] = (room2_ptr >> 8) & 0xFF;

  for (int sprite_pc : {kRoom0SpritePc, kRoom1SpritePc, kRoom2SpritePc}) {
    rom.mutable_data()[sprite_pc + 0] = 0x00;
    rom.mutable_data()[sprite_pc + 1] = 0xFF;
  }
}

void SeedChestEntry(Rom& rom, int room_id, uint8_t chest_id, bool big) {
  const uint16_t word = static_cast<uint16_t>(room_id) | (big ? 0x8000 : 0);
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x01;
  rom.mutable_data()[zelda3::kChestsLengthPointer + 1] = 0x00;
  rom.mutable_data()[kChestDataPc + 0] = word & 0xFF;
  rom.mutable_data()[kChestDataPc + 1] = (word >> 8) & 0xFF;
  rom.mutable_data()[kChestDataPc + 2] = chest_id;
}

void SetupPotItemTable(Rom& rom) {
  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom0Pc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom1Pc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom2Pc));

  rom.mutable_data()[zelda3::kRoomItemsPointers + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 5] = (room2_ptr >> 8) & 0xFF;

  rom.mutable_data()[kPotRoom0Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom0Pc + 1] = 0xFF;
  rom.mutable_data()[kPotRoom1Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom1Pc + 1] = 0xFF;
  rom.mutable_data()[kPotRoom2Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom2Pc + 1] = 0xFF;
}

void SetupHeaderPointers(Rom& rom) {
  WriteLongPointer(rom, zelda3::kRoomHeaderPointer, PcToSnes(kHeaderTablePc));
  rom.mutable_data()[zelda3::kRoomHeaderPointerBank] =
      (PcToSnes(kRoom0HeaderPc) >> 16) & 0xFF;

  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0HeaderPc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1HeaderPc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2HeaderPc));

  rom.mutable_data()[kHeaderTablePc + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 5] = (room2_ptr >> 8) & 0xFF;
}

void SeedHeaderBytes(Rom& rom, int header_pc,
                     std::initializer_list<uint8_t> bytes, uint16_t message_id,
                     int room_id) {
  std::copy(bytes.begin(), bytes.end(), rom.mutable_data() + header_pc);
  rom.mutable_data()[zelda3::kMessagesIdDungeon + (room_id * 2) + 0] =
      message_id & 0xFF;
  rom.mutable_data()[zelda3::kMessagesIdDungeon + (room_id * 2) + 1] =
      (message_id >> 8) & 0xFF;
}

void SeedPotItemBytes(Rom& rom, int pc_addr,
                      std::initializer_list<uint8_t> bytes) {
  std::copy(bytes.begin(), bytes.end(), rom.mutable_data() + pc_addr);
}

}  // namespace

TEST(DungeonEditorV2RomSafetyTest, SaveUsesRoomIndicesNotInternalIds) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // No rooms are loaded, so SaveRoomData should be a no-op and never write.
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Inject a room with an out-of-range internal ID. Save must not use
  // room.id() for iteration (otherwise it would try SaveRoomData(999)).
  editor->rooms()[5] = zelda3::Room(999, &rom, nullptr);
  editor->rooms()[5].SetLoaded(false);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomBlocksInvalidRoomBeforeWriting) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/70, /*y=*/0,
                                        /*size=*/0, /*layer=*/0));

  auto status = editor->SaveRoom(0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveWritesWaterFillZonesWhenCollisionSavingDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  ASSERT_TRUE(editor->rooms()[0].water_fill_dirty());

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_FALSE(editor->rooms()[0].water_fill_dirty());
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  ASSERT_EQ(zones[0].fill_offsets.size(), 1u);
  EXPECT_EQ(zones[0].fill_offsets[0], 65u);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesChestsWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetChests().push_back(chest_data{0x42, false});
  room.GetChests().push_back(chest_data{0x77, true});
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveChests = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer], 0x02);
  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer + 1], 0x00);

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadChests();
  ASSERT_EQ(reloaded_room.GetChests().size(), 2u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x42);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
  EXPECT_EQ(reloaded_room.GetChests()[1].id, 0x77);
  EXPECT_TRUE(reloaded_room.GetChests()[1].size);
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsChestsWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);
  SeedChestEntry(rom, /*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetChests().push_back(chest_data{0x42, false});

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveChests = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer], 0x01);
  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer + 1], 0x00);

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadChests();
  ASSERT_EQ(reloaded_room.GetChests().size(), 1u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x55);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesPotItemsWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupPotItemTable(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);

  zelda3::PotItem first;
  first.position = 0x1234;
  first.item = 0x56;
  room.GetPotItems().push_back(first);
  room.MarkPotItemsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSavePotItems = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsPotItemsWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupPotItemTable(rom);
  SeedPotItemBytes(rom, kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);

  zelda3::PotItem first;
  first.position = 0x5678;
  first.item = 0x9A;
  room.GetPotItems().push_back(first);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSavePotItems = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveSkipsPotItemsWhenDisabledForSelectedRoom) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupRoomObjectPointers(rom);
  SetupSpritePointers(rom);
  SetupHeaderPointers(rom);
  SetupChestTable(rom);
  SetupPotItemTable(rom);
  SeedPotItemBytes(rom, kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetPotItems().push_back(zelda3::PotItem{0x5678, 0x9A});

  editor->add_room(0);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesRoomHeadersWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.SetBlockset(0x1C);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveRoomHeaders = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  EXPECT_EQ(reloaded_room.palette(), 0x2A);
  EXPECT_EQ(reloaded_room.blockset(), 0x1C);
  EXPECT_EQ(reloaded_room.message_id(), 0x1357);
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsRoomHeadersWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.SetBlockset(0x1C);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveRoomHeaders = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  EXPECT_EQ(reloaded_room.palette(), 0x03);
  EXPECT_EQ(reloaded_room.blockset(), 0x04);
  EXPECT_EQ(reloaded_room.message_id(), 0x1111);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesSkipsCleanLoadedRoomsUntilDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupRoomObjectPointers(rom);
  SetupHeaderPointers(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0] = zelda3::LoadRoomFromRom(&rom, 0);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveRoomHeaders = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetPalette(0x2A);
  EXPECT_FALSE(editor->CollectWriteRanges().empty());
}

TEST(DungeonEditorV2RomSafetyTest, PendingRoomCountTracksRoomDirtyState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EXPECT_EQ(editor->PendingRoomCount(), 0);
  EXPECT_FALSE(editor->HasPendingRoomChanges());

  editor->rooms()[0].SetPalette(0x2A);
  editor->rooms()[1].MarkPotItemsDirty();

  EXPECT_EQ(editor->PendingRoomCount(), 2);
  EXPECT_TRUE(editor->HasPendingRoomChanges());

  editor->FocusRoom(0);
  *editor->mutable_current_room_id() = 0;
  EXPECT_TRUE(editor->CurrentRoomHasPendingChanges());

  editor->rooms()[0].ClearSaveDirtyState();
  editor->rooms()[0].ClearCustomCollisionDirty();
  editor->rooms()[0].ClearWaterFillDirty();
  editor->rooms()[1].ClearSaveDirtyState();
  editor->rooms()[1].ClearCustomCollisionDirty();
  editor->rooms()[1].ClearWaterFillDirty();

  EXPECT_EQ(editor->PendingRoomCount(), 0);
  EXPECT_FALSE(editor->HasPendingRoomChanges());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesDuplicateWaterFillMasksInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(0x01);
  editor->rooms()[1].SetWaterFillTile(/*x=*/2, /*y=*/2, /*filled=*/true);
  editor->rooms()[1].set_water_fill_sram_bit_mask(
      0x01);  // Duplicate on purpose.

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);
  EXPECT_EQ(editor->rooms()[1].water_fill_sram_bit_mask(), 0x02);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  EXPECT_EQ(zones[1].room_id, 1);
  EXPECT_EQ(zones[1].sram_bit_mask, 0x02);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesInvalidWaterFillMaskInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/4, /*y=*/4, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(
      0x03);  // Invalid (not single-bit).

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesWaterFillRegionWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveWaterFillZones = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kWaterFillTableStart));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kWaterFillTableEnd));
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesCustomCollisionRegionsWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 2u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers +
                                  (zelda3::kNumberOfRooms * 3)));
  EXPECT_EQ(ranges[1].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataPosition));
  EXPECT_EQ(ranges[1].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataSoftEnd));
}

TEST(DungeonEditorV2RomSafetyTest, UndoSnapshotLeakDetection) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();

  // Add an initial object to make changes detectable.
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/10, /*y=*/10,
                                        /*size=*/0, /*layer=*/0));

  // First BeginUndoSnapshot should work normally.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after BeginUndoSnapshot";

  // Second BeginUndoSnapshot without FinalizeUndoAction should auto-finalize
  // the first snapshot (preventing leak) and start a new one.
  // This tests that double-Begin is handled gracefully.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should still be true after second "
         "BeginUndoSnapshot";

  // FinalizeUndoAction should clear the flag.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after FinalizeUndoAction";

  // BeginUndoSnapshot after FinalizeUndoAction should work normally again.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after new BeginUndoSnapshot";

  // Clean up pending undo.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after final FinalizeUndoAction";
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUEviction) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 25 viewers (exceeds kMaxCachedViewers = 20)
  for (int i = 0; i < 25; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr) << "Failed to get viewer for room " << i;
  }

  // Verify that viewer count is at most kMaxCachedViewers
  EXPECT_LE(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers))
      << "Viewer cache should not exceed kMaxCachedViewers";

  // Verify LRU ordering: most recent rooms (20-24) should still be cached
  for (int i = 20; i < 25; ++i) {
    EXPECT_TRUE(editor->room_viewers_.Contains(i))
        << "Recent room " << i << " should still be cached";
  }

  // Verify oldest rooms (0-4) were evicted
  int evicted_count = 0;
  for (int i = 0; i < 5; ++i) {
    if (!editor->room_viewers_.Contains(i)) {
      evicted_count++;
    }
  }
  EXPECT_GT(evicted_count, 0) << "Some old rooms should have been evicted";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheNeverEvictsActiveRooms) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Mark room 0 as active
  editor->active_rooms_.push_back(0);

  // Get viewer for room 0
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create 25 more viewers (should trigger eviction)
  for (int i = 1; i < 26; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  // Verify room 0 is still cached (it's active, so should not be evicted)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Active room 0 should never be evicted";

  // Verify the same viewer instance is returned
  EXPECT_EQ(editor->GetViewerForRoom(0), viewer0)
      << "Active room viewer should remain the same instance";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUAccessOrderUpdate) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 20 viewers (exactly at limit)
  for (int i = 0; i < 20; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  EXPECT_EQ(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers));

  // Access room 0 again (should move it to the back of LRU)
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create one more viewer (should evict room 1, not room 0)
  auto* viewer20 = editor->GetViewerForRoom(20);
  ASSERT_NE(viewer20, nullptr);

  // Verify room 0 is still cached (was recently accessed)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Recently accessed room 0 should not be evicted";

  // Verify room 1 was evicted (oldest in LRU after room 0 was accessed)
  EXPECT_FALSE(editor->room_viewers_.Contains(1))
      << "Room 1 should have been evicted as the oldest";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

}  // namespace yaze::editor

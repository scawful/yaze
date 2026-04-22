#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3::test {

class DungeonEditorSystemTest : public ::testing::Test {
 protected:
  static constexpr int kRoom0ObjectPc = 0x100000;
  static constexpr int kRoom1ObjectPc = 0x100100;
  static constexpr int kRoom2ObjectPc = 0x100200;

  static constexpr int kRoom0SpritePc = 0x049000;
  static constexpr int kRoom1SpritePc = 0x049020;
  static constexpr int kRoom2SpritePc = 0x049040;

  static constexpr int kHeaderTablePc = 0x0F6000;
  static constexpr int kRoom0HeaderPc = 0x114000;
  static constexpr int kRoom1HeaderPc = 0x114020;
  static constexpr int kRoom2HeaderPc = 0x114040;

  static constexpr int kChestDataPc = 0x110000;
  static constexpr int kPotRoom0Pc = 0x008000;
  static constexpr int kPotRoom1Pc = 0x008020;
  static constexpr int kPotRoom2Pc = 0x008040;

  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

    SetupRoomObjectPointers();
    SetupSpritePointers();
    SetupHeaderPointers();
    SetupChestTable();
    SetupPotItemTable();
  }

  void WriteLongPointer(int addr, uint32_t snes_addr) {
    rom_->mutable_data()[addr + 0] = snes_addr & 0xFF;
    rom_->mutable_data()[addr + 1] = (snes_addr >> 8) & 0xFF;
    rom_->mutable_data()[addr + 2] = (snes_addr >> 16) & 0xFF;
  }

  void WriteEmptyObjectStream(int room_data_pc) {
    rom_->mutable_data()[room_data_pc + 0] = 0x00;
    rom_->mutable_data()[room_data_pc + 1] = 0x00;
    const std::vector<uint8_t> empty = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
    };
    ASSERT_TRUE(rom_->WriteVector(room_data_pc + 2, empty).ok());
  }

  void SetupRoomObjectPointers() {
    WriteLongPointer(kRoomObjectPointer, PcToSnes(0x0F8000));

    WriteLongPointer(0x0F8000 + 0, PcToSnes(kRoom0ObjectPc));
    WriteLongPointer(0x0F8000 + 3, PcToSnes(kRoom1ObjectPc));
    WriteLongPointer(0x0F8000 + 6, PcToSnes(kRoom2ObjectPc));

    WriteEmptyObjectStream(kRoom0ObjectPc);
    WriteEmptyObjectStream(kRoom1ObjectPc);
    WriteEmptyObjectStream(kRoom2ObjectPc);
  }

  void SetupSpritePointers() {
    const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0SpritePc));
    const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1SpritePc));
    const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2SpritePc));

    rom_->mutable_data()[kRoomsSpritePointer + 0] = room0_ptr & 0xFF;
    rom_->mutable_data()[kRoomsSpritePointer + 1] = (room0_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomsSpritePointer + 2] = room1_ptr & 0xFF;
    rom_->mutable_data()[kRoomsSpritePointer + 3] = (room1_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomsSpritePointer + 4] = room2_ptr & 0xFF;
    rom_->mutable_data()[kRoomsSpritePointer + 5] = (room2_ptr >> 8) & 0xFF;

    for (int sprite_pc : {kRoom0SpritePc, kRoom1SpritePc, kRoom2SpritePc}) {
      rom_->mutable_data()[sprite_pc + 0] = 0x00;
      rom_->mutable_data()[sprite_pc + 1] = 0xFF;
    }
  }

  void SetupHeaderPointers() {
    WriteLongPointer(kRoomHeaderPointer, PcToSnes(kHeaderTablePc));
    rom_->mutable_data()[kRoomHeaderPointerBank] =
        (PcToSnes(kRoom0HeaderPc) >> 16) & 0xFF;

    const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0HeaderPc));
    const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1HeaderPc));
    const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2HeaderPc));

    rom_->mutable_data()[kHeaderTablePc + 0] = room0_ptr & 0xFF;
    rom_->mutable_data()[kHeaderTablePc + 1] = (room0_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kHeaderTablePc + 2] = room1_ptr & 0xFF;
    rom_->mutable_data()[kHeaderTablePc + 3] = (room1_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kHeaderTablePc + 4] = room2_ptr & 0xFF;
    rom_->mutable_data()[kHeaderTablePc + 5] = (room2_ptr >> 8) & 0xFF;
  }

  void SetupChestTable() {
    WriteLongPointer(kChestsDataPointer1, PcToSnes(kChestDataPc));
    rom_->mutable_data()[kChestsLengthPointer] = 0x00;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    std::fill_n(rom_->mutable_data() + kChestDataPc, 0x100, 0x00);
  }

  void SetupPotItemTable() {
    const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom0Pc));
    const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom1Pc));
    const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom2Pc));

    rom_->mutable_data()[kRoomItemsPointers + 0] = room0_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 1] = (room0_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 2] = room1_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 3] = (room1_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 4] = room2_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 5] = (room2_ptr >> 8) & 0xFF;

    for (int pc : {kPotRoom0Pc, kPotRoom1Pc, kPotRoom2Pc}) {
      rom_->mutable_data()[pc + 0] = 0xFF;
      rom_->mutable_data()[pc + 1] = 0xFF;
    }
  }

  void SeedChestEntry(int room_id, uint8_t chest_id, bool big) {
    const uint16_t word = static_cast<uint16_t>(room_id) | (big ? 0x8000 : 0);
    rom_->mutable_data()[kChestsLengthPointer] = 0x01;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    rom_->mutable_data()[kChestDataPc + 0] = word & 0xFF;
    rom_->mutable_data()[kChestDataPc + 1] = (word >> 8) & 0xFF;
    rom_->mutable_data()[kChestDataPc + 2] = chest_id;
  }

  void SeedPotItemBytes(int pc_addr, std::initializer_list<uint8_t> bytes) {
    std::copy(bytes.begin(), bytes.end(), rom_->mutable_data() + pc_addr);
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(DungeonEditorSystemTest, SetCurrentRoomBindsObjectEditorToManagedRoom) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());
  ASSERT_TRUE(system.SetCurrentRoom(1).ok());

  auto object_editor = system.GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  EXPECT_EQ(object_editor->GetRoom().id(), 1);
  EXPECT_TRUE(object_editor->GetRoom().IsLoaded());
}

TEST_F(DungeonEditorSystemTest, SaveRoomPersistsExternalRoomObjectsAndChests) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());

  Room external_room = LoadRoomFromRom(rom_.get(), 0);
  system.SetExternalRoom(&external_room);

  auto object_editor = system.GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  ASSERT_EQ(object_editor->GetRoom().id(), 0);

  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x0F, 0).ok());
  external_room.GetChests().push_back(chest_data{0x42, false});

  ASSERT_TRUE(system.SaveRoom(0).ok());

  Room reloaded = LoadRoomFromRom(rom_.get(), 0);
  ASSERT_EQ(reloaded.GetTileObjects().size(), 1u);
  ASSERT_EQ(reloaded.GetChests().size(), 1u);
  EXPECT_EQ(reloaded.GetChests()[0].id, 0x42);
  EXPECT_FALSE(reloaded.GetChests()[0].size);
}

TEST_F(DungeonEditorSystemTest, SaveRoomPersistsExternalRoomPotItemsAndHeader) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());

  Room external_room = LoadRoomFromRom(rom_.get(), 0);
  system.SetExternalRoom(&external_room);

  external_room.GetPotItems().push_back(PotItem{0x1234, 0x56});
  external_room.SetPalette(0x2A);
  external_room.SetMessageId(0x1357);

  ASSERT_TRUE(system.SaveRoom(0).ok());

  Room reloaded = LoadRoomFromRom(rom_.get(), 0);
  ASSERT_EQ(reloaded.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded.GetPotItems()[0].item, 0x56);
  EXPECT_EQ(reloaded.palette(), 0x2A);
  EXPECT_EQ(reloaded.message_id(), 0x1357);
}

TEST_F(DungeonEditorSystemTest, SaveRoomPreservesOtherRoomIndexedData) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());

  SeedChestEntry(/*room_id=*/1, /*chest_id=*/0x77, /*big=*/true);
  SeedPotItemBytes(kPotRoom1Pc, {0x78, 0x56, 0x9A, 0xFF, 0xFF});

  Room external_room = LoadRoomFromRom(rom_.get(), 0);
  system.SetExternalRoom(&external_room);

  external_room.GetChests().push_back(chest_data{0x21, false});
  external_room.GetPotItems().push_back(PotItem{0x1234, 0x56});

  ASSERT_TRUE(system.SaveRoom(0).ok());

  Room room0 = LoadRoomFromRom(rom_.get(), 0);
  ASSERT_EQ(room0.GetChests().size(), 1u);
  EXPECT_EQ(room0.GetChests()[0].id, 0x21);
  EXPECT_FALSE(room0.GetChests()[0].size);
  ASSERT_EQ(room0.GetPotItems().size(), 1u);
  EXPECT_EQ(room0.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(room0.GetPotItems()[0].item, 0x56);

  Room room1_chests_only(1, rom_.get());
  room1_chests_only.LoadChests();
  ASSERT_EQ(room1_chests_only.GetChests().size(), 1u);
  EXPECT_EQ(room1_chests_only.GetChests()[0].id, 0x77);
  EXPECT_TRUE(room1_chests_only.GetChests()[0].size);

  Room room1 = LoadRoomFromRom(rom_.get(), 1);
  ASSERT_EQ(room1.GetPotItems().size(), 1u);
  EXPECT_EQ(room1.GetPotItems()[0].position, 0x5678);
  EXPECT_EQ(room1.GetPotItems()[0].item, 0x9A);
}

TEST_F(DungeonEditorSystemTest, SaveDungeonPersistsAllCachedRooms) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());

  ASSERT_TRUE(system.SetCurrentRoom(0).ok());
  auto object_editor = system.GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  ASSERT_TRUE(object_editor->InsertObject(5, 5, 0x10, 0x0F, 0).ok());
  object_editor->GetMutableRoom()->GetChests().push_back(
      chest_data{0x21, false});

  ASSERT_TRUE(system.SetCurrentRoom(1).ok());
  ASSERT_EQ(object_editor->GetRoom().id(), 1);
  ASSERT_TRUE(object_editor->InsertObject(10, 10, 0x20, 0x0F, 0).ok());
  object_editor->GetMutableRoom()->GetChests().push_back(
      chest_data{0x77, true});

  ASSERT_TRUE(system.SaveDungeon().ok());

  Room room0_chests_only(0, rom_.get());
  room0_chests_only.LoadChests();
  ASSERT_EQ(room0_chests_only.GetChests().size(), 1u);

  Room room1_chests_only(1, rom_.get());
  room1_chests_only.LoadChests();
  ASSERT_EQ(room1_chests_only.GetChests().size(), 1u);

  Room room0 = LoadRoomFromRom(rom_.get(), 0);
  ASSERT_EQ(room0.GetTileObjects().size(), 1u);
  ASSERT_EQ(room0.GetChests().size(), 1u);
  EXPECT_EQ(room0.GetChests()[0].id, 0x21);
  EXPECT_FALSE(room0.GetChests()[0].size);

  Room room1 = LoadRoomFromRom(rom_.get(), 1);
  ASSERT_EQ(room1.GetTileObjects().size(), 1u);
  ASSERT_EQ(room1.GetChests().size(), 1u);
  EXPECT_EQ(room1.GetChests()[0].id, 0x77);
  EXPECT_TRUE(room1.GetChests()[0].size);
}

TEST_F(DungeonEditorSystemTest,
       SaveDungeonPersistsPotItemsAndHeadersAcrossCachedRooms) {
  DungeonEditorSystem system(rom_.get());
  ASSERT_TRUE(system.Initialize().ok());

  ASSERT_TRUE(system.SetCurrentRoom(0).ok());
  auto object_editor = system.GetObjectEditor();
  ASSERT_NE(object_editor, nullptr);
  object_editor->GetMutableRoom()->GetPotItems().push_back(
      PotItem{0x1234, 0x56});
  object_editor->GetMutableRoom()->SetPalette(0x11);
  object_editor->GetMutableRoom()->SetMessageId(0x2468);

  ASSERT_TRUE(system.SetCurrentRoom(1).ok());
  ASSERT_EQ(object_editor->GetRoom().id(), 1);
  object_editor->GetMutableRoom()->GetPotItems().push_back(
      PotItem{0x5678, 0x9A});
  object_editor->GetMutableRoom()->SetPalette(0x22);
  object_editor->GetMutableRoom()->SetMessageId(0x1357);

  ASSERT_TRUE(system.SaveDungeon().ok());

  Room room0 = LoadRoomFromRom(rom_.get(), 0);
  ASSERT_EQ(room0.GetPotItems().size(), 1u);
  EXPECT_EQ(room0.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(room0.GetPotItems()[0].item, 0x56);
  EXPECT_EQ(room0.palette(), 0x11);
  EXPECT_EQ(room0.message_id(), 0x2468);

  Room room1 = LoadRoomFromRom(rom_.get(), 1);
  ASSERT_EQ(room1.GetPotItems().size(), 1u);
  EXPECT_EQ(room1.GetPotItems()[0].position, 0x5678);
  EXPECT_EQ(room1.GetPotItems()[0].item, 0x9A);
  EXPECT_EQ(room1.palette(), 0x22);
  EXPECT_EQ(room1.message_id(), 0x1357);
}

}  // namespace yaze::zelda3::test

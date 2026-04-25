#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonSaveTest : public ::testing::Test {
 protected:
  static constexpr int kChestDataPc = 0x110000;
  static constexpr int kPotRoom0Pc = 0x008000;
  static constexpr int kPotRoom1Pc = 0x008020;
  static constexpr int kPotRoom2Pc = 0x008040;
  static constexpr int kPitDataPc = 0x112000;
  static constexpr int kBlocksRegion1Pc = 0x113000;
  static constexpr int kBlocksRegion2Pc = 0x113080;
  static constexpr int kBlocksRegion3Pc = 0x113100;
  static constexpr int kBlocksRegion4Pc = 0x113180;

  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    // Create a minimal ROM for testing (2MB)
    std::vector<uint8_t> dummy_data(0x200000, 0);
    rom_->LoadFromData(dummy_data);

    SetupRoomObjectPointers();
    SetupSpritePointers();

    room_ = std::make_unique<Room>(0, rom_.get());
  }

  void SetupRoomObjectPointers() {
    // 1. Setup kRoomObjectPointer (0x874C) to point to our table at 0xF8000
    // The code reads 3 bytes from kRoomObjectPointer
    // int object_pointer = (rom_data[room_object_pointer + 2] << 16) + ...
    // We want object_pointer to be 0xF8000 (PC address)
    // 0xF8000 is 1F:8000 in LoROM (Bank 1F)
    // So we write 00 80 1F at 0x874C
    int ptr_loc = kRoomObjectPointer;
    rom_->mutable_data()[ptr_loc] = 0x00;
    rom_->mutable_data()[ptr_loc + 1] = 0x80;
    rom_->mutable_data()[ptr_loc + 2] = 0x1F;

    // 2. Setup Room 0 pointer at 0xF8000
    // We want Room 0 data to be at 0x100000 (Bank 20, PC 0x100000)
    // 0x100000 is 20:8000 in LoROM
    // Write 00 80 20 at 0xF8000
    int table_loc = 0xF8000;
    rom_->mutable_data()[table_loc] = 0x00;
    rom_->mutable_data()[table_loc + 1] = 0x80;
    rom_->mutable_data()[table_loc + 2] = 0x20;

    // 3. Setup Room 1 pointer at 0xF8003 (for size calculation)
    // We want Room 0 to have 0x100 bytes of space
    // So Room 1 starts at 0x100100 (20:8100)
    // Write 00 81 20 at 0xF8003
    rom_->mutable_data()[table_loc + 3] = 0x00;
    rom_->mutable_data()[table_loc + 4] = 0x81;
    rom_->mutable_data()[table_loc + 5] = 0x20;

    // 4. Seed Room 0 object data at 0x100000.
    //
    // Room::LoadObjects expects the room pointer table to point directly at the
    // 2-byte room header (floor gfx/layout). The object stream begins at +2.
    const int room_data_loc = 0x100000;
    rom_->mutable_data()[room_data_loc + 0] = 0x00;  // floor gfx nibble data
    rom_->mutable_data()[room_data_loc + 1] = 0x00;  // layout/flags

    // 5. Provide a valid empty object stream so LoadObjects doesn't scan zeros.
    // Format: layer0 terminator, layer1 terminator, door marker, door terminator.
    const int objects_start = room_data_loc + 2;
    std::vector<uint8_t> empty = {
        0xFF, 0xFF,  // end layer0
        0xFF, 0xFF,  // end layer1
        0xF0, 0xFF,  // door marker ($FFF0)
        0xFF, 0xFF,  // door terminator ($FFFF) (also end layer2)
    };
    ASSERT_TRUE(rom_->WriteVector(objects_start, empty).ok());
  }

  void SetupSpritePointers() {
    // 1. Setup kRoomsSpritePointer (0x4C298)
    // Points to table in Bank 09. Let's put table at 0x48000 (09:8000)
    int ptr_loc = kRoomsSpritePointer;
    rom_->mutable_data()[ptr_loc] = 0x00;
    rom_->mutable_data()[ptr_loc + 1] = 0x80;
    // Bank is hardcoded to 0x09 in code, so we only write low 2 bytes.

    // 2. Setup Sprite Pointer Table at 0x48000 (09:8000)
    // Room 0 pointer -> sprite list at 0x49000 (09:9000)
    // Write 00 90 at 0x48000
    int table_loc = 0x48000;
    rom_->mutable_data()[table_loc] = 0x00;
    rom_->mutable_data()[table_loc + 1] = 0x90;

    // Room 1 pointer at 0x48002 (for size calculation)
    // Let's give 0x50 bytes for sprites.
    // Next room at 0x49050 (09:9050)
    // Write 50 90 at 0x48002
    rom_->mutable_data()[table_loc + 2] = 0x50;
    rom_->mutable_data()[table_loc + 3] = 0x90;

    // 3. Setup Sprite Data at 0x49000
    // Sortsprite byte (0 or 1)
    rom_->mutable_data()[0x49000] = 0x00;
    // End of sprites (0xFF)
    rom_->mutable_data()[0x49001] = 0xFF;
  }

  void WriteLongPointer(int addr, uint32_t snes_addr) {
    rom_->mutable_data()[addr + 0] = snes_addr & 0xFF;
    rom_->mutable_data()[addr + 1] = (snes_addr >> 8) & 0xFF;
    rom_->mutable_data()[addr + 2] = (snes_addr >> 16) & 0xFF;
  }

  void SetupChestTable() {
    WriteLongPointer(kChestsDataPointer1, PcToSnes(kChestDataPc));
    rom_->mutable_data()[kChestsLengthPointer] = 0x00;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    std::fill_n(rom_->mutable_data() + kChestDataPc, 0x100, 0x00);
  }

  void SeedChestEntry(int room_id, uint8_t chest_id, bool big) {
    const uint16_t word = static_cast<uint16_t>(room_id) | (big ? 0x8000 : 0);
    rom_->mutable_data()[kChestsLengthPointer] = 0x01;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    rom_->mutable_data()[kChestDataPc + 0] = word & 0xFF;
    rom_->mutable_data()[kChestDataPc + 1] = (word >> 8) & 0xFF;
    rom_->mutable_data()[kChestDataPc + 2] = chest_id;
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

    rom_->mutable_data()[kPotRoom0Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom0Pc + 1] = 0xFF;
    rom_->mutable_data()[kPotRoom1Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom1Pc + 1] = 0xFF;
    rom_->mutable_data()[kPotRoom2Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom2Pc + 1] = 0xFF;
  }

  void SeedPotItemBytes(int pc_addr, std::initializer_list<uint8_t> bytes) {
    std::copy(bytes.begin(), bytes.end(), rom_->mutable_data() + pc_addr);
  }

  void SetupPitRegion() {
    WriteLongPointer(kPitPointer, PcToSnes(kPitDataPc));
    rom_->mutable_data()[kPitCount] = 0x02;
    rom_->mutable_data()[kPitDataPc + 0] = 0x34;
    rom_->mutable_data()[kPitDataPc + 1] = 0x12;
  }

  void SetupBlockRegions() {
    WriteLongPointer(kBlocksPointer1, PcToSnes(kBlocksRegion1Pc));
    WriteLongPointer(kBlocksPointer2, PcToSnes(kBlocksRegion2Pc));
    WriteLongPointer(kBlocksPointer3, PcToSnes(kBlocksRegion3Pc));
    WriteLongPointer(kBlocksPointer4, PcToSnes(kBlocksRegion4Pc));

    rom_->mutable_data()[kBlocksLength] = 0x04;
    rom_->mutable_data()[kBlocksLength + 1] = 0x00;

    rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0xAA;
    rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0xBB;
    rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0xCC;
    rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0xDD;
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Room> room_;
};

TEST_F(DungeonSaveTest, SaveObjects_FitsInSpace) {
  // Add a few objects
  RoomObject obj1(0x10, 10, 10, 0, 0);
  room_->AddObject(obj1);

  auto status = room_->SaveObjects();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(DungeonSaveTest, SaveObjects_TooLarge) {
  // Add MANY objects to exceed 256 bytes
  // Each object encodes to 3 bytes.
  // We need > 85 objects.
  for (int i = 0; i < 100; ++i) {
    RoomObject obj(0x10, 10, 10, 0, 0);
    room_->AddObject(obj);
  }

  auto status = room_->SaveObjects();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
}

TEST_F(DungeonSaveTest, SaveSprites_FitsInSpace) {
  // Add a sprite
  zelda3::Sprite spr(0x10, 10, 10, 0, 0);
  room_->GetSprites().push_back(spr);

  auto status = room_->SaveSprites();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(DungeonSaveTest, SaveSprites_TooLargeFallsBackToRelocation) {
  // Add MANY sprites to exceed 0x50 (80) bytes
  // Each sprite is 3 bytes.
  // We need > 26 sprites.
  for (int i = 0; i < 30; ++i) {
    zelda3::Sprite spr(0x10, 10, 10, 0, 0);
    room_->GetSprites().push_back(spr);
  }

  auto status = room_->SaveSprites();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_NE((rom_->data()[0x48001] << 8) | rom_->data()[0x48000], 0x9000);
}

TEST_F(DungeonSaveTest, SaveSprites_PreservesSortspriteHeaderByte) {
  // Seed the room with a known sort-byte value (0x00) followed by terminator.
  rom_->mutable_data()[0x49000] = 0x00;
  rom_->mutable_data()[0x49001] = 0xFF;

  zelda3::Sprite spr(0x10, 10, 10, 0, 0);
  room_->GetSprites().push_back(spr);

  auto status = room_->SaveSprites();
  ASSERT_TRUE(status.ok()) << status.message();

  // SaveSprites must preserve the first byte (SortSprites mode) and write
  // encoded payload after it.
  EXPECT_EQ(rom_->data()[0x49000], 0x00);
  EXPECT_EQ(rom_->data()[0x49001], 0x0A);  // b1 (Y)
  EXPECT_EQ(rom_->data()[0x49002], 0x0A);  // b2 (X)
  EXPECT_EQ(rom_->data()[0x49003], 0x10);  // b3 (sprite id)
  EXPECT_EQ(rom_->data()[0x49004], 0xFF);  // terminator
}

TEST_F(DungeonSaveTest, LoadSprites_DoesNotDuplicateOnReload) {
  // sort byte + one sprite + terminator
  rom_->mutable_data()[0x49000] = 0x00;
  rom_->mutable_data()[0x49001] = 0x0A;  // b1 (Y)
  rom_->mutable_data()[0x49002] = 0x0A;  // b2 (X)
  rom_->mutable_data()[0x49003] = 0x10;  // b3 (id)
  rom_->mutable_data()[0x49004] = 0xFF;

  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 1u);

  // Reloading the same room should refresh, not append duplicate entries.
  room_->LoadSprites();
  EXPECT_EQ(room_->GetSprites().size(), 1u);
}

TEST_F(DungeonSaveTest, EncodeObjects_SkipsTorchesAndBlocks) {
  Room room(0, rom_.get());
  room.ClearTileObjects();

  // One normal room object plus two "special table" objects.
  RoomObject normal(0x10, 1, 1, 0, 0);
  RoomObject torch(0x150, 2, 2, 0, 0);
  torch.set_options(ObjectOption::Torch);
  RoomObject block(0x0E00, 3, 3, 0, 0);
  block.set_options(ObjectOption::Block);

  room.AddTileObject(normal);
  room.AddTileObject(torch);
  room.AddTileObject(block);

  auto encoded = room.EncodeObjects();

  // Expect only the normal object to be encoded:
  // - 3 bytes object
  // - layer0 terminator (FF FF)
  // - layer1 terminator (FF FF)
  // - door marker (F0 FF) (in layer2 stream)
  // - door terminator (FF FF) (also layer2 terminator)
  EXPECT_EQ(encoded.size(), 11u);

  const auto nb = normal.EncodeObjectToBytes();
  EXPECT_EQ(encoded[0], nb.b1);
  EXPECT_EQ(encoded[1], nb.b2);
  EXPECT_EQ(encoded[2], nb.b3);

  EXPECT_EQ(encoded[3], 0xFF);
  EXPECT_EQ(encoded[4], 0xFF);
  EXPECT_EQ(encoded[5], 0xFF);
  EXPECT_EQ(encoded[6], 0xFF);

  EXPECT_EQ(encoded[7], 0xF0);
  EXPECT_EQ(encoded[8], 0xFF);
  EXPECT_EQ(encoded[9], 0xFF);
  EXPECT_EQ(encoded[10], 0xFF);
}

TEST_F(DungeonSaveTest, SaveObjects_RoundTripsDoorsInLayer2Stream) {
  // Seed the room's object stream with a valid empty encoding:
  // layer0 terminator, layer1 terminator, door marker, door terminator.
  const int objects_start_pc = 0x100000 + 2;  // 2-byte header at 0x100000
  std::vector<uint8_t> empty = {
      0xFF, 0xFF,  // end layer0
      0xFF, 0xFF,  // end layer1
      0xF0, 0xFF,  // door marker ($FFF0)
      0xFF, 0xFF,  // door terminator ($FFFF) (also end layer2)
  };
  ASSERT_TRUE(rom_->WriteVector(objects_start_pc, empty).ok());

  Room room(0, rom_.get());
  room.LoadObjects();
  EXPECT_EQ(room.GetDoors().size(), 0u);

  // Add one door and save.
  Room::Door door;
  door.position = 3;
  door.direction = DoorDirection::North;
  door.type = DoorType::NormalDoor;
  room.AddDoor(door);

  auto save_status = room.SaveObjects();
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  // Reload and verify the door is still present.
  Room room2(0, rom_.get());
  room2.LoadObjects();
  ASSERT_EQ(room2.GetDoors().size(), 1u);

  const auto [b1_expected, b2_expected] = door.EncodeBytes();
  const auto [b1_actual, b2_actual] = room2.GetDoors()[0].EncodeBytes();
  EXPECT_EQ(b1_actual, b1_expected);
  EXPECT_EQ(b2_actual, b2_expected);
}

TEST_F(DungeonSaveTest, SaveAllTorches_WritesLitBit) {
  std::vector<Room> rooms(kNumberOfRooms);

  RoomObject torch(0x150, 10, 20, 0, 1);
  torch.set_options(ObjectOption::Torch);
  torch.lit_ = true;
  rooms[1].AddTileObject(torch);

  auto status = SaveAllTorches(rom_.get(), rooms);
  EXPECT_TRUE(status.ok()) << status.message();

  const auto& rom_data = rom_->vector();
  const uint16_t torch_len =
      static_cast<uint16_t>(rom_data[kTorchesLengthPointer]) |
      (static_cast<uint16_t>(rom_data[kTorchesLengthPointer + 1]) << 8);
  EXPECT_EQ(torch_len, 6u);

  EXPECT_EQ(rom_data[kTorchData + 0], 0x01);
  EXPECT_EQ(rom_data[kTorchData + 1], 0x00);

  // word = ((x + y*64) << 1) with layer in bit 13 and lit in bit 15.
  EXPECT_EQ(rom_data[kTorchData + 2], 0x14);  // low byte
  EXPECT_EQ(rom_data[kTorchData + 3],
            0xAA);  // high byte: layer + lit + address bits

  EXPECT_EQ(rom_data[kTorchData + 4], 0xFF);
  EXPECT_EQ(rom_data[kTorchData + 5], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllTorches_NoOpWhenUnchanged) {
  // Seed ROM with a torch blob identical to what SaveAllTorches would emit.
  // This should be a no-op (no writes) and keep the ROM clean.
  std::vector<uint8_t> blob = {
      0x01, 0x00,  // room_id = 1
      0x14, 0xAA,  // torch word (x=10,y=20,layer=1,lit=1)
      0xFF, 0xFF,  // terminator
  };
  ASSERT_TRUE(
      rom_->WriteWord(kTorchesLengthPointer, static_cast<uint16_t>(blob.size()))
          .ok());
  ASSERT_TRUE(rom_->WriteVector(kTorchData, blob).ok());
  rom_->ClearDirty();

  std::vector<Room> rooms(kNumberOfRooms);
  RoomObject torch(0x150, 10, 20, 0, 1);
  torch.set_options(ObjectOption::Torch);
  torch.lit_ = true;
  rooms[1].AddTileObject(torch);

  auto status = SaveAllTorches(rom_.get(), rooms);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest, SaveAllChests_WritesSingleSmallChest) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x01);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x42);
}

TEST_F(DungeonSaveTest, SaveAllChests_WritesBigChestWithHighBit) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x77, true});

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x01);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x80);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x77);
}

TEST_F(DungeonSaveTest, SaveAllChests_PreservesRomEntriesForUntouchedRooms) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/1, /*chest_id=*/0x99, /*big=*/false);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x02);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);

  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x42);

  EXPECT_EQ(rom_->data()[kChestDataPc + 3], 0x01);
  EXPECT_EQ(rom_->data()[kChestDataPc + 4], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 5], 0x99);
}

TEST_F(DungeonSaveTest,
       SaveAllChests_LoadedRoomWithNoChestsClearsExistingRomEntries) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x00);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
}

TEST_F(DungeonSaveTest, SaveAllChests_ReloadedRoomMatchesSerializedState) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].GetChests().push_back(chest_data{0x77, true});

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  Room reloaded_room(0, rom_.get());
  reloaded_room.LoadChests();

  ASSERT_EQ(reloaded_room.GetChests().size(), 2u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x42);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
  EXPECT_EQ(reloaded_room.GetChests()[1].id, 0x77);
  EXPECT_TRUE(reloaded_room.GetChests()[1].size);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_LoadedRoomWritesEntriesAndTerminator) {
  SetupPotItemTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  PotItem item;
  item.position = 0x1234;
  item.item = 0x56;
  rooms[0].GetPotItems().push_back(item);

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0x34);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0x12);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 2], 0x56);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 3], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 4], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_ReloadedRoomMatchesSerializedState) {
  SetupPotItemTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  PotItem first;
  first.position = 0x1234;
  first.item = 0x56;
  rooms[0].GetPotItems().push_back(first);

  PotItem second;
  second.position = 0x5678;
  second.item = 0x9A;
  rooms[0].GetPotItems().push_back(second);

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  Room reloaded_room(0, rom_.get());
  reloaded_room.LoadPotItems();

  ASSERT_EQ(reloaded_room.GetPotItems().size(), 2u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
  EXPECT_EQ(reloaded_room.GetPotItems()[1].position, 0x5678);
  EXPECT_EQ(reloaded_room.GetPotItems()[1].item, 0x9A);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_UnloadedRoomPreservesExistingRomData) {
  SetupPotItemTable();
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0x34);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0x12);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 2], 0x56);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 3], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 4], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_LoadedRoomWithNoItemsWritesTerminator) {
  SetupPotItemTable();
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllCollision_DirtyRoomWithoutCustomRegionFails) {
  std::vector<uint8_t> small_data(0x100000, 0);
  rom_->LoadFromData(small_data);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetCollisionTile(1, 1, 0x2A);

  auto status = SaveAllCollision(rom_.get(), absl::MakeSpan(rooms));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonSaveTest, SaveAllPits_ValidRegionPreservesExistingBytes) {
  SetupPitRegion();

  const auto before0 = rom_->data()[kPitDataPc + 0];
  const auto before1 = rom_->data()[kPitDataPc + 1];

  auto status = SaveAllPits(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPitDataPc + 0], before0);
  EXPECT_EQ(rom_->data()[kPitDataPc + 1], before1);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_ValidRegionsPreserveExistingBytes) {
  SetupBlockRegions();

  auto status = SaveAllBlocks(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0xAA);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0xBB);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0xCC);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0xDD);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_PreservesUnmaterializedRoomBytes) {
  // Migration safety invariant: when only some rooms are
  // materialized, blocks for the other rooms (lookup returns null,
  // OR returns a Room whose blocks weren't loaded) must keep their
  // existing ROM bytes verbatim. The encoder cannot drop them just
  // because no in-memory representation was supplied.
  //
  // Setup: two encoded entries in slot 0 (room 0, materialized) and
  // slot 1 (room 5, NOT materialized). Save with a lookup that only
  // returns room_ for id=0. Both slots must come out unchanged.
  SetupBlockRegions();
  // Slot 0: room 0, px=10, py=20, layer=1 (word 0x4A14).
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;
  // Slot 1: room 5, px=15, py=25, layer=0.
  //   word = (15 << 1) | (25 << 7) | (0 << 14) = 30 | 3200 = 0x0C9E
  //   → b3 = 0x9E, b4 = 0x0C.
  rom_->mutable_data()[kBlocksRegion1Pc + 4] = 0x05;
  rom_->mutable_data()[kBlocksRegion1Pc + 5] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 6] = 0x9E;
  rom_->mutable_data()[kBlocksRegion1Pc + 7] = 0x0C;
  rom_->mutable_data()[kBlocksLength] = 0x08;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  const std::array<uint8_t, 8> before = {0x00, 0x00, 0x14, 0x4A,
                                         0x05, 0x00, 0x9E, 0x0C};

  // Materialize room 0 only. LoadBlocks scans the global buffer for
  // matching room_id, so only slot 0 lands in tile_objects_. Room 5
  // stays unloaded — its block bytes must survive the save.
  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);
  ASSERT_TRUE(room_->AreBlocksLoaded());

  // Lookup hands out room_ for id 0; everything else is null
  // (simulates an editor that only materialized room 0).
  auto status = SaveAllBlocks(rom_.get(), 296, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + i], before[i])
        << "byte " << i
        << " — unmaterialized room 5's block bytes must survive a save "
           "where only room 0 was materialized.";
  }
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_EditedBlockPreservesUnmaterializedNeighbor) {
  // Migration safety invariant #2: an edit to a materialized room's
  // block writes through, AND an unmaterialized room's adjacent slot
  // remains byte-identical. This is the test that pins the encoder
  // boundary between "owned by editor, re-encode" and "owned by ROM,
  // preserve verbatim".
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // px=10
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;
  rom_->mutable_data()[kBlocksRegion1Pc + 4] = 0x05;
  rom_->mutable_data()[kBlocksRegion1Pc + 5] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 6] = 0x9E;
  rom_->mutable_data()[kBlocksRegion1Pc + 7] = 0x0C;
  rom_->mutable_data()[kBlocksLength] = 0x08;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);

  // Edit room 0's block: px 10 → 30. New word: (30<<1)|(20<<7)|(1<<14)
  // = 0x4A3C → b3 = 0x3C, b4 = 0x4A.
  for (auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) == ObjectOption::Block) {
      obj.set_x(30);
      break;
    }
  }

  auto status = SaveAllBlocks(rom_.get(), 296, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0x3C) << "edited px=30";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x4A);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 4], 0x05) << "room 5 preserved";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 5], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 6], 0x9E);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 7], 0x0C);
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_NoOpRoundTripPreservesPointedBytes) {
  // Real encoder invariant #1 (test-first for the SaveAllBlocks
  // follow-up). After Load → Save with no in-memory mutation, the
  // bytes at the dereferenced data region must be byte-identical to
  // the original — vanilla saves with no edits cannot reshuffle
  // anything. Single block in single room: source-order preservation
  // is trivial here, so this pins the simpler invariant first; a
  // multi-room ordering test belongs to the real ROM round-trip
  // follow-up.
  SetupBlockRegions();
  // Replace the fixture's sample 0xAA..0xDD with a real entry for
  // room 0 at (px=10, py=20, layer=1) — the same shape used by
  // LoadBlocks_ReadsFromDereferencedPointerRegion.
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;

  const std::array<uint8_t, 4> before = {0x00, 0x00, 0x14, 0x4A};

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U)
      << "Pre-save: LoadBlocks should have produced exactly one Block "
         "tile object.";

  auto status = SaveAllBlocks(rom_.get(), 1, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + i], before[i])
        << "byte " << i
        << " — no-op round trip must not mutate the pointed region.";
  }
  // Length immediate also stays at 4 (one entry × 4 bytes).
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x04);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RoomAware_EditedBlockUpdatesRegion) {
  // Real encoder invariant #2: an in-memory edit (here, moving the
  // block from px=10 to px=30) writes back through the pointed
  // region. The block stays at load_order=0 since it was loaded —
  // the encoder must keep it in slot 0 and just rewrite its bytes.
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // px=10
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;  // py=20, layer=1

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);

  // Mutate px to 30 in place. New encoded word:
  //   word = (30 << 1) | (20 << 7) | (1 << 14)
  //        = 60 | 2560 | 16384 = 19004 = 0x4A3C
  // → b3 = 0x3C, b4 = 0x4A.
  for (auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) == ObjectOption::Block) {
      obj.set_x(30);
      break;
    }
  }

  auto status = SaveAllBlocks(rom_.get(), 1, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0x00) << "room_id lo";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0x00) << "room_id hi";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0x3C) << "word lo (px=30)";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x4A) << "word hi";
}

TEST_F(DungeonSaveTest, LoadBlocks_ReadsFromDereferencedPointerRegion) {
  // Loader/saver invariant: `kBlocksPointer1..4` are 3-byte SNES
  // long-address operand slots embedded in the bank_02 LDA.l
  // instructions that fan the pushable-block table into WRAM. The actual
  // entry bytes live at the pointed-to region (`kBlocksRegion1Pc..` in
  // this fixture's mock layout).
  //
  // `SaveAllBlocks` already dereferences each pointer slot and writes
  // data at the SNES address it encodes. `LoadBlocks` previously read
  // bytes directly out of the operand slots — i.e. it was decoding the
  // LDA.l opcode operand bytes as block data and silently producing
  // garbage. This test pins the dereference invariant.
  SetupBlockRegions();

  // Overwrite the fixture's sample 0xAA..0xDD seed with an encoded
  // pushable-block entry for room_id=0 at (px=10, py=20, layer=1):
  //   word = (px << 1) | (py << 7) | (layer << 14)
  //        = 20 | 2560 | 16384 = 18964 = 0x4A14
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;  // room_id lo
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;  // room_id hi
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // word lo
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;  // word hi

  room_->LoadBlocks();

  int block_count = 0;
  for (const auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) != ObjectOption::Block)
      continue;
    ++block_count;
    EXPECT_EQ(obj.x(), 10);
    EXPECT_EQ(obj.y(), 20);
    EXPECT_EQ(obj.GetLayerValue(), 1);
  }
  EXPECT_EQ(block_count, 1)
      << "LoadBlocks must dereference kBlocksPointer1 as a 3-byte SNES "
         "long-address operand and load block data from the pointed "
         "region — not from the operand slot itself.";
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze

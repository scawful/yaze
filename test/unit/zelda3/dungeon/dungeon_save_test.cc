#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <memory>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonSaveTest : public ::testing::Test {
 protected:
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

    // 4. Setup Room 0 Object Data Header at 0x100000
    // The code reads tile_address from room_address (which is 0x100000)
    // int tile_address = (rom_data[room_address + 2] << 16) + ...
    // We want tile_address to be 0x100005 (just after this pointer)
    // 0x100005 is 20:8005
    int room_data_loc = 0x100000;
    rom_->mutable_data()[room_data_loc] = 0x05;
    rom_->mutable_data()[room_data_loc + 1] = 0x80;
    rom_->mutable_data()[room_data_loc + 2] = 0x20;

    // 5. Setup actual object data at 0x100005
    // Header (2 bytes) + Objects
    // 0x100005: Floor/Layout info (2 bytes)
    rom_->mutable_data()[0x100005] = 0x00;
    rom_->mutable_data()[0x100006] = 0x00;
    // 0x100007: Start of objects
    // Empty object list: FF FF (Layer 1) FF FF (Layer 2) FF FF (Layer 3) FF FF (End)
    // Total 8 bytes.
    // Available space is 0x100 - 5 = 0xFB bytes (approx)
    // Actually CalculateRoomSize uses the Room Pointers (0xF8000).
    // Room 0 Size = 0x100100 - 0x100000 = 0x100 (256 bytes).
    // Used by header/pointers: 5 bytes? No, CalculateRoomSize returns raw size between room starts.
    // So available is 256 bytes.
    // SaveObjects subtracts 2 for header. So 254 bytes for objects.
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

TEST_F(DungeonSaveTest, SaveSprites_TooLarge) {
  // Add MANY sprites to exceed 0x50 (80) bytes
  // Each sprite is 3 bytes.
  // We need > 26 sprites.
  for (int i = 0; i < 30; ++i) {
    zelda3::Sprite spr(0x10, 10, 10, 0, 0);
    room_->GetSprites().push_back(spr);
  }

  auto status = room_->SaveSprites();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
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
  // 3 bytes object + 3 layer terminators (FF FF) + door marker (F0 FF) +
  // door terminator (FF FF).
  EXPECT_EQ(encoded.size(), 13u);

  const auto nb = normal.EncodeObjectToBytes();
  EXPECT_EQ(encoded[0], nb.b1);
  EXPECT_EQ(encoded[1], nb.b2);
  EXPECT_EQ(encoded[2], nb.b3);

  EXPECT_EQ(encoded[3], 0xFF);
  EXPECT_EQ(encoded[4], 0xFF);
  EXPECT_EQ(encoded[5], 0xFF);
  EXPECT_EQ(encoded[6], 0xFF);
  EXPECT_EQ(encoded[7], 0xFF);
  EXPECT_EQ(encoded[8], 0xFF);

  EXPECT_EQ(encoded[9], 0xF0);
  EXPECT_EQ(encoded[10], 0xFF);
  EXPECT_EQ(encoded[11], 0xFF);
  EXPECT_EQ(encoded[12], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllTorches_WritesLitBit) {
  std::array<Room, kNumberOfRooms> rooms;

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
  EXPECT_EQ(rom_data[kTorchData + 3], 0xAA);  // high byte: layer + lit + address bits

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
  ASSERT_TRUE(rom_->WriteWord(kTorchesLengthPointer,
                              static_cast<uint16_t>(blob.size()))
                  .ok());
  ASSERT_TRUE(rom_->WriteVector(kTorchData, blob).ok());
  rom_->ClearDirty();

  std::array<Room, kNumberOfRooms> rooms;
  RoomObject torch(0x150, 10, 20, 0, 1);
  torch.set_options(ObjectOption::Torch);
  torch.lit_ = true;
  rooms[1].AddTileObject(torch);

  auto status = SaveAllTorches(rom_.get(), rooms);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rom_->dirty());
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze

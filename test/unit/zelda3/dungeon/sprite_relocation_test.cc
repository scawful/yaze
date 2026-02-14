#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace test {

class SpriteRelocationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    std::vector<uint8_t> dummy_data(0x200000, 0x00);
    ASSERT_TRUE(rom_->LoadFromData(dummy_data).ok());

    // kRoomsSpritePointer stores the low 16 bits of a Bank 09 pointer table.
    // Use Bank 09:8000 => PC 0x048000.
    rom_->mutable_data()[kRoomsSpritePointer] = 0x00;
    rom_->mutable_data()[kRoomsSpritePointer + 1] = 0x80;
    sprite_table_pc_ = SnesToPc(0x098000);

    SetAllRoomPointers(kSpritesData);
    WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  }

  void SetAllRoomPointers(int pc_addr) {
    for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
      SetRoomPointer(room_id, pc_addr);
    }
    WriteSpriteStream(pc_addr, 0x00, EncodeSpritePayload(0));
  }

  void SetRoomPointer(int room_id, int pc_addr) {
    ASSERT_GE(room_id, 0);
    ASSERT_LT(room_id, kNumberOfRooms);
    const uint32_t snes = PcToSnes(pc_addr);
    const int ptr_off = sprite_table_pc_ + room_id * 2;
    rom_->mutable_data()[ptr_off] = snes & 0xFF;
    rom_->mutable_data()[ptr_off + 1] = (snes >> 8) & 0xFF;
  }

  int GetRoomPointerPc(int room_id) const {
    EXPECT_GE(room_id, 0);
    EXPECT_LT(room_id, kNumberOfRooms);
    const int ptr_off = sprite_table_pc_ + room_id * 2;
    const uint16_t low16 =
        (rom_->data()[ptr_off + 1] << 8) | rom_->data()[ptr_off];
    return SnesToPc(0x090000 | low16);
  }

  std::vector<uint8_t> EncodeSpritePayload(int count,
                                           uint8_t id_base = 0x10) const {
    std::vector<uint8_t> payload;
    payload.reserve(count * 3 + 1);
    for (int i = 0; i < count; ++i) {
      payload.push_back(0x0A + (i & 0x07));  // b1 (y/flags/layer)
      payload.push_back(0x0B + (i & 0x07));  // b2 (x/flags)
      payload.push_back(id_base + i);        // b3 (id)
    }
    payload.push_back(0xFF);
    return payload;
  }

  void WriteSpriteStream(int pc_addr, uint8_t sort_mode,
                         const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> bytes;
    bytes.reserve(payload.size() + 1);
    bytes.push_back(sort_mode);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    ASSERT_TRUE(rom_->WriteVector(pc_addr, bytes).ok());
  }

  void AddSprites(Room* room, int count, uint8_t id_base = 0x20) const {
    for (int i = 0; i < count; ++i) {
      room->GetSprites().emplace_back(id_base + i, 5 + i, 6 + i, 0, 0);
    }
  }

  std::unique_ptr<Rom> rom_;
  int sprite_table_pc_ = 0;
};

TEST_F(SpriteRelocationTest, FindMaxUsedSpriteAddress_EmptyROM) {
  EXPECT_EQ(FindMaxUsedSpriteAddress(rom_.get()), kSpritesData + 2);
}

TEST_F(SpriteRelocationTest, FindMaxUsedSpriteAddress_WithExistingSprites) {
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(3));
  EXPECT_EQ(FindMaxUsedSpriteAddress(rom_.get()), kSpritesData + 11);
}

TEST_F(SpriteRelocationTest, FindMaxUsedSpriteAddress_MultipleRooms) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  SetRoomPointer(2, kSpritesData + 0x40);

  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(1));
  WriteSpriteStream(kSpritesData + 0x20, 0x01, EncodeSpritePayload(2));
  WriteSpriteStream(kSpritesData + 0x40, 0x00, EncodeSpritePayload(4));

  EXPECT_EQ(FindMaxUsedSpriteAddress(rom_.get()), kSpritesData + 0x40 + 14);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_WritesToFreeSpace) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x30);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x30, 0x00, EncodeSpritePayload(2));

  const int expected_write_pos = FindMaxUsedSpriteAddress(rom_.get());
  const auto encoded = EncodeSpritePayload(4, 0x40);
  auto status = RelocateSpriteData(rom_.get(), 0, encoded);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(GetRoomPointerPc(0), expected_write_pos);
  EXPECT_EQ(rom_->data()[expected_write_pos], 0x00);
  for (size_t i = 0; i < encoded.size(); ++i) {
    EXPECT_EQ(rom_->data()[expected_write_pos + 1 + i], encoded[i]);
  }
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_PreservesSortByte) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x01, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(1));

  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(2));
  ASSERT_TRUE(status.ok()) << status.message();

  const int relocated_pc = GetRoomPointerPc(0);
  EXPECT_EQ(rom_->data()[relocated_pc], 0x01);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_UpdatesPointerTable) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(1));

  const int expected_write_pos = FindMaxUsedSpriteAddress(rom_.get());
  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(2));
  ASSERT_TRUE(status.ok()) << status.message();

  const uint32_t snes = PcToSnes(expected_write_pos);
  const int ptr_off = sprite_table_pc_ + 0;
  EXPECT_EQ(rom_->data()[ptr_off], snes & 0xFF);
  EXPECT_EQ(rom_->data()[ptr_off + 1], (snes >> 8) & 0xFF);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_ZeroFillsOldSlot) {
  SetAllRoomPointers(kSpritesData + 0x20);
  SetRoomPointer(0, kSpritesData);
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(2));

  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(4));
  ASSERT_TRUE(status.ok()) << status.message();

  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(rom_->data()[kSpritesData + i], 0x00);
  }
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_PreservesSharedOldSlot) {
  SetAllRoomPointers(kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(0));

  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(1));
  ASSERT_TRUE(status.ok()) << status.message();

  // Shared stream must remain valid for other rooms still pointing at it.
  EXPECT_EQ(rom_->data()[kSpritesData], 0x00);
  EXPECT_EQ(rom_->data()[kSpritesData + 1], 0xFF);
}

TEST_F(SpriteRelocationTest, SaveSprites_FallsBackToRelocation) {
  SetAllRoomPointers(kSpritesData + 0x04);
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x02);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x02, 0x00, EncodeSpritePayload(0));

  Room room(0, rom_.get());
  AddSprites(&room, 1, 0x55);
  auto status = room.SaveSprites();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_NE(GetRoomPointerPc(0), kSpritesData);
}

TEST_F(SpriteRelocationTest, SaveSprites_InPlaceStillWorks) {
  SetAllRoomPointers(kSpritesData + 0x80);
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(0));

  Room room(0, rom_.get());
  AddSprites(&room, 1, 0x33);
  auto status = room.SaveSprites();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(GetRoomPointerPc(0), kSpritesData);
  EXPECT_EQ(rom_->data()[kSpritesData], 0x00);
  EXPECT_EQ(rom_->data()[kSpritesData + 4], 0xFF);
}

TEST_F(SpriteRelocationTest, SaveSprites_RelocationRoundTrip) {
  SetAllRoomPointers(kSpritesData + 0x04);
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x02);
  WriteSpriteStream(kSpritesData, 0x01, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x02, 0x00, EncodeSpritePayload(0));

  Room room(0, rom_.get());
  room.GetSprites().emplace_back(0xA3, 10, 12, 0, 0);
  room.GetSprites().emplace_back(0x21, 14, 18, 0, 0);
  auto save_status = room.SaveSprites();
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  room.LoadSprites();
  ASSERT_EQ(room.GetSprites().size(), 2u);
  EXPECT_EQ(room.GetSprites()[0].id(), 0xA3);
  EXPECT_EQ(room.GetSprites()[1].id(), 0x21);
}

TEST_F(SpriteRelocationTest, SaveSprites_MultipleRelocations) {
  SetAllRoomPointers(kSpritesData + 0x08);
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x02);
  SetRoomPointer(2, kSpritesData + 0x04);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x02, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x04, 0x00, EncodeSpritePayload(0));

  Room room_a(0, rom_.get());
  AddSprites(&room_a, 1, 0x60);
  ASSERT_TRUE(room_a.SaveSprites().ok());
  const int a_ptr = GetRoomPointerPc(0);

  Room room_b(1, rom_.get());
  AddSprites(&room_b, 2, 0x70);
  ASSERT_TRUE(room_b.SaveSprites().ok());
  const int b_ptr = GetRoomPointerPc(1);

  EXPECT_GT(a_ptr, kSpritesData);
  EXPECT_GT(b_ptr, a_ptr);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_RegionFull) {
  SetRoomPointer(kNumberOfRooms - 1, kSpritesEndData - 2);
  WriteSpriteStream(kSpritesEndData - 2, 0x00, EncodeSpritePayload(0));

  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(1));
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_EmptyRoom) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(0));

  auto status = RelocateSpriteData(rom_.get(), 0, EncodeSpritePayload(0));
  ASSERT_TRUE(status.ok()) << status.message();

  const int relocated_pc = GetRoomPointerPc(0);
  EXPECT_EQ(rom_->data()[relocated_pc], 0x00);
  EXPECT_EQ(rom_->data()[relocated_pc + 1], 0xFF);
}

TEST_F(SpriteRelocationTest, RelocateSpriteData_RejectsMalformedPayload) {
  SetRoomPointer(0, kSpritesData);
  SetRoomPointer(1, kSpritesData + 0x20);
  WriteSpriteStream(kSpritesData, 0x00, EncodeSpritePayload(0));
  WriteSpriteStream(kSpritesData + 0x20, 0x00, EncodeSpritePayload(0));

  std::vector<uint8_t> malformed = {0x10, 0xFF};  // Not N*3 + terminator.
  auto status = RelocateSpriteData(rom_.get(), 0, malformed);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(SpriteRelocationTest, FindMaxUsedSpriteAddress_LastRoom) {
  SetRoomPointer(kNumberOfRooms - 1, kSpritesData + 0x60);
  WriteSpriteStream(kSpritesData + 0x60, 0x00, EncodeSpritePayload(3));

  EXPECT_EQ(FindMaxUsedSpriteAddress(rom_.get()), kSpritesData + 0x60 + 11);
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze

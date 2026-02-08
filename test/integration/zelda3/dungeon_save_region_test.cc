// Integration tests for dungeon save region preservation (ZScream parity).
// Verifies that SaveAllCollision, SaveAllPits, SaveAllBlocks, and SaveAllTorches
// preserve ROM regions when no edits are made, so validate-yaze --feature=*
// can pass when comparing a yaze-saved ROM to a golden ROM.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "absl/types/span.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace test {

using namespace yaze::zelda3;

class DungeonSaveRegionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    YAZE_SKIP_IF_ROM_MISSING(yaze::test::RomRole::kVanilla,
                             "DungeonSaveRegionTest");
    const std::string rom_path =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    auto status = rom_->LoadFromFile(rom_path);
    if (!status.ok()) {
      GTEST_SKIP() << "ROM file not available: " << status.message();
    }
    original_rom_data_ = rom_->vector();
  }

  void TearDown() override {
    if (rom_ && !original_rom_data_.empty()) {
      for (size_t i = 0; i < original_rom_data_.size(); ++i) {
        rom_->WriteByte(static_cast<int>(i), original_rom_data_[i]);
      }
    }
  }

  std::unique_ptr<Rom> rom_;
  std::vector<uint8_t> original_rom_data_;
};

// --- Collision (ZScream CompareCollisionRegions: 0x128090..0x130000) ---

TEST_F(DungeonSaveRegionTest, SaveAllCollisionPreservesRegion) {
  const auto& data = rom_->vector();
  const int ptrs_size = kNumberOfRooms * 3;
  const int data_size = kCustomCollisionDataEnd - kCustomCollisionDataPosition;
  if (kCustomCollisionRoomPointers + ptrs_size >
          static_cast<int>(data.size()) ||
      kCustomCollisionDataPosition + data_size >
          static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for collision region";
  }

  std::vector<uint8_t> ptrs_before(
      data.begin() + kCustomCollisionRoomPointers,
      data.begin() + kCustomCollisionRoomPointers + ptrs_size);
  std::vector<uint8_t> data_before(
      data.begin() + kCustomCollisionDataPosition,
      data.begin() + kCustomCollisionDataPosition + data_size);

  std::array<Room, kNumberOfRooms> rooms;
  auto status = SaveAllCollision(rom_.get(), absl::MakeSpan(rooms));
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  std::vector<uint8_t> ptrs_after(
      after.begin() + kCustomCollisionRoomPointers,
      after.begin() + kCustomCollisionRoomPointers + ptrs_size);
  std::vector<uint8_t> data_after(
      after.begin() + kCustomCollisionDataPosition,
      after.begin() + kCustomCollisionDataPosition + data_size);

  EXPECT_THAT(ptrs_after, ::testing::ContainerEq(ptrs_before));
  EXPECT_THAT(data_after, ::testing::ContainerEq(data_before));
}

// --- Pits (count @ 0x394A6, pointer @ 0x394AB, data at pointer) ---

TEST_F(DungeonSaveRegionTest, SaveAllPitsPreservesRegion) {
  const auto& data = rom_->vector();
  if (kPitCount >= static_cast<int>(data.size()) ||
      kPitPointer + 2 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for pits region";
  }

  int count_before = data[kPitCount];
  int entries = count_before / 2;
  std::vector<uint8_t> count_and_ptr_before(data.begin() + kPitCount,
                                            data.begin() + kPitCount + 1);
  count_and_ptr_before.insert(count_and_ptr_before.end(),
                              data.begin() + kPitPointer,
                              data.begin() + kPitPointer + 3);

  int pit_ptr_snes = (data[kPitPointer + 2] << 16) |
                     (data[kPitPointer + 1] << 8) | data[kPitPointer];
  int pit_data_pc =
      static_cast<int>(SnesToPc(static_cast<uint32_t>(pit_ptr_snes)));
  int data_len = entries * 2;
  std::vector<uint8_t> pit_data_before;
  if (entries > 0 && pit_data_pc >= 0 &&
      pit_data_pc + data_len <= static_cast<int>(data.size())) {
    pit_data_before.assign(data.begin() + pit_data_pc,
                           data.begin() + pit_data_pc + data_len);
  }

  auto status = SaveAllPits(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  EXPECT_EQ(after[kPitCount], count_before);
  EXPECT_EQ(after[kPitPointer], data[kPitPointer]);
  EXPECT_EQ(after[kPitPointer + 1], data[kPitPointer + 1]);
  EXPECT_EQ(after[kPitPointer + 2], data[kPitPointer + 2]);
  if (!pit_data_before.empty()) {
    ASSERT_GE(static_cast<int>(after.size()), pit_data_pc + data_len);
    for (int i = 0; i < data_len; ++i) {
      EXPECT_EQ(after[pit_data_pc + i], pit_data_before[i])
          << "Pit data mismatch at offset " << i;
    }
  }
}

// --- Blocks (length @ kBlocksLength, four pointers, four 0x80 regions) ---

TEST_F(DungeonSaveRegionTest, SaveAllBlocksPreservesRegion) {
  const auto& data = rom_->vector();
  if (kBlocksLength + 1 >= static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for blocks region";
  }

  int blocks_count = (data[kBlocksLength + 1] << 8) | data[kBlocksLength];
  if (blocks_count <= 0) {
    auto status = SaveAllBlocks(rom_.get());
    ASSERT_TRUE(status.ok()) << status.message();
    return;
  }

  const int kRegionSize = 0x80;
  int ptrs[4] = {kBlocksPointer1, kBlocksPointer2, kBlocksPointer3,
                 kBlocksPointer4};
  std::vector<std::vector<uint8_t>> regions_before;
  for (int r = 0; r < 4; ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(data.size()))
      break;
    int snes =
        (data[ptrs[r] + 2] << 16) | (data[ptrs[r] + 1] << 8) | data[ptrs[r]];
    int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    int off = r * kRegionSize;
    int len = std::min(kRegionSize, blocks_count - off);
    if (len <= 0)
      break;
    if (pc < 0 || pc + len > static_cast<int>(data.size()))
      break;
    regions_before.emplace_back(data.begin() + pc, data.begin() + pc + len);
  }

  auto status = SaveAllBlocks(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  EXPECT_EQ((after[kBlocksLength + 1] << 8) | after[kBlocksLength],
            blocks_count);
  for (size_t r = 0; r < regions_before.size(); ++r) {
    if (ptrs[r] + 2 >= static_cast<int>(after.size()))
      break;
    int snes =
        (after[ptrs[r] + 2] << 16) | (after[ptrs[r] + 1] << 8) | after[ptrs[r]];
    int pc = static_cast<int>(SnesToPc(static_cast<uint32_t>(snes)));
    int len = static_cast<int>(regions_before[r].size());
    ASSERT_GE(static_cast<int>(after.size()), pc + len);
    for (int i = 0; i < len; ++i) {
      EXPECT_EQ(after[pc + i], regions_before[r][i])
          << "Blocks region " << r << " offset " << i;
    }
  }
}

// --- Torches (length @ kTorchesLengthPointer, data @ kTorchData, max 0x120) ---

TEST_F(DungeonSaveRegionTest,
       SaveAllTorchesPreservesRegionWhenRoomsUnmodified) {
  const auto& data = rom_->vector();
  constexpr int kTorchesMaxSize = 0x120;
  if (kTorchesLengthPointer + 1 >= static_cast<int>(data.size()) ||
      kTorchData + kTorchesMaxSize > static_cast<int>(data.size())) {
    GTEST_SKIP() << "ROM too small for torches region";
  }

  int len_before =
      (data[kTorchesLengthPointer + 1] << 8) | data[kTorchesLengthPointer];
  if (len_before > kTorchesMaxSize)
    len_before = kTorchesMaxSize;
  std::vector<uint8_t> torch_region_before(
      data.begin() + kTorchesLengthPointer,
      data.begin() + kTorchesLengthPointer + 2);
  torch_region_before.insert(torch_region_before.end(),
                             data.begin() + kTorchData,
                             data.begin() + kTorchData + len_before);

  std::vector<Room> rooms;
  rooms.reserve(kNumberOfRooms);
  for (int i = 0; i < kNumberOfRooms; ++i) {
    rooms.emplace_back(i, rom_.get());
    rooms.back().LoadObjects();
  }

  auto status = SaveAllTorches(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& after = rom_->vector();
  int len_after =
      (after[kTorchesLengthPointer + 1] << 8) | after[kTorchesLengthPointer];
  EXPECT_EQ(len_after, len_before) << "Torch length changed";
  for (int i = 0; i < len_before && i < len_after; ++i) {
    EXPECT_EQ(after[kTorchData + i], torch_region_before[2 + i])
        << "Torch data mismatch at offset " << i;
  }
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze

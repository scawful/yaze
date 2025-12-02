#include "zelda3/overworld/overworld_version_helper.h"
#include "gtest/gtest.h"
#include "rom/rom.h"

namespace yaze::zelda3 {

TEST(OverworldVersionHelperTest, GetVersion_VanillaSmallRom) {
  Rom rom;
  std::vector<uint8_t> data(1024 * 1024, 0); // 1MB ROM
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());
  
  // Should return Vanilla and not crash
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom), OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_VanillaExpandedRom_ZeroFilled) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0); // 2MB ROM, 0 filled
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());
  
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom), OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_VanillaExpandedRom_FF_Filled) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0xFF); // 2MB ROM, 0xFF filled
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());
  
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom), OverworldVersion::kVanilla);
}

TEST(OverworldVersionHelperTest, GetVersion_V1) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = 1;
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());
  
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom), OverworldVersion::kZSCustomV1);
}

TEST(OverworldVersionHelperTest, GetVersion_V3) {
  Rom rom;
  std::vector<uint8_t> data(2 * 1024 * 1024, 0);
  data[OverworldCustomASMHasBeenApplied] = 3;
  auto status = rom.LoadFromData(data);
  ASSERT_TRUE(status.ok());
  
  EXPECT_EQ(OverworldVersionHelper::GetVersion(rom), OverworldVersion::kZSCustomV3);
}

} // namespace yaze::zelda3

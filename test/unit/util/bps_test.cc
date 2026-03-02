#include "util/bps.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::test {

namespace {

std::vector<uint8_t> MakeSourceData(size_t size) {
  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i) {
    data[i] = static_cast<uint8_t>((i * 37) & 0xFF);
  }
  return data;
}

}  // namespace

TEST(BpsTest, CreateAndApplyRoundTrip) {
  auto source = MakeSourceData(2048);
  auto target = source;

  for (size_t i = 128; i < 192; ++i) {
    target[i] ^= 0x5A;
  }
  for (size_t i = 600; i < 640; ++i) {
    target[i] = static_cast<uint8_t>(target[i] + 17);
  }
  target.push_back(0x42);
  target.push_back(0x99);
  target.push_back(0x00);

  std::vector<uint8_t> patch;
  auto create_status = util::CreateBpsPatch(source, target, patch);
  ASSERT_TRUE(create_status.ok()) << create_status.message();
  ASSERT_GE(patch.size(), 16u);
  EXPECT_EQ(patch[0], 'B');
  EXPECT_EQ(patch[1], 'P');
  EXPECT_EQ(patch[2], 'S');
  EXPECT_EQ(patch[3], '1');

  std::vector<uint8_t> applied;
  auto apply_status = util::ApplyBpsPatch(source, patch, applied);
  ASSERT_TRUE(apply_status.ok()) << apply_status.message();
  EXPECT_EQ(applied, target);
}

TEST(BpsTest, ApplyRejectsInvalidHeader) {
  auto source = MakeSourceData(64);
  std::vector<uint8_t> bad_patch = {'N', 'O', 'P', 'E', 0x80};
  std::vector<uint8_t> output;

  auto status = util::ApplyBpsPatch(source, bad_patch, output);
  EXPECT_FALSE(status.ok());
}

TEST(BpsTest, ApplyRejectsCorruptedPatchCrc) {
  auto source = MakeSourceData(256);
  auto target = source;
  target[12] ^= 0xFF;
  target[25] ^= 0x80;

  std::vector<uint8_t> patch;
  ASSERT_TRUE(util::CreateBpsPatch(source, target, patch).ok());
  ASSERT_GT(patch.size(), 20u);

  patch[8] ^= 0x01;

  std::vector<uint8_t> output;
  auto status = util::ApplyBpsPatch(source, patch, output);
  EXPECT_FALSE(status.ok());
}

TEST(BpsTest, ApplyRejectsWrongSourceRom) {
  auto source = MakeSourceData(512);
  auto target = source;
  target[123] ^= 0xAA;

  std::vector<uint8_t> patch;
  ASSERT_TRUE(util::CreateBpsPatch(source, target, patch).ok());

  auto wrong_source = source;
  wrong_source[0] ^= 0x01;

  std::vector<uint8_t> output;
  auto status = util::ApplyBpsPatch(wrong_source, patch, output);
  EXPECT_FALSE(status.ok());
}

}  // namespace yaze::test

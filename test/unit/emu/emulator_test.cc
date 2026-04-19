#include "app/emu/emulator.h"

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "rom/rom.h"

namespace yaze::emu {
namespace {

constexpr size_t kLoRomHeaderOffset = 0x7FC0;
constexpr size_t kTestRomSize = 512 * 1024;
constexpr int kNativeSampleRate = 32040;
constexpr double kNtscFrameRate = 60.0988;
constexpr double kPalFrameRate = 50.007;

std::vector<uint8_t> MakeTestRomData() {
  std::vector<uint8_t> rom_data(kTestRomSize, 0x00);
  rom_data[kLoRomHeaderOffset + 0x17] = 9;  // 0x400 << 9 = 512 KiB
  rom_data[kLoRomHeaderOffset + 0x18] = 3;  // Small SRAM allocation
  return rom_data;
}

TEST(EmulatorTest, ReloadRuntimeRomRecomputesTimingConstantsFromRegion) {
  const auto rom_data = MakeTestRomData();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  Emulator emulator;
  emulator.snes().memory().set_pal_timing(false);
  ASSERT_TRUE(emulator.EnsureInitialized(&rom));

  EXPECT_NEAR(emulator.wanted_frames(),
              static_cast<float>(1.0 / kNtscFrameRate), 1e-6f);
  EXPECT_EQ(emulator.wanted_samples(),
            static_cast<int>(std::lround(kNativeSampleRate / kNtscFrameRate)));

  emulator.snes().memory().set_pal_timing(true);
  ASSERT_TRUE(emulator.ReloadRuntimeRom(rom.vector()).ok());

  EXPECT_NEAR(emulator.wanted_frames(), static_cast<float>(1.0 / kPalFrameRate),
              1e-6f);
  EXPECT_EQ(emulator.wanted_samples(),
            static_cast<int>(std::lround(kNativeSampleRate / kPalFrameRate)));
  EXPECT_TRUE(emulator.running());
}

}  // namespace
}  // namespace yaze::emu

#include "app/emu/emulator.h"

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "app/gfx/backend/null_renderer.h"
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

class TextureCountingRenderer : public gfx::NullRenderer {
 public:
  gfx::TextureHandle CreateTextureWithFormat(int width, int height,
                                             uint32_t format,
                                             int access) override {
    ++created;
    return &texture;
  }

  int created = 0;
  int texture = 0;
};

TEST(EmulatorTest, RunCreatesPpuTextureAfterRuntimeReload) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeTestRomData()).ok());
  TextureCountingRenderer renderer;
  Emulator emulator;
  emulator.set_renderer(&renderer);

  // Run Project Output reloads the runtime before the viewer ever ran.
  ASSERT_TRUE(emulator.ReloadRuntimeRom(rom.vector()).ok());
  ASSERT_TRUE(emulator.is_snes_initialized());
  ASSERT_EQ(emulator.ppu_texture(), nullptr);

  emulator.Run(&rom);
  EXPECT_EQ(emulator.ppu_texture(), &renderer.texture)
      << "the viewer needs a texture without being opened before Run";
  EXPECT_EQ(renderer.created, 1);

  ASSERT_TRUE(emulator.ReloadRuntimeRom(rom.vector()).ok());
  emulator.Run(&rom);
  EXPECT_EQ(emulator.ppu_texture(), &renderer.texture);
  EXPECT_EQ(renderer.created, 1) << "a later reload reuses the texture";
}

TEST(EmulatorTest, RunAfterRuntimeReloadKeepsEmulatingWithoutATexture) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeTestRomData()).ok());
  gfx::NullRenderer renderer;
  Emulator emulator;
  emulator.set_renderer(&renderer);
  ASSERT_TRUE(emulator.ReloadRuntimeRom(rom.vector()).ok());

  emulator.Run(&rom);
  // Long enough for the next call to emulate and present a frame.
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  emulator.Run(&rom);

  EXPECT_EQ(emulator.ppu_texture(), nullptr);
  EXPECT_TRUE(emulator.running());
  EXPECT_GT(emulator.snes().mutable_cycles(), 0u)
      << "frames ran and skipped the missing texture";
}

}  // namespace
}  // namespace yaze::emu

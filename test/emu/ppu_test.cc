#include "app/emu/video/ppu.h"

#include <gmock/gmock.h>

#include "app/emu/cpu/clock.h"
#include "app/emu/memory/memory.h"
#include "app/emu/memory/mock_memory.h"

namespace yaze {
namespace app {
namespace emu {

class MockPpu : public PpuInterface {
 public:
  MOCK_METHOD(void, Write, (uint16_t address, uint8_t data), (override));
  MOCK_METHOD(uint8_t, Read, (uint16_t address), (const, override));

  MOCK_METHOD(void, RenderFrame, (), (override));
  MOCK_METHOD(void, RenderScanline, (), (override));
  MOCK_METHOD(void, RenderBackground, (int layer), (override));
  MOCK_METHOD(void, RenderSprites, (), (override));

  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(void, Update, (double deltaTime), (override));
  MOCK_METHOD(void, UpdateClock, (double deltaTime), (override));
  MOCK_METHOD(void, UpdateInternalState, (int cycles), (override));

  MOCK_METHOD(const std::vector<uint8_t>&, GetFrameBuffer, (),
              (const, override));
  MOCK_METHOD(std::shared_ptr<gfx::Bitmap>, GetScreen, (), (const, override));

  MOCK_METHOD(void, UpdateModeSettings, (), (override));
  MOCK_METHOD(void, UpdateTileData, (), (override));
  MOCK_METHOD(void, UpdateTileMapData, (), (override));
  MOCK_METHOD(void, UpdatePaletteData, (), (override));

  MOCK_METHOD(void, ApplyEffects, (), (override));
  MOCK_METHOD(void, ComposeLayers, (), (override));

  MOCK_METHOD(void, DisplayFrameBuffer, (), (override));
  MOCK_METHOD(void, Notify, (uint32_t address, uint8_t data), (override));

  std::vector<uint8_t> internalFrameBuffer;
  std::vector<uint8_t> vram;
  std::vector<SpriteAttributes> sprites;
  std::vector<Tilemap> tilemaps;
  BackgroundMode bgMode;
};

class PpuTest : public ::testing::Test {
 protected:
  MockMemory mock_memory;
  MockClock mock_clock;
  MockPpu mock_ppu;

  PpuTest() {}

  void SetUp() override {
    ON_CALL(mock_ppu, Init()).WillByDefault([this]() {
      mock_ppu.internalFrameBuffer.resize(256 * 240);
      mock_ppu.vram.resize(0x10000);
    });

    ON_CALL(mock_ppu, Write(::testing::_, ::testing::_))
        .WillByDefault([this](uint16_t address, uint8_t data) {
          mock_ppu.vram[address] = data;
        });

    ON_CALL(mock_ppu, Read(::testing::_))
        .WillByDefault(
            [this](uint16_t address) { return mock_ppu.vram[address]; });

    ON_CALL(mock_ppu, RenderScanline()).WillByDefault([this]() {
      // Simulate scanline rendering logic...
    });

    ON_CALL(mock_ppu, GetFrameBuffer()).WillByDefault([this]() {
      return mock_ppu.internalFrameBuffer;
    });

    // Additional ON_CALL setups as needed...
  }

  void TearDown() override {
    // Common cleanup (if necessary)
  }

  const uint8_t testVRAMValue = 0xAB;
  const uint16_t testVRAMAddress = 0x2000;
  const std::vector<uint8_t> spriteData = {/* ... */};
  const std::vector<uint8_t> bgData = {/* ... */};
  const uint8_t testPaletteIndex = 3;
  const uint16_t testTileIndex = 42;
};

// Test Initialization
TEST_F(PpuTest, InitializationSetsCorrectFrameBufferSize) {
  // EXPECT_CALL(mock_ppu, Init()).Times(1);
  // mock_ppu.Init();
  // EXPECT_EQ(mock_ppu.GetFrameBuffer().size(), 256 * 240);
}

// Test State Reset
TEST_F(PpuTest, ResetClearsFrameBuffer) {
  // EXPECT_CALL(mock_ppu, Reset()).Times(1);
  // mock_ppu.Reset();
  // auto frameBuffer = mock_ppu.GetFrameBuffer();
  // EXPECT_TRUE(std::all_of(frameBuffer.begin(), frameBuffer.end(),
  //                         [](uint8_t val) { return val == 0; }));
}

// Test Memory Interaction
TEST_F(PpuTest, ReadWriteVRAM) {
  // uint16_t address = testVRAMAddress;
  // uint8_t value = testVRAMValue;
  // EXPECT_CALL(mock_ppu, Write(address, value)).Times(1);
  // mock_ppu.Write(address, value);
  // EXPECT_EQ(mock_ppu.Read(address), value);
}

// Test Rendering Mechanics
TEST_F(PpuTest, RenderScanlineUpdatesFrameBuffer) {
  // Setup PPU with necessary background and sprite data
  // Call RenderScanline and check if the framebuffer is updated correctly
}

// Test Mode and Register Handling
TEST_F(PpuTest, Mode0Rendering) {
  // Set PPU to Mode0 and verify correct rendering behavior
}

// Test Interrupts and Counters
TEST_F(PpuTest, VBlankInterruptTriggered) {
  // Simulate conditions for V-Blank and test if the interrupt is triggered
}

// Test Composite Rendering and Output
TEST_F(PpuTest, FrameComposition) {
  // Setup various layers and sprites, call ComposeLayers, and verify the frame
  // buffer
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
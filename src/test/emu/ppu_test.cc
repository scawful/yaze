#include "app/emu/video/ppu.h"

#include <gmock/gmock.h>

#include "app/emu/cpu/clock.h"
#include "app/emu/memory/memory.h"
#include "test/mocks/mock_memory.h"

namespace yaze {
namespace test {
namespace emu {

using yaze::emu::Clock;
using yaze::emu::memory::MockClock;
using yaze::emu::memory::MockMemory;
using yaze::emu::video::BackgroundMode;
using yaze::emu::video::PpuInterface;
using yaze::emu::video::SpriteAttributes;
using yaze::emu::video::Tilemap;
using yaze::gfx::Bitmap;

/**
 * @brief Mock Ppu class for testing
 */
class MockPpu : public PpuInterface {
 public:
  MOCK_METHOD(void, Write, (uint16_t address, uint8_t data), (override));
  MOCK_METHOD(uint8_t, Read, (uint16_t address), (const, override));

  std::vector<uint8_t> internalFrameBuffer;
  std::vector<uint8_t> vram;
  std::vector<SpriteAttributes> sprites;
  std::vector<Tilemap> tilemaps;
  BackgroundMode bgMode;
};

/**
 * \test Test fixture for PPU unit tests
 */
class PpuTest : public ::testing::Test {
 protected:
  MockMemory mock_memory;
  MockClock mock_clock;
  MockPpu mock_ppu;

  PpuTest() {}

  void SetUp() override {
    ON_CALL(mock_ppu, Write(::testing::_, ::testing::_))
        .WillByDefault([this](uint16_t address, uint8_t data) {
          mock_ppu.vram[address] = data;
        });

    ON_CALL(mock_ppu, Read(::testing::_))
        .WillByDefault(
            [this](uint16_t address) { return mock_ppu.vram[address]; });
  }
};

}  // namespace emu
}  // namespace test
}  // namespace yaze
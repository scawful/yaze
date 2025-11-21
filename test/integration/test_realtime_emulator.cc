#include "gtest/gtest.h"
#include "app/emulator.h"
#include "app/rom.h"
#include "testing.h"

using namespace yaze;

namespace {

class RealTimeEmulatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up a dummy ROM for testing
    std::vector<uint8_t> rom_data(0x8000, 0); // 32KB ROM filled with zeros
    rom_ = std::make_shared<Rom>(rom_data);
    emulator_ = std::make_unique<Emulator>(rom_);
  }

  std::shared_ptr<Rom> rom_;
  std::unique_ptr<Emulator> emulator_;
};

TEST_F(RealTimeEmulatorTest, TestPatchByte) {
  // TODO: This is where GEMINI_FLASH_AUTOM will add the test assertions.
  // 1. Read a byte from a specific ROM offset in the emulator's memory.
  // 2. Call PatchROMByte to change that byte to a new value.
  // 3. Read the byte again and assert that it has been changed to the new value.
}

} // namespace

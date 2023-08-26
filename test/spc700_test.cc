#include "app/emu/audio/spc700.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace app {
namespace emu {

class MockAudioRAM : public AudioRam {
 public:
  MOCK_METHOD(uint8_t, read, (uint16_t address), (const, override));
  MOCK_METHOD(void, write, (uint16_t address, uint8_t value), (override));
};

class SPC700Test : public ::testing::Test {
 protected:
  SPC700Test() = default;

  MockAudioRAM audioRAM;
  SPC700 spc700{audioRAM};
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

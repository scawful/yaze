#include "app/emu/ppu.h"

#include <gmock/gmock.h>

namespace yaze {
namespace app {
namespace emu {

class MockPPU : public IPPU {
 public:
  MOCK_METHOD(void, writeRegister, (uint16_t address, uint8_t data),
              (override));
  MOCK_METHOD(uint8_t, readRegister, (uint16_t address), (const, override));
  MOCK_METHOD(void, setOAMData, (const std::vector<uint8_t>& data), (override));
  MOCK_METHOD(std::vector<uint8_t>, getOAMData, (), (const, override));
  MOCK_METHOD(void, setVRAMData, (const std::vector<uint8_t>& data),
              (override));
  MOCK_METHOD(std::vector<uint8_t>, getVRAMData, (), (const, override));
  MOCK_METHOD(void, setCGRAMData, (const std::vector<uint8_t>& data),
              (override));
  MOCK_METHOD(std::vector<uint8_t>, getCGRAMData, (), (const, override));
  MOCK_METHOD(void, renderFrame, (), (override));
  MOCK_METHOD(std::vector<uint32_t>, getFrameBuffer, (), (const, override));
};

}  // namespace emu
}  // namespace app
}  // namespace yaze